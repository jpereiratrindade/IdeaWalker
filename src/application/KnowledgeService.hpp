/**
 * @file KnowledgeService.hpp
 * @brief Service for managing notes, insights, and task state.
 */

#pragma once

#include "domain/ThoughtRepository.hpp"
#include "domain/Insight.hpp"
#include <memory>
#include <string>
#include <vector>
#include <map>

namespace ideawalker::application {

/**
 * @class KnowledgeService
 * @brief Manages the lifecycle and state of knowledge artifacts.
 */
class KnowledgeService {
public:
    explicit KnowledgeService(std::unique_ptr<domain::ThoughtRepository> repo);

    /** @brief Updates the content of a specific note. */
    void UpdateNote(const std::string& filename, const std::string& content);

    /** @brief Toggles the status of a task in a note. */
    void ToggleTask(const std::string& filename, int index);

    /** @brief Sets specific task status. */
    void SetTaskStatus(const std::string& filename, int index, bool completed, bool inProgress);

    /** @brief Returns all processed insights with parsed tasks. */
    std::vector<domain::Insight> GetAllInsights();

    /** @brief Returns raw items from the inbox. */
    std::vector<domain::RawThought> GetRawThoughts();

    /** @brief Returns activity statistics. */
    std::map<std::string, int> GetActivityHistory();

    /** @brief Returns files linking to the target. */
    std::vector<std::string> GetBacklinks(const std::string& filename);

    /** @brief Returns history versions for a note. */
    std::vector<std::string> GetNoteHistory(const std::string& noteId);

    /** @brief Returns content of a specific version. */
    std::string GetVersionContent(const std::string& versionFilename);

    /** @brief Returns content of a specific note. */
    std::string GetNoteContent(const std::string& filename);

    /** @brief Provides access to the repository (legacy/internal use). */
    domain::ThoughtRepository& GetRepository() { return *m_repo; }

private:
    std::unique_ptr<domain::ThoughtRepository> m_repo;
};

} // namespace ideawalker::application
