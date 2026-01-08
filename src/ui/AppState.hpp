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

namespace ideawalker::application {
class OrganizerService;
class ConversationService;
class ContextAssembler;
class DocumentIngestionService;
}

namespace ideawalker::ui {

    /**
     * @enum NodeType
     * @brief Types of nodes available in the interactive graph.
     */
    enum class NodeType {
        INSIGHT,    ///< A structured note or insight.
        TASK_TODO,  ///< A task that hasn't been started.
        TASK_DONE,  ///< A completed task.
        NOTE_LINK,  ///< A wiki-style link to another note.
        TASK,       ///< General task node.
        CONCEPT,    ///< A referenced concept that doesn't exist as a file yet.
        HYPOTHESIS  ///< A temporary anchor for integration hypotheses (Cyan).
    };

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
     * @enum NodeShape
     * @brief Mermaid-style shapes for graph rendering.
     */
    enum class NodeShape {
        BOX,            ///< [ ] Square box.
        ROUNDED_BOX,    ///< ( ) Rounded box.
        CIRCLE,         ///< (( )) Circle.
        STADIUM,        ///< ([ ]) Stadium shape.
        SUBROUTINE,     ///< [[ ]] Double-bordered box.
        CYLINDER,       ///< [( )] Cylinder/Database.
        HEXAGON,        ///< {{ }} Hexagon.
        RHOMBUS,        ///< { } Diamond.
        ASYMMETRIC,     ///< > ] Flag shape.
        BANG,           ///< )) (( Bang shape.
        CLOUD           ///< ) ( Cloud shape.
    };

    /**
     * @enum LayoutOrientation
     * @brief Orientation for tree-like diagrams.
     */
    enum class LayoutOrientation {
        LeftRight,
        TopDown
    };

    /**
     * @struct GraphNode
     * @brief Represents a visible node in the graph (Neural Web or Mermaid Preview).
     */
    struct GraphNode {
        int id; ///< Unique node ID.
        std::string title; ///< Text label.
        float x, y; ///< Normalized or screen coordinates.
        float w, h; ///< Exact dimensions (width, height) calculated for rendering.
        float wrapW = 200.0f; ///< The text wrap width used during size calculation.
        float vx, vy; ///< Velocity vectors for physics simulation.
        NodeType type; ///< Categorical type.
        bool isCompleted = false; ///< Task status (if applicable).
        bool isInProgress = false; ///< Task status (if applicable).
        NodeShape shape = NodeShape::ROUNDED_BOX; ///< Visual shape style.
    };

/**
 * @struct GraphLink
 * @brief Represents a connection between two nodes.
 */
struct GraphLink {
    int id; ///< Unique link ID.
    int startNode; ///< Source node ID.
    int endNode; ///< Destination node ID.
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
    domain::AIPersona currentPersona = domain::AIPersona::AnalistaCognitivo; ///< The currently selected AI persona.
    std::string processingStatus = "Thinking..."; ///< Status text while processing.
    
    std::vector<ExternalFile> externalFiles; ///< List of open external files.
    int selectedExternalFileIndex = -1; ///< Index of the currently edited external file.
    bool showOpenFileModal = false;  ///< UI visibility flag.
    char openFilePathBuffer[512] = ""; ///< Buffer for file path input.
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
    std::unique_ptr<application::OrganizerService> organizerService; ///< Service that powers the app logic.
    std::vector<domain::RawThought> inboxThoughts; ///< List of items in the project inbox.
    std::vector<domain::Insight> allInsights; ///< List of all project notes.
    std::map<std::string, int> activityHistory; ///< Map for the activity heatmap (date -> count).
    std::vector<std::string> currentBacklinks;  ///< Backlinks for the currently selected note.
    
    std::vector<GraphNode> graphNodes; ///< Nodes for the Neural Web view.
    std::vector<GraphLink> graphLinks; ///< Links for the Neural Web view.
    bool graphInitialized = false; ///< Initialization flag for ImNodes positions.

    /**
     * @struct PreviewGraphState
     * @brief Cached layout and nodes for a specific Mermaid block in Markdown.
     */
    struct PreviewGraphState {
        std::vector<GraphNode> nodes; ///< List of nodes in the diagram.
        std::vector<GraphLink> links; ///< List of links between nodes.
        std::unordered_map<int, int> nodeById; ///< Mapping of node IDs to their indices in the nodes vector (O(1) lookup).
        std::vector<int> roots; ///< List of root node IDs for the hierarchical layout.
        std::unordered_map<int, std::vector<int>> childrenNodes; ///< Adjacency list for fast tree traversal.
        bool initialized = false; ///< Flag indicating if the graph layout has been initialized.
        LayoutOrientation orientation = LayoutOrientation::LeftRight; ///< Target orientation (LR or TB).
        std::string lastContent; ///< Cached content to prevent redundant layout updates.
    };
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

    // Conversational Context State
    std::unique_ptr<application::ConversationService> conversationService;
    std::unique_ptr<application::ContextAssembler> contextAssembler;
    std::unique_ptr<application::DocumentIngestionService> ingestionService;
    
    bool showConversationPanel = true; ///< Visibility for the Cognitive Dialogue Panel.
};

} // namespace ideawalker::ui
