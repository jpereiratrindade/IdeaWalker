#pragma once

#include <atomic>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_set>
#include <vector>

#include "domain/Insight.hpp"
#include "domain/ThoughtRepository.hpp"

namespace ideawalker::application {
class OrganizerService;
}

namespace ideawalker::ui {

enum class NodeType {
    INSIGHT,
    TASK
};

struct GraphNode {
    int id;
    std::string title;
    float x, y;
    float vx, vy;
    NodeType type = NodeType::INSIGHT;
    bool isCompleted = false;
    bool isInProgress = false;
};

struct GraphLink {
    int id;
    int startNode;
    int endNode;
};

struct AppState {
    std::string outputLog;
    std::string selectedNoteContent;
    std::string selectedFilename;
    std::string selectedInboxFilename;
    std::string unifiedKnowledge;
    bool unifiedKnowledgeView = true;
    std::unique_ptr<domain::Insight> consolidatedInsight;
    bool emojiEnabled = false;
    std::string projectRoot;
    char projectPathBuffer[512];
    bool showOpenProjectModal = false;
    bool showSaveAsProjectModal = false;
    bool showNewProjectModal = false;
    bool requestExit = false;
    char saveAsFilename[128];
    std::atomic<bool> isProcessing{false};
    std::atomic<bool> isTranscribing{false}; // New flag
    std::atomic<bool> pendingRefresh{false};
    int activeTab = 0; // 0: Dashboard, 1: Knowledge, 2: Execution, 3: Neural Web
    int requestedTab = -1;
    bool showTasksInGraph = true;
    bool physicsEnabled = true;

    std::string ExportToMermaid() const;
    std::string ExportFullMarkdown() const;
    void UpdateGraphPhysics(const std::unordered_set<int>& selectedNodes = {});
    void CenterGraph();

    std::unique_ptr<domain::Insight> currentInsight;
    std::unique_ptr<application::OrganizerService> organizerService;
    std::vector<domain::RawThought> inboxThoughts;
    std::vector<domain::Insight> allInsights;
    std::map<std::string, int> activityHistory;
    std::vector<std::string> currentBacklinks;
    
    // Graph State
    std::vector<GraphNode> graphNodes;
    std::vector<GraphLink> graphLinks;
    bool graphInitialized = false;

    std::mutex logMutex;

    AppState();
    ~AppState();

    bool NewProject(const std::string& rootPath);
    bool OpenProject(const std::string& rootPath);
    bool SaveProject();
    bool SaveProjectAs(const std::string& rootPath);
    bool CloseProject();
    void RefreshInbox();
    void RefreshAllInsights();
    void HandleFileDrop(const std::string& filePath);
    void RebuildGraph();
    void UpdateGraphPhysics();
    void AppendLog(const std::string& line);
    std::string GetLogSnapshot();
};

} // namespace ideawalker::ui
