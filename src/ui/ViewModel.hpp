/**
 * @file ViewModel.hpp
 * @brief Pure UI and Data State binding for ImGui views.
 */

#pragma once

#include <atomic>
#include <map>
#include <mutex>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <memory>

#include "domain/Insight.hpp"

#include "domain/Suggestion.hpp"
#include "domain/SourceArtifact.hpp"
#include "domain/writing/MermaidGraph.hpp"
#include "domain/writing/services/CoherenceLensService.hpp"
#include "domain/writing/services/RevisionQualityService.hpp"

namespace ideawalker::ui {

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
     * @struct ViewModel
     * @brief Pure UI and Data State binding for ImGui views.
     */
    struct ViewModel {
        ProjectState project;
        NeuralWebState neuralWeb;
        ExternalFilesState external;
        UiState ui;

        /** @brief Thread-safe log append. */
        void AppendLog(const std::string& line) {
            std::lock_guard<std::mutex> lock(ui.logMutex);
            ui.outputLog += line;
        }

        /** @brief Thread-safe log retrieval. */
        std::string GetLogSnapshot() {
            std::lock_guard<std::mutex> lock(ui.logMutex);
            return ui.outputLog;
        }

        /** @brief Thread-safe processing status update. */
        void SetProcessingStatus(const std::string& status) {
            std::lock_guard<std::mutex> lock(ui.statusMutex);
            ui.processingStatus = status;
        }

        /** @brief Thread-safe processing status retrieval. */
        std::string GetProcessingStatus() {
            std::lock_guard<std::mutex> lock(ui.statusMutex);
            return ui.processingStatus;
        }
    };

} // namespace ideawalker::ui
