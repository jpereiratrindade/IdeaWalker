/**
 * @file TrajectoryStage.hpp
 * @brief Value Object defining the lifecycle stages of a writing trajectory.
 */

#pragma once

#include <string>
#include <vector>

namespace ideawalker::domain::writing {

/**
 * @enum TrajectoryStage
 * @brief Represents the current maturity stage of the writing project.
 */
enum class TrajectoryStage {
    Intent,             ///< Initial idea, defining purpose and audience.
    Outline,            ///< Structuring the argument flow.
    Drafting,           ///< Generating raw content.
    Revising,           ///< Refining, compressing, and clarifying.
    Consolidating,      ///< Merging segments, checking coherence.
    ReadyForDefense,    ///< Prepared for critique/review.
    Final               ///< Published or submitted state.
};

/**
 * @brief Helper to convert stage to string for display/logging.
 */
inline std::string StageToString(TrajectoryStage stage) {
    switch (stage) {
        case TrajectoryStage::Intent: return "Intent";
        case TrajectoryStage::Outline: return "Outline";
        case TrajectoryStage::Drafting: return "Drafting";
        case TrajectoryStage::Revising: return "Revising";
        case TrajectoryStage::Consolidating: return "Consolidating";
        case TrajectoryStage::ReadyForDefense: return "ReadyForDefense";
        case TrajectoryStage::Final: return "Final";
        default: return "Unknown";
    }
}

/**
 * @brief Returns the next stage in the canonical sequence.
 */
inline TrajectoryStage NextStage(TrajectoryStage stage) {
    switch (stage) {
        case TrajectoryStage::Intent: return TrajectoryStage::Outline;
        case TrajectoryStage::Outline: return TrajectoryStage::Drafting;
        case TrajectoryStage::Drafting: return TrajectoryStage::Revising;
        case TrajectoryStage::Revising: return TrajectoryStage::Consolidating;
        case TrajectoryStage::Consolidating: return TrajectoryStage::ReadyForDefense;
        case TrajectoryStage::ReadyForDefense: return TrajectoryStage::Final;
        case TrajectoryStage::Final: return TrajectoryStage::Final;
        default: return TrajectoryStage::Final;
    }
}

/**
 * @brief Checks if a target stage is the next valid stage.
 */
inline bool IsNextStage(TrajectoryStage current, TrajectoryStage target) {
    return target == NextStage(current);
}

} // namespace ideawalker::domain::writing
