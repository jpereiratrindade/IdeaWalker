/**
 * @file WritingTrajectory.hpp
 * @brief Aggregate Root for a writing project.
 */

#pragma once

#include <string>
#include <vector>
#include <map>
#include <memory>
#include <variant>
#include <stdexcept>

#include "value_objects/WritingIntent.hpp"
#include "value_objects/TrajectoryStage.hpp"
#include "entities/DraftSegment.hpp"
#include "entities/RevisionDecision.hpp"
#include "entities/DefenseCard.hpp"
#include "events/WritingEvents.hpp"

namespace ideawalker::domain::writing {

class WritingTrajectory {
private:
    std::string trajectoryId;
    WritingIntent intent;
    TrajectoryStage stage = TrajectoryStage::Intent;
    
    // Map segmentId -> Segment
    std::map<std::string, DraftSegment> segments;
    
    // Ordered list of revisions (history)
    std::vector<RevisionDecision> revisionHistory;
    
    // Defense Cards
    std::vector<DefenseCard> defenseCards;


    // Uncommitted events (new changes)
    std::vector<WritingDomainEvent> uncommittedEvents;

public:
    WritingTrajectory(std::string id, WritingIntent i)
        : trajectoryId(std::move(id)), intent(std::move(i)) {
        
        TrajectoryCreated evt{
            trajectoryId,
            intent,
            std::chrono::system_clock::now()
        };
        // Apply is strictly for rehydration/state change, but constructor handles initial state.
        // We just record the event.
        uncommittedEvents.push_back(evt);
    }
    
    // --- Event Management ---
    const std::vector<WritingDomainEvent>& getUncommittedEvents() const {
        return uncommittedEvents;
    }
    
    void clearUncommittedEvents() {
        uncommittedEvents.clear();
    }

    // --- Accessors ---
    const std::string& getId() const { return trajectoryId; }
    const WritingIntent& getIntent() const { return intent; }
    TrajectoryStage getStage() const { return stage; }
    const std::map<std::string, DraftSegment>& getSegments() const { return segments; }
    const std::vector<RevisionDecision>& getHistory() const { return revisionHistory; }

    // --- Commands (State Mutators) ---

    // Adds a new segment.
    void addSegment(const std::string& title, const std::string& content, SourceTag source = SourceTag::Human) {
        // Generate ID (in real app, use UUID generator)
        std::string segId = trajectoryId + "-seg-" + std::to_string(segments.size() + 1);
        
        segments.emplace(std::piecewise_construct,
            std::forward_as_tuple(segId),
            std::forward_as_tuple(segId, title, content, source)
        );

        SegmentAdded evt{
            trajectoryId,
            segId,
            title,
            content,
            SourceTagToString(source),
            std::chrono::system_clock::now()
        };
        uncommittedEvents.push_back(evt);
    }

    // Revises an existing segment.
    void reviseSegment(const std::string& segmentId, 
                               const std::string& newContent, 
                               RevisionOperation op, 
                               const std::string& rationale,
                               SourceTag source) {
        auto it = segments.find(segmentId);
        if (it == segments.end()) {
            throw std::runtime_error("Segment not found: " + segmentId);
        }

        std::string oldContent = it->second.content;
        
        // Update segment state
        it->second.update(newContent, source);

        // Record decision
        // decisionId generation
        std::string decisionId = trajectoryId + "-dec-" + std::to_string(revisionHistory.size() + 1);
        RevisionDecision decision(decisionId, segmentId, op, rationale);
        revisionHistory.push_back(decision);

        SegmentRevised evt{
            trajectoryId,
            segmentId,
            oldContent,
            newContent,
            decisionId,
            op,
            rationale,
            SourceTagToString(source),
            std::chrono::system_clock::now()
        };
        uncommittedEvents.push_back(evt);
    }
    
    // Advances stage
    void advanceStage(TrajectoryStage targetStage) {
        if (stage == TrajectoryStage::Final) {
            throw std::invalid_argument("Cannot advance stage from Final.");
        }
        if (!IsNextStage(stage, targetStage)) {
            throw std::invalid_argument("Invalid stage transition.");
        }
        if (stage == TrajectoryStage::Intent && !intent.isValid()) {
            throw std::invalid_argument("Cannot advance stage without a valid intent.");
        }

        TrajectoryStage old = stage;
        stage = targetStage;
        
        StageAdvanced evt{
            trajectoryId,
            old,
            targetStage,
            std::chrono::system_clock::now()
        };
        uncommittedEvents.push_back(evt);
    }



    // --- Defense Commands ---
    void addDefenseCard(const std::string& cardId, const std::string& segmentId, const std::string& prompt, const std::vector<std::string>& points) {
        DefenseCard card(cardId, segmentId, prompt);
        card.expectedDefensePoints = points;
        defenseCards.push_back(card);

        DefenseCardAdded evt;
        evt.trajectoryId = trajectoryId;
        evt.cardId = cardId;
        evt.segmentId = segmentId;
        evt.prompt = prompt;
        evt.expectedPoints = points;
        evt.timestamp = std::chrono::system_clock::now();
        uncommittedEvents.push_back(evt);
    }

    void updateDefenseStatus(const std::string& cardId, DefenseStatus newStatus, const std::string& response) {
        for (auto& card : defenseCards) {
            if (card.cardId == cardId) {
                if (newStatus == DefenseStatus::Rehearsed) card.markRehearsed(response);
                else if (newStatus == DefenseStatus::Passed) card.markPassed();
                
                DefenseStatusUpdated evt;
                evt.trajectoryId = trajectoryId;
                evt.cardId = cardId;
                evt.newStatus = (newStatus == DefenseStatus::Passed) ? "Passed" : "Rehearsed";
                evt.response = response;
                evt.timestamp = std::chrono::system_clock::now();
                uncommittedEvents.push_back(evt);
                break;
            }
        }
    }
    
    const std::vector<DefenseCard>& getDefenseCards() const { return defenseCards; }

    // --- Rehydration (Apply Events) ---
    // Used when loading from Event Store
    
    // Factory method to create empty instance for rehydration
    static WritingTrajectory createEmpty(std::string id) {
        // Dummy intent, will be overwritten by TrajectoryCreated event
        WritingTrajectory t(std::move(id), WritingIntent());
        t.clearUncommittedEvents(); // CRITICAL: Remove the event generated by constructor
        return t;
    }
    
    // Helper to apply any event variant
    void applyEvent(const WritingDomainEvent& event) {
        std::visit([this](auto&& arg) {
            this->apply(arg);
        }, event);
    }
    
    void apply(const TrajectoryCreated& e) {
        trajectoryId = e.trajectoryId;
        intent = e.intent;
        stage = TrajectoryStage::Intent; 
    }

    void apply(const SegmentAdded& e) {
        SourceTag tag = SourceTag::Human; 
        if (e.sourceTag == "ai_generated") tag = SourceTag::AiGenerated;
        if (e.sourceTag == "ai_assisted") tag = SourceTag::AiAssisted;
        if (e.sourceTag == "human_reviewed") tag = SourceTag::HumanReviewed;

        segments.emplace(std::piecewise_construct,
            std::forward_as_tuple(e.segmentId),
            std::forward_as_tuple(e.segmentId, e.title, e.content, tag)
        );
    }

    void apply(const SegmentRevised& e) {
        auto it = segments.find(e.segmentId);
        if (it != segments.end()) {
             SourceTag tag = SourceTagFromString(e.sourceTag);
             it->second.update(e.newContent, tag);
             
             RevisionDecision dec(e.decisionId, e.segmentId, e.operation, e.rationale);
             revisionHistory.push_back(dec);
        }
    }
    
    void apply(const StageAdvanced& e) {
        stage = e.newStage;
    }
    
    void apply(const DefenseCardAdded& e) {
        DefenseCard card(e.cardId, e.segmentId, e.prompt);
        card.expectedDefensePoints = e.expectedPoints;
        defenseCards.push_back(card);
    }

    void apply(const DefenseStatusUpdated& e) {
        for (auto& card : defenseCards) {
            if (card.cardId == e.cardId) {
                if (e.newStatus == "Rehearsed") card.markRehearsed(e.response);
                else if (e.newStatus == "Passed") card.markPassed();
                break;
            }
        }
    }

private:
   // Private constructor for rehydration if needed, but public generic one works with createEmpty
   WritingTrajectory() = default; 

};

} // namespace ideawalker::domain::writing
