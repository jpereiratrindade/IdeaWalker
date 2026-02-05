/**
 * @file FileRepository.hpp
 * @brief Filesystem-based implementation of the ThoughtRepository.
 */

#pragma once
#include "domain/ThoughtRepository.hpp"
#include <string>

namespace ideawalker::infrastructure {

/**
 * @class FileRepository
 * @brief Manages the storage of thoughts and insights on the local filesystem.
 */
class FileRepository : public domain::ThoughtRepository {
public:
    /**
     * @brief Constructor for FileRepository.
     * @param inboxPath Directory for incoming raw thoughts (.txt).
     * @param notesPath Directory for structured insights (.md).
     */
    FileRepository(const std::string& inboxPath, const std::string& notesPath, const std::string& historyPath, const std::string& observationsPath);

    /** @brief Fetches .txt files from the inbox. @see domain::ThoughtRepository::fetchInbox */
    std::vector<domain::RawThought> fetchInbox() override;

    /** @brief Compares file modification times. @see domain::ThoughtRepository::shouldProcess */
    bool shouldProcess(const domain::RawThought& thought, const std::string& insightId) override;

    /** @brief Saves insight to a markdown file in the notes directory. @see domain::ThoughtRepository::saveInsight */
    void saveInsight(const domain::Insight& insight) override;

    /** @brief Directly updates or creates a note file. @see domain::ThoughtRepository::updateNote */
    void updateNote(const std::string& filename, const std::string& content) override;

    /** @brief Loads all markdown files from the notes directory. @see domain::ThoughtRepository::fetchHistory */
    std::vector<domain::Insight> fetchHistory() override;

    /** @brief Identifies [[backlinks]] in notes. @see domain::ThoughtRepository::getBacklinks */
    std::vector<std::string> getBacklinks(const std::string& filename) override;

    /** @brief Generates activity data based on file modification times. @see domain::ThoughtRepository::getActivityHistory */
    std::map<std::string, int> getActivityHistory() override;

    /** @brief Scans .history for versions of a note. @see domain::ThoughtRepository::getVersions */
    std::vector<std::string> getVersions(const std::string& noteId) override;

    /** @brief Reads content of a version file. @see domain::ThoughtRepository::getVersionContent */
    std::string getVersionContent(const std::string& versionFilename) override;

    /** @brief Reads content of a note file. @see domain::ThoughtRepository::getNoteContent */
    std::string getNoteContent(const std::string& filename) override;

    /** @brief recursive search for observations. @see domain::ThoughtRepository::findObservationContent */
    std::optional<std::string> findObservationContent(const std::string& filename) override;

private:
    std::string m_inboxPath; ///< Path to inbox directory.
    std::string m_notesPath; ///< Path to notes directory.
    std::string m_historyPath; ///< Path to history/versioning directory.
    std::string m_observationsPath; ///< Path to observations directory.
    void logActivity();        ///< Logs current date activity to persistent file.
};

} // namespace ideawalker::infrastructure
