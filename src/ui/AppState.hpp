/**
 * @file AppState.hpp
 * @brief Core application state and UI logic coordination.
 */

#pragma once

#include <atomic>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "domain/Insight.hpp"
#include "domain/ThoughtRepository.hpp"
#include "domain/AIService.hpp"
#include "domain/Suggestion.hpp"
#include "domain/writing/MermaidGraph.hpp"
#include "domain/writing/services/CoherenceLensService.hpp"
#include "domain/writing/services/RevisionQualityService.hpp"
#include "application/AppServices.hpp"

namespace ideawalker::infrastructure { class PersistenceService; }

namespace ideawalker::ui {

    // Moved NodeType, NodeShape, LayoutOrientation, GraphNode, GraphLink to domain/writing/MermaidGraph.hpp
    
    using domain::writing::NodeType;
    using domain::writing::NodeShape;
    using domain::writing::LayoutOrientation;
    using domain::writing::GraphNode;
    using domain::writing::GraphLink;
    using domain::writing::PreviewGraphState;

    /**
     * @struct ExternalFile
     * @brief Represents a file opened from outside the project structure.
     */
    struct ExternalFile {
        std::string path;     ///< Full filesystem path.
        std::string filename; ///< Basename for display.
        std::string content;  ///< Current text content in editor.
        bool modified = false; ///< True if content has unsaved changes.
    };

/**
 * @struct AppState
 * @brief Singleton-like state containing all data needed for UI rendering and application flow.
 */
struct AppState {
    std::string outputLog;          ///< Log of system messages.
    std::string selectedNoteContent; ///< Content of the currently viewed note.
    std::string selectedFilename;   ///< Filename of the currently viewed note.
    std::string selectedInboxFilename; ///< Filename of the currently viewed inbox draft.
    std::string unifiedKnowledge;   ///< All notes concatenated for the "Organized Knowledge" view.
    bool unifiedKnowledgeView = true; ///< Toggle for the unified knowledge view.
    std::unique_ptr<domain::Insight> consolidatedInsight; ///< The special consolidated tasks insight.
    bool emojiEnabled = false;      ///< True if suitable fonts for emojis were loaded.
    std::string projectRoot;        ///< Absolute path to the active project dictionary.
    char projectPathBuffer[512];    ///< Buffer for the "Open Project" text input.
    bool showOpenProjectModal = false; ///< UI visibility flag.
    bool showSaveAsProjectModal = false; ///< UI visibility flag.
    bool showNewProjectModal = false; ///< UI visibility flag.
    bool requestExit = false;       ///< Set to true to trigger application shutdown.
    char saveAsFilename[128];       ///< Buffer for "Save As" input.
    std::atomic<bool> isProcessing{false}; ///< True while AI is organizing a thought.
    std::atomic<bool> isTranscribing{false}; ///< True while Whisper is transcribing audio.
    std::atomic<bool> pendingRefresh{false}; ///< Flag to trigger list refreshes from threads.
    int activeTab = 0; ///< Current main navigation tab.
    int requestedTab = -1; ///< Used to force tab switch.
    bool showTasksInGraph = true; ///< Neural Web setting.
    bool physicsEnabled = true;   ///< Neural Web setting.
    bool previewMode = false;     ///< Toggle for Markdown Preview in Knowledge tab.
    bool unifiedPreviewMode = false; ///< Toggle for unified knowledge markdown preview.
    bool fastMode = false; ///< Toggle for single-pass inference (CPU optimization).
    std::string processingStatus = "Thinking..."; ///< Status text while processing.
    
    std::vector<ExternalFile> externalFiles; ///< List of open external files.
    int selectedExternalFileIndex = -1; ///< Index of the currently edited external file.
    bool showOpenFileModal = false;  ///< UI visibility flag.
    char openFilePathBuffer[512] = ""; ///< Buffer for file path input.
    bool showTranscriptionModal = false; ///< UI visibility flag for audio transcription.
    char transcriptionPathBuffer[512] = ""; ///< Buffer for audio file path input.
    bool showSettingsModal = false; ///< Show the settings modal.
    bool showTaskDetailsModal = false; ///< Show the task details modal.
    std::string selectedTaskTitle; ///< Title of the selected task for the detailed modal.
    std::string selectedTaskOrigin; ///< Origin file of the selected task.
    std::string selectedTaskContent; ///< Full content or details of the selected task.UI visibility flag.

    // History Modal State
    bool showHistoryModal = false; ///< Show the history/timeline modal.
    std::string selectedNoteIdForHistory; ///< ID of the note being inspected.
    std::vector<std::string> historyVersions; ///< List of available versions.
    std::string selectedHistoryContent; ///< Content of the selected version.
    int selectedHistoryIndex = -1; ///< Index of the currently selected version.

    // Dialogue Selection State
    std::vector<std::string> dialogueFiles; ///< List of saved dialogue files.
    int selectedDialogueIndex = -1; ///< Currently selected dialogue index.
    void RefreshDialogueList(); ///< Refresh the list of saved dialogues.

    /** @brief Serializes the current Neural Web state to a Mermaid mindmap. */
    std::string ExportToMermaid() const;
    /** @brief Exports the entire knowledge base to a single Markdown file with Mermaid diagrams. */
    std::string ExportFullMarkdown() const;
    /** @brief Advances the force-directed graph physics by one step. */
    void UpdateGraphPhysics(const std::unordered_set<int>& selectedNodes = {});
    /** @brief Resets all node positions to the center of the viewport. */
    void CenterGraph();

    /** @brief Loads a file from outside the project hierarchy. */
    void OpenExternalFile(const std::string& path);
    /** @brief Writes an external file's memory buffer back to disk. */
    void SaveExternalFile(int index);

    std::unique_ptr<domain::Insight> currentInsight; ///< Domain object for the active note.
    
    application::AppServices services; ///< Injected services.

    std::vector<domain::RawThought> inboxThoughts; ///< List of items in the project inbox.
    std::vector<domain::Insight> allInsights; ///< List of all project notes.
    std::map<std::string, int> activityHistory; ///< Map for the activity heatmap (date -> count).
    std::vector<std::string> currentBacklinks;  ///< Backlinks for the currently selected note.
    
    std::vector<GraphNode> graphNodes; ///< Nodes for the Neural Web view.
    std::vector<GraphLink> graphLinks; ///< Links for the Neural Web view.
    bool graphInitialized = false; ///< Initialization flag for ImNodes positions.

    std::map<int, PreviewGraphState> previewGraphs; ///< Multi-diagram cache.

    void* mainGraphContext = nullptr; ///< Pointer to the ImNodes editor context for Neural Web.
    void* previewGraphContext = nullptr; ///< Pointer to the ImNodes editor context for Mermaid Preview.

    std::mutex logMutex; ///< Mutex for thread-safe logging.
    std::mutex processingStatusMutex; ///< Mutex for thread-safe status updates.

    AppState();
    ~AppState();

    /** @brief Initializes ImNodes contexts. */
    void InitImNodes();
    /** @brief Destroys ImNodes contexts. */
    void ShutdownImNodes();

    /** @brief Creates and opens a new project at the given path. */
    bool NewProject(const std::string& rootPath);
    /** @brief Opens an existing project. */
    bool OpenProject(const std::string& rootPath);
    /** @brief Saves current metadata (future-proofing). */
    bool SaveProject();
    /** @brief Clones the project data to a new directory and opens it. */
    bool SaveProjectAs(const std::string& rootPath);
    /** @brief Resets the state and clears all project data from memory. */
    bool CloseProject();
    /** @brief Triggers a reload of the inbox from the filesystem. */
    void RefreshInbox();
    /** @brief Triggers a reload of all notes and rebuilds the Neural Web graph. */
    void RefreshAllInsights();
    /** @brief Handles file dnd events (e.g., audio files for transcription). */
    void HandleFileDrop(const std::string& filePath);
    /** @brief Explicitly requests transcription for a file path. */
    void RequestTranscription(const std::string& filePath);
    /** @brief Regenerates the Neural Web graph nodes and links from notes. */
    void RebuildGraph();
    /** @brief Thread-safe log append. */
    void AppendLog(const std::string& line);
    /** @brief Thread-safe log retrieval. */
    std::string GetLogSnapshot();
    /** @brief Thread-safe processing status update. */
    void SetProcessingStatus(const std::string& status);
    /** @brief Thread-safe processing status retrieval. */
    std::string GetProcessingStatus();
    /** @brief Loads history versions and resets selection for a note. */
    void LoadHistory(const std::string& noteId);

    /** @brief Injects the required services into the state. */
    void InjectServices(application::AppServices&& newServices);
    // Suggestion Engine UI State
    std::vector<domain::Suggestion> currentSuggestions;
    std::atomic<bool> isAnalyzingSuggestions{false};
    std::mutex suggestionsMutex;

    /** @brief Triggers a background indexing and suggestion generation session. */
    void AnalyzeSuggestions();
    
    // Writing Trajectory Context UI State
    bool showTrajectoryPanel = false;
    bool showSegmentEditor = false;
    bool showDefensePanel = false;
    std::string activeTrajectoryId;
    
    // Coherence Analysis Results
    std::vector<ideawalker::domain::writing::Inconsistency> coherenceIssues;
    ideawalker::domain::writing::QualityReport lastQualityReport;

    // Configuration / Persistence
    std::string currentAIModel; ///< The currently selected AI model.
    std::string videoDriverPreference; ///< Preference for SDL Video Driver (e.g. "x11" or "wayland").
    /** @brief Loads application settings from the project root. */
    void LoadConfig();
    /** @brief Saves application settings to the project root. */
    void SaveConfig();
    /** @brief Sets the AI model and persists the choice. */
    void SetAIModel(const std::string& modelName);

    bool showConversationPanel = true; ///< Visibility for the Cognitive Dialogue Panel.

    // Help & Updates
    bool updateAvailable = false;
    std::string latestVersion;
    std::atomic<bool> isCheckingUpdates{false};
    bool showUpdateModal = false;
    bool showHelpModal = false;
    
    /** @brief Triggers an asynchronous check for updates on GitHub. */
    void CheckForUpdates();
};

} // namespace ideawalker::ui
