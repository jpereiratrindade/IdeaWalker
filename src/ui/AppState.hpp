/**
 * @file AppState.hpp
 * @brief Core application state and UI logic coordination.
 */

#pragma once

#include <atomic>
#include <functional>
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
#include "domain/SourceArtifact.hpp"
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
     * @struct ProjectState
     * @brief State related to the active project and its data.
     */
    struct ProjectState {
        std::string root;               ///< Absolute path to the active project directory.
        char pathBuffer[512] = "";      ///< Buffer for the "Open Project" text input.
        std::vector<domain::Insight> allInsights; ///< List of all project notes.
        std::vector<domain::RawThought> inboxThoughts; ///< List of items in the project inbox.
        std::map<std::string, int> activityHistory; ///< Map for the activity heatmap (date -> count).
        std::unique_ptr<domain::Insight> consolidatedInsight; ///< Special consolidated tasks insight.
        std::unique_ptr<domain::Insight> currentInsight; ///< Domain object for the active note.
        std::string currentAIModel;     ///< The currently selected AI model.
        std::string videoDriverPreference; ///< Preference for SDL Video Driver.
    };

    /**
     * @struct NeuralWebState
     * @brief State for the graph visualization.
     */
    struct NeuralWebState {
        std::vector<GraphNode> nodes;
        std::vector<GraphLink> links;
        bool initialized = false;
        bool showTasks = true;
        bool physicsEnabled = true;
        void* mainContext = nullptr;
        void* previewContext = nullptr;
        std::map<int, PreviewGraphState> previewGraphs;
    };

    /**
     * @struct ExternalFilesState
     * @brief State for files opened outside the project.
     */
    struct ExternalFilesState {
        std::vector<ExternalFile> files;
        int selectedIndex = -1;
    };

    /**
     * @struct UiState
     * @brief State for UI flags, buffers, and navigation.
     */
    struct UiState {
        std::string outputLog;
        std::string selectedNoteContent;
        std::string selectedFilename;
        std::string selectedInboxFilename;
        std::string unifiedKnowledge;
        bool unifiedKnowledgeView = true;
        bool emojiEnabled = false;
        int activeTab = 0;
        int requestedTab = -1;
        bool previewMode = false;
        bool unifiedPreviewMode = false;
        bool fastMode = false;
        std::string processingStatus = "Thinking...";
        bool requestExit = false;
        float dashboardLogHeight = 220.0f;

        // Modals visibility and buffers
        bool showOpenProject = false;
        bool showSaveAsProject = false;
        bool showNewProject = false;
        bool showOpenFile = false;
        bool showTranscription = false;
        bool showSettings = false;
        bool showTaskDetails = false;
        bool showHistory = false;
        bool showUpdate = false;
        bool showHelp = false;
        bool showTrajectoryPanel = false;
        bool showSegmentEditor = false;
        bool showDefensePanel = false;
        bool showConversation = true;

        char saveAsFilename[128] = "";
        char openFilePathBuffer[512] = "";
        char transcriptionPathBuffer[512] = "";
        
        // Task Details
        std::string selectedTaskTitle;
        std::string selectedTaskOrigin;
        std::string selectedTaskContent;

        // History
        std::string selectedNoteIdForHistory;
        std::vector<std::string> historyVersions;
        std::string selectedHistoryContent;
        int selectedHistoryIndex = -1;

        // Dialogue
        std::vector<std::string> dialogueFiles;
        int selectedDialogueIndex = -1;

        // Suggestions
        std::vector<domain::Suggestion> currentSuggestions;
        std::vector<std::string> currentBacklinks;
        std::vector<domain::SourceArtifact> scientificInboxArtifacts;
        std::unordered_set<std::string> scientificInboxSelected;
        bool scientificInboxLoaded = false;

        // Async status (Atomic)
        std::atomic<bool> isProcessing{false};
        std::atomic<bool> isTranscribing{false};
        std::atomic<bool> pendingRefresh{false};
        std::atomic<bool> isAnalyzingSuggestions{false};
        std::atomic<bool> isCheckingUpdates{false};

        // Mutexes
        std::mutex logMutex;
        std::mutex statusMutex;
        std::mutex suggestionsMutex;

        // Results
        std::vector<domain::writing::Inconsistency> coherenceIssues;
        domain::writing::QualityReport lastQualityReport;
        bool updateAvailable = false;
        std::string latestVersion;
        std::string activeTrajectoryId;
    };

/**
 * @struct AppState
 * @brief Singleton-like state containing all data needed for UI rendering and application flow.
 */
struct AppState {
    ProjectState project;
    NeuralWebState neuralWeb;
    ExternalFilesState external;
    UiState ui;

    application::AppServices services; ///< Injected services.
    std::function<application::AppServices(const std::string&)> servicesFactory; ///< Builds services for a project root.

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

    /** @brief Triggers a background indexing and suggestion generation session. */
    void AnalyzeSuggestions();

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

    /** @brief Loads application settings from the project root. */
    void LoadConfig();
    /** @brief Saves application settings to the project root. */
    void SaveConfig();
    /** @brief Sets the AI model and persists the choice. */
    void SetAIModel(const std::string& modelName);

    /** @brief Triggers an asynchronous check for updates on GitHub. */
    void CheckForUpdates();

    void RefreshDialogueList();
};

} // namespace ideawalker::ui
