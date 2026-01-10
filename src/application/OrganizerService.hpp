/**
 * @file OrganizerService.hpp
 * @brief Application service for coordinating thought organization and actionable tracking.
 */

#pragma once
#include "domain/ThoughtRepository.hpp"
#include "domain/AIService.hpp"
#include "domain/TranscriptionService.hpp"
#include <functional>
#include <memory>
#include <functional>

namespace ideawalker::application {

/**
 * @class OrganizerService
 * @brief High-level service that coordinates the flow between repositories, AI services, and transcription.
 */
class OrganizerService {
public:
    /**
     * @enum ProcessResult
     * @brief Represents the outcome of processing an inbox item.
     */
    enum class ProcessResult {
        Processed,       ///< Item was successfully processed.
        SkippedUpToDate, ///< Item was skipped because it's already up to date.
        NotFound,        ///< Item could not be found.
        Failed           ///< Processing failed (e.g., AI error).
    };

    /** @brief Filename used for consolidated task list. */
    static constexpr const char* kConsolidatedTasksFilename = "_Consolidated_Tasks.md";

    /**
     * @brief Constructor for OrganizerService.
     * @param repo Pointer to the heart of project storage.
     * @param ai Pointer to the AI logic provider.
     * @param transcriber Pointer to the audio transcription service.
     */
    OrganizerService(std::unique_ptr<domain::ThoughtRepository> repo, 
                     std::unique_ptr<domain::AIService> ai,
                     std::unique_ptr<domain::TranscriptionService> transcriber);

    /**
     * @brief Processes all items in the inbox.
     * @param force If true, re-processes items even if they haven't changed.
     * @param statusCallback Optional callback for status.
     */
    void processInbox(bool force = false, bool fastMode = false, std::function<void(std::string)> statusCallback = nullptr);

    /**
     * @brief Processes a specific item in the inbox.
     * @param filename Name of the file to process.
     * @param force If true, re-processes the item even if it hasn't changed.
     * @param fastMode If true, use single-pass inference.
     * @param statusCallback Optional callback for status.
     * @return Result of the processing attempt.
     */
    ProcessResult processInboxItem(const std::string& filename, bool force = false, bool fastMode = false, std::function<void(std::string)> statusCallback = nullptr);

    /**
     * @brief Scans all insights and updates the consolidated task file.
     * @return True if successful.
     */
    bool updateConsolidatedTasks();

    /**
     * @brief Updates the content of a specific note/file.
     * @param filename The file to update.
     * @param content The new content.
     */
    void updateNote(const std::string& filename, const std::string& content);

    /**
     * @brief Toggles the completion status of a task within a note.
     * @param filename The note containing the task.
     * @param index The 0-based index of the task in the list.
     */
    void toggleTask(const std::string& filename, int index);

    /**
     * @brief Explicitly sets the status of a task.
     * @param filename The note containing the task.
     * @param index The index of the task.
     * @param completed Completed status.
     * @param inProgress In-progress status.
     */
    void setTaskStatus(const std::string& filename, int index, bool completed, bool inProgress);

    /**
     * @brief Retrieves a list of files that link to the specified file.
     * @param filename Target file.
     * @return List of filenames.
     */
    std::vector<std::string> getBacklinks(const std::string& filename);

    /**
     * @brief Retrieves activity statistics (notes per day).
     * @return Map of date string to note count.
     */
    std::map<std::string, int> getActivityHistory();

    /**
     * @brief Retrieves all raw thoughts currently in the inbox.
     * @return List of raw thoughts.
     */
    std::vector<domain::RawThought> getRawThoughts();

    /**
     * @brief Retrieves all insights (processed notes) from the history.
     * @return List of insights with parsed actionables.
     */
    std::vector<domain::Insight> getAllInsights();
    
    /**
     * @brief Asynchronously transcribes an audio file.
     * @param audioPath Path to the audio file.
     * @param onSuccess Callback invoked with the transcription text on success.
     * @param onError Callback invoked with an error message on failure.
     */
    void transcribeAudio(const std::string& audioPath, 
                        std::function<void(std::string)> onSuccess, 
                        std::function<void(std::string)> onError);


    /**
     * @brief Retrieves the list of available history versions for a note.
     * @param noteId The ID/Filename of the note.
     * @return List of version filenames.
     */
    std::vector<std::string> getNoteHistory(const std::string& noteId);

    /**
     * @brief Retrieves the content of a specific history version.
     * @param versionFilename The filename of the version.
     * @return The content string.
     */
    std::string getVersionContent(const std::string& versionFilename);
    
    /**
     * @brief Retrieves the content of a specific note.
     * @param filename The note filename.
     * @return Content string.
     */
    std::string getNoteContent(const std::string& filename);
    
private:
    std::unique_ptr<domain::ThoughtRepository> m_repo; ///< Repository for project data.
    std::unique_ptr<domain::AIService> m_ai; ///< AI processing service.
    std::unique_ptr<domain::TranscriptionService> m_transcriber; ///< Audio transcription service.
};

} // namespace ideawalker::application
