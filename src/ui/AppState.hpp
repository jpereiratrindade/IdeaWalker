#pragma once

#include <atomic>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

#include "domain/Insight.hpp"
#include "domain/ThoughtRepository.hpp"

namespace ideawalker::application {
class OrganizerService;
}

namespace ideawalker::ui {

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
    std::atomic<bool> pendingRefresh{false};
    int activeTab = 0; // 0: Dashboard, 1: Knowledge, 2: Execution
    int requestedTab = -1;

    std::unique_ptr<domain::Insight> currentInsight;
    std::unique_ptr<application::OrganizerService> organizerService;
    std::vector<domain::RawThought> inboxThoughts;
    std::vector<domain::Insight> allInsights;
    std::map<std::string, int> activityHistory;
    std::vector<std::string> currentBacklinks;
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
    void AppendLog(const std::string& line);
    std::string GetLogSnapshot();
};

} // namespace ideawalker::ui
