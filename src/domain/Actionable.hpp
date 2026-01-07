/**
 * @file Actionable.hpp
 * @brief Domain entity representing a task or actionable item.
 */

#pragma once
#include <string>

namespace ideawalker::domain {

/**
 * @struct Actionable
 * @brief Represents a single task derived from a thought or note.
 */
struct Actionable {
    std::string description; ///< Description of the task.
    bool isCompleted = false; ///< True if the task is finished.
    bool isInProgress = false; ///< True if the task is currently being worked on.

    /**
     * @brief Constructor for Actionable.
     * @param desc Task description.
     * @param completed Initial completed status.
     * @param inProgress Initial in-progress status.
     */
    Actionable(const std::string& desc, bool completed = false, bool inProgress = false) 
        : description(desc), isCompleted(completed), isInProgress(inProgress) {}
};

} // namespace ideawalker::domain
