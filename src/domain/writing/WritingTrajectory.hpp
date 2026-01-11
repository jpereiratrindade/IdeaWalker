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

#include "value_objects/WritingIntent.hpp"
#include "value_objects/TrajectoryStage.hpp"
#include "entities/DraftSegment.hpp"
#include "entities/RevisionDecision.hpp"
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

    // --- Rehydration (Apply Events) ---
    // Used when loading from Event Store
    
    // Factory method to create empty instance for rehydration
    static WritingTrajectory createEmpty(std::string id) {
        // Dummy intent, will be overwritten by TrajectoryCreated event
        return WritingTrajectory(std::move(id), WritingIntent());
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
             SourceTag tag = SourceTag::Human;
             // Parsing simplified
             
             it->second.update(e.newContent, tag);
             
             RevisionDecision dec(e.decisionId, e.segmentId, e.operation, e.rationale);
             revisionHistory.push_back(dec);
        }
    }
    
    void apply(const StageAdvanced& e) {
        stage = e.newStage;
    }

private:
   // Private constructor for rehydration if needed, but public generic one works with createEmpty
   WritingTrajectory() = default; 

};

} // namespace ideawalker::domain::writing
