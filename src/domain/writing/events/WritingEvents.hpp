/**
 * @file WritingEvents.hpp
 * @brief Domain Events definitions for the Writing Context.
 */

#pragma once

#include <string>
#include <chrono>
#include <variant>
#include "../value_objects/TrajectoryStage.hpp"
#include "../value_objects/WritingIntent.hpp"
#include "../entities/RevisionDecision.hpp"

namespace ideawalker::domain::writing {

struct TrajectoryCreated {
    static constexpr const char* Type = "TrajectoryCreated";
    std::string trajectoryId;
    WritingIntent intent;
    std::chrono::system_clock::time_point timestamp;
};

struct SegmentAdded {
    static constexpr const char* Type = "SegmentAdded";
    std::string trajectoryId;
    std::string segmentId;
    std::string title;
    std::string content;
    std::string sourceTag;
    std::chrono::system_clock::time_point timestamp;
};

struct SegmentRevised {
    static constexpr const char* Type = "SegmentRevised";
    std::string trajectoryId;
    std::string segmentId;
    std::string oldContent;
    std::string newContent;
    std::string decisionId;
    RevisionOperation operation;
    std::string rationale;
    std::string sourceTag;
    std::chrono::system_clock::time_point timestamp;
};

struct StageAdvanced {
    static constexpr const char* Type = "StageAdvanced";
    std::string trajectoryId;
    TrajectoryStage oldStage;
    TrajectoryStage newStage;
    std::chrono::system_clock::time_point timestamp;
};

// Defense Events
struct DefenseCardAdded {
    static constexpr const char* Type = "DefenseCardAdded";
    std::string trajectoryId;
    std::string cardId;
    std::string segmentId;
    std::string prompt;
    std::vector<std::string> expectedPoints;
    std::chrono::system_clock::time_point timestamp;
};

struct DefenseStatusUpdated {
    static constexpr const char* Type = "DefenseStatusUpdated";
    std::string trajectoryId;
    std::string cardId;
    std::string newStatus; // "Pending", "Rehearsed", "Passed"
    std::string response; 
    std::chrono::system_clock::time_point timestamp;
};

// variant for generic handling
using WritingDomainEvent = std::variant<
    TrajectoryCreated, 
    SegmentAdded, 
    SegmentRevised, 
    StageAdvanced,
    DefenseCardAdded,
    DefenseStatusUpdated
>;

} // namespace ideawalker::domain::writing
