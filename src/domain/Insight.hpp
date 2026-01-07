/**
 * @file Insight.hpp
 * @brief Domain entity representing a processed note with metadata and actionables.
 */

#pragma once
#include <string>
#include <vector>
#include <chrono>
#include <sstream>
#include "Actionable.hpp"

namespace ideawalker::domain {

/**
 * @class Insight
 * @brief Represents a structured thought, containing metadata, full content, and extracted tasks.
 */
class Insight {
public:
    /**
     * @struct Metadata
     * @brief Essential metadata for an insight.
     */
    struct Metadata {
        std::string id; ///< Unique identifier (usually filename).
        std::string title; ///< Human-readable title.
        std::string date; ///< Creation/modification date.
        std::vector<std::string> tags; ///< Categorization tags.
    };

    /**
     * @brief Constructor for Insight.
     * @param meta Initial metadata.
     * @param content Full text content.
     */
    Insight(const Metadata& meta, const std::string& content)
        : m_metadata(meta), m_content(content) {}

    /** @brief Returns insight metadata. */
    const Metadata& getMetadata() const { return m_metadata; }
    
    /** @brief Returns full text content. */
    const std::string& getContent() const { return m_content; }
    
    /** @brief Returns list of parsed actionables. */
    const std::vector<Actionable>& getActionables() const { return m_actionables; }

    /**
     * @brief Filters tasks by their completion status.
     * @param completed Status to filter by.
     * @return List of matching tasks.
     */
    std::vector<Actionable> getTasksByStatus(bool completed) const {
        std::vector<Actionable> filtered;
        for (const auto& task : m_actionables) {
            if (task.isCompleted == completed) {
                filtered.push_back(task);
            }
        }
        return filtered;
    }

    /** @brief Manually adds an actionable to the insight. */
    void addActionable(const Actionable& actionable) {
        m_actionables.push_back(actionable);
    }

    /**
     * @brief Parses markdown task lines (- [ ], - [x], - [/]) from the content.
     */
    void parseActionablesFromContent() {
        m_actionables.clear();
        std::stringstream ss(m_content);
        std::string line;
        while (std::getline(ss, line)) {
            size_t pos_unchecked = line.find("- [ ]");
            size_t pos_checked = line.find("- [x]");
            size_t pos_progress = line.find("- [/]");
            
            if (pos_unchecked != std::string::npos || pos_checked != std::string::npos || pos_progress != std::string::npos) {
                bool completed = (pos_checked != std::string::npos);
                bool inProgress = (pos_progress != std::string::npos);
                size_t pos = std::string::npos;
                if (pos_unchecked != std::string::npos) pos = pos_unchecked;
                else if (pos_checked != std::string::npos) pos = pos_checked;
                else if (pos_progress != std::string::npos) pos = pos_progress;

                std::string desc = line.substr(pos + 5);
                desc.erase(0, desc.find_first_not_of(" \t")); // Trim
                if (!desc.empty()) {
                    m_actionables.emplace_back(desc, completed, inProgress);
                }
            }
        }
    }

    /** @brief Updates the full text content. */
    void setContent(const std::string& content) { m_content = content; }

    /**
     * @brief Toggles the status of a task at a given index (cycle: Todo -> InProgress -> Done -> Todo).
     * @param index 0-based index of the task.
     */
    void toggleActionable(size_t index) {
        if (index >= m_actionables.size()) return;

        std::stringstream ss(m_content);
        std::stringstream out;
        std::string line;
        size_t currentIdx = 0;
        
        while (std::getline(ss, line)) {
            size_t pos_todo = line.find("- [ ]");
            size_t pos_done = line.find("- [x]");
            size_t pos_prog = line.find("- [/]");

            if (pos_todo != std::string::npos || pos_done != std::string::npos || pos_prog != std::string::npos) {
                if (currentIdx == index) {
                    if (pos_todo != std::string::npos) {
                        line.replace(pos_todo, 5, "- [/]");
                        m_actionables[index].isInProgress = true;
                        m_actionables[index].isCompleted = false;
                    } else if (pos_prog != std::string::npos) {
                        line.replace(pos_prog, 5, "- [x]");
                        m_actionables[index].isInProgress = false;
                        m_actionables[index].isCompleted = true;
                    } else if (pos_done != std::string::npos) {
                        line.replace(pos_done, 5, "- [ ]");
                        m_actionables[index].isInProgress = false;
                        m_actionables[index].isCompleted = false;
                    }
                }
                currentIdx++;
            }
            out << line << "\n";
        }
        m_content = out.str();
    }

    /**
     * @brief Explicitly sets the status of a task at a given index.
     * @param index 0-based index.
     * @param completed Final completed status.
     * @param inProgress Final in-progress status.
     */
    void setActionableStatus(size_t index, bool completed, bool inProgress) {
        if (index >= m_actionables.size()) return;

        std::stringstream ss(m_content);
        std::stringstream out;
        std::string line;
        size_t currentIdx = 0;
        
        while (std::getline(ss, line)) {
            size_t pos_todo = line.find("- [ ]");
            size_t pos_done = line.find("- [x]");
            size_t pos_prog = line.find("- [/]");

            if (pos_todo != std::string::npos || pos_done != std::string::npos || pos_prog != std::string::npos) {
                if (currentIdx == index) {
                    size_t pos = (pos_todo != std::string::npos) ? pos_todo : 
                                 ((pos_done != std::string::npos) ? pos_done : pos_prog);
                    
                    std::string newState = "- [ ]";
                    if (completed) newState = "- [x]";
                    else if (inProgress) newState = "- [/]";
                    
                    line.replace(pos, 5, newState);
                    m_actionables[index].isCompleted = completed;
                    m_actionables[index].isInProgress = inProgress;
                }
                currentIdx++;
            }
            out << line << "\n";
        }
        m_content = out.str();
    }

private:
    Metadata m_metadata; ///< Insight metadata.
    std::string m_content; ///< Full text content.
    std::vector<Actionable> m_actionables; ///< List of tasks extracted from content.
};

} // namespace ideawalker::domain
