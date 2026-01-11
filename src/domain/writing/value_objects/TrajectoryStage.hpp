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

} // namespace ideawalker::domain::writing
