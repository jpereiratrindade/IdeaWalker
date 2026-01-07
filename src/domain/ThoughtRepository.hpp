/**
 * @file ThoughtRepository.hpp
 * @brief Interface for persistence and retrieval of thoughts and insights.
 */

#pragma once
#include <vector>
#include <string>
#include <memory>
#include <map>
#include "Insight.hpp"

namespace ideawalker::domain {

/**
 * @struct RawThought
 * @brief Represents a raw thought (e.g., an unorganized markdown file or a draft).
 */
struct RawThought {
    std::string filename; ///< Source filename.
    std::string content; ///< Raw text content.
};

/**
 * @class ThoughtRepository
 * @brief Abstract interface for persistent storage of project data.
 */
class ThoughtRepository {
public:
    virtual ~ThoughtRepository() = default;

    /** @brief Fetches all raw thoughts from the inbox area. */
    virtual std::vector<RawThought> fetchInbox() = 0;

    /**
     * @brief Checks if a thought has changed and needs re-processing.
     * @param thought The raw thought to check.
     * @param insightId Target insight identifier.
     * @return True if processing is required.
     */
    virtual bool shouldProcess(const RawThought& thought, const std::string& insightId) = 0;

    /** @brief Saves a processed insight to the history/knowledge base area. */
    virtual void saveInsight(const Insight& insight) = 0;

    /**
     * @brief Updates the content of a specific note file.
     * @param filename Target file.
     * @param content New content.
     */
    virtual void updateNote(const std::string& filename, const std::string& content) = 0;

    /** @brief Fetches all insights from the history. */
    virtual std::vector<Insight> fetchHistory() = 0;

    /**
     * @brief Identifies all files that reference the specified file.
     * @param filename Target file.
     * @return List of filenames.
     */
    virtual std::vector<std::string> getBacklinks(const std::string& filename) = 0;

    /**
     * @brief Retrieves activity data for visualization.
     * @return Map of date string to activity count.
     */
    virtual std::map<std::string, int> getActivityHistory() = 0;

    /**
     * @brief Retrieves a list of available versions for a specific note.
     * @param noteId The ID of the note (filename without extension usually).
     * @return List of version filenames (e.g., "Nota_ID_Timestamp.md").
     */
    virtual std::vector<std::string> getVersions(const std::string& noteId) = 0;

    /**
     * @brief Retrieves the content of a specific version.
     * @param versionFilename The filename of the version to load.
     * @return The raw content of the file.
     */
    virtual std::string getVersionContent(const std::string& versionFilename) = 0;
};

} // namespace ideawalker::domain
