/**
 * @file WritingTrajectoryRepositoryFs.cpp
 * @brief Implementation of WritingTrajectoryRepositoryFs.
 */

#include "WritingTrajectoryRepositoryFs.hpp"
#include <nlohmann/json.hpp>
#include <iostream>

namespace ideawalker::infrastructure::writing {

using json = nlohmann::json;

WritingTrajectoryRepositoryFs::WritingTrajectoryRepositoryFs(std::unique_ptr<WritingEventStoreFs> eventStore)
    : m_eventStore(std::move(eventStore)) {}

// Serialization helper
std::vector<StoredEvent> WritingTrajectoryRepositoryFs::serializeEvents(const std::string& trajectoryId, 
                                       const std::vector<WritingDomainEvent>& domainEvents) {
    std::vector<StoredEvent> stored;
    for (const auto& varEvent : domainEvents) {
        std::visit([&](auto&& e) {
            using T = std::decay_t<decltype(e)>;
            json j;
            // Common serialization logic
            // Since we don't have reflection, manual JSON mapping here
            
            if constexpr (std::is_same_v<T, TrajectoryCreated>) {
                j["intent"] = {
                    {"purpose", e.intent.purpose},
                    {"audience", e.intent.audience},
                    {"coreClaim", e.intent.coreClaim},
                    {"constraints", e.intent.constraints}
                };
            }
            else if constexpr (std::is_same_v<T, SegmentAdded>) {
                j = {
                    {"segmentId", e.segmentId},
                    {"title", e.title},
                    {"content", e.content},
                    {"sourceTag", e.sourceTag}
                };
            }
            else if constexpr (std::is_same_v<T, SegmentRevised>) {
                j = {
                    {"segmentId", e.segmentId},
                    {"oldContent", e.oldContent},
                    {"newContent", e.newContent},
                    {"decisionId", e.decisionId},
                    {"operation", OperationToString(e.operation)},
                    {"rationale", e.rationale},
                    {"sourceTag", e.sourceTag}
                };
            }
            else if constexpr (std::is_same_v<T, StageAdvanced>) {
                j = {
                    {"oldStage", StageToString(e.oldStage)},
                    {"newStage", StageToString(e.newStage)}
                };
            }
            else if constexpr (std::is_same_v<T, DefenseCardAdded>) {
                j = {
                    {"cardId", e.cardId},
                    {"segmentId", e.segmentId},
                    {"prompt", e.prompt},
                    {"expectedPoints", e.expectedPoints}
                };
            }
            else if constexpr (std::is_same_v<T, DefenseStatusUpdated>) {
                j = {
                    {"cardId", e.cardId},
                    {"newStatus", e.newStatus},
                    {"response", e.response}
                };
            }
            
            stored.push_back({T::Type, j.dump(), e.timestamp});
        }, varEvent);
    }
    return stored;
}

void WritingTrajectoryRepositoryFs::save(const WritingTrajectory& trajectory) {
    // In save, we assume it's new, but logic is same as update: save uncommitted events.
    auto storedEvents = serializeEvents(trajectory.getId(), trajectory.getUncommittedEvents());
    m_eventStore->append(trajectory.getId(), storedEvents);
    // Note: We can't clear uncommitted events on the const object passed here.
    // The interface method signature `save(const ...)` implies we don't modify the object.
    // However, clean Aggregate practice usually requires clearing events after save.
    // For now, we just save. The object in memory will still have events, but if it dies, it's fine.
    // If it lives, we should clear them.
}

void WritingTrajectoryRepositoryFs::update(WritingTrajectory& trajectory) {
    auto storedEvents = serializeEvents(trajectory.getId(), trajectory.getUncommittedEvents());
    m_eventStore->append(trajectory.getId(), storedEvents);
    trajectory.clearUncommittedEvents();
}

std::optional<WritingTrajectory> WritingTrajectoryRepositoryFs::findById(const std::string& id) {
    auto stored = m_eventStore->readAll(id);
    if (stored.empty()) return std::nullopt;

    // Rehydrate
    auto trajectory = WritingTrajectory::createEmpty(id);
    
    try {
        for (const auto& s : stored) {
            if (s.eventType == TrajectoryCreated::Type) {
                auto j = json::parse(s.eventDataJson);
                WritingIntent intent(
                    j["intent"]["purpose"],
                    j["intent"]["audience"],
                    j["intent"].value("coreClaim", ""),
                    j["intent"].value("constraints", "")
                );
                trajectory.applyEvent(TrajectoryCreated{id, intent, s.timestamp});
            }
            else if (s.eventType == SegmentAdded::Type) {
                auto j = json::parse(s.eventDataJson);
                trajectory.applyEvent(SegmentAdded{
                    id, 
                    j["segmentId"], 
                    j["title"], 
                    j["content"], 
                    j.value("sourceTag", "human"), 
                    s.timestamp
                });
            }
            else if (s.eventType == SegmentRevised::Type) {
                auto j = json::parse(s.eventDataJson);
                RevisionOperation op = RevisionOperation::Clarify; // Default
                std::string opStr = j["operation"];
                if (opStr == "clarify") op = RevisionOperation::Clarify;
                else if (opStr == "compress") op = RevisionOperation::Compress;
                else if (opStr == "expand") op = RevisionOperation::Expand;
                else if (opStr == "reorganize") op = RevisionOperation::Reorganize;
                else if (opStr == "cite") op = RevisionOperation::Cite;
                else if (opStr == "remove") op = RevisionOperation::Remove;
                else if (opStr == "reframe") op = RevisionOperation::Reframe;
                else if (opStr == "correction") op = RevisionOperation::Correction;

                trajectory.applyEvent(SegmentRevised{
                    id,
                    j["segmentId"],
                    j["oldContent"],
                    j["newContent"],
                    j["decisionId"],
                    op,
                    j["rationale"],
                    j.value("sourceTag", "human"),
                    s.timestamp
                });
            }
            else if (s.eventType == DefenseCardAdded::Type) {
                auto j = json::parse(s.eventDataJson);
                trajectory.applyEvent(DefenseCardAdded{
                    id,
                    j["cardId"],
                    j["segmentId"],
                    j["prompt"],
                    j.value("expectedPoints", std::vector<std::string>{}),
                    s.timestamp
                });
            }
            else if (s.eventType == DefenseStatusUpdated::Type) {
                auto j = json::parse(s.eventDataJson);
                trajectory.applyEvent(DefenseStatusUpdated{
                    id,
                    j["cardId"],
                    j["newStatus"],
                    j.value("response", ""),
                    s.timestamp
                });
            }
            else if (s.eventType == StageAdvanced::Type) {
                auto j = json::parse(s.eventDataJson);
                // We need to parse string back to enum. 
                // A helper would be better, but for now simple comparison:
                auto stageFromString = [](const std::string& str) {
                    if (str == "Outline") return TrajectoryStage::Outline;
                    if (str == "Drafting") return TrajectoryStage::Drafting;
                    if (str == "Revising") return TrajectoryStage::Revising;
                    if (str == "Consolidating") return TrajectoryStage::Consolidating;
                    if (str == "ReadyForDefense") return TrajectoryStage::ReadyForDefense;
                    if (str == "Final") return TrajectoryStage::Final;
                    return TrajectoryStage::Intent;
                };

                trajectory.applyEvent(StageAdvanced{
                    id,
                    stageFromString(j["oldStage"]),
                    stageFromString(j["newStage"]),
                    s.timestamp
                });
            }
        }
    } catch (const std::exception& e) {
        std::cerr << "Error rehydrating trajectory " << id << ": " << e.what() << std::endl;
        return std::nullopt;
    }
    
    return trajectory;
}

std::vector<WritingTrajectory> WritingTrajectoryRepositoryFs::findAll() {
    std::vector<WritingTrajectory> trajectories;
    auto ids = m_eventStore->getAllTrajectoryIds();
    
    for (const auto& id : ids) {
        auto trajOpt = findById(id);
        if (trajOpt) {
            trajectories.push_back(std::move(*trajOpt));
        }
    }
    
    // Sort by timestamp? Or allow service to sort. 
    // Repo returns unsorted collection usually.
    return trajectories; 
}

} // namespace ideawalker::infrastructure::writing
