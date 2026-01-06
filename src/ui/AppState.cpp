#include "ui/AppState.hpp"

#include "application/OrganizerService.hpp"
#include "infrastructure/FileRepository.hpp"
#include "infrastructure/OllamaAdapter.hpp"

#include <algorithm>
#include <cstdio>
#include <filesystem>
#include <sstream>

namespace ideawalker::ui {

namespace {

bool EnsureProjectFolders(const std::filesystem::path& root) {
    try {
        std::filesystem::create_directories(root / "inbox");
        std::filesystem::create_directories(root / "notas");
        return true;
    } catch (...) {
        return false;
    }
}

bool CopyProjectData(const std::filesystem::path& fromRoot, const std::filesystem::path& toRoot) {
    try {
        std::filesystem::create_directories(toRoot / "inbox");
        std::filesystem::create_directories(toRoot / "notas");
        std::filesystem::copy(fromRoot / "inbox", toRoot / "inbox",
                              std::filesystem::copy_options::recursive | std::filesystem::copy_options::overwrite_existing);
        std::filesystem::copy(fromRoot / "notas", toRoot / "notas",
                              std::filesystem::copy_options::recursive | std::filesystem::copy_options::overwrite_existing);
        return true;
    } catch (...) {
        return false;
    }
}

} // namespace

AppState::AppState()
    : outputLog("Idea Walker v0.1.0-alpha - DDD Core initialized.\n") {
    saveAsFilename[0] = '\0';
    projectPathBuffer[0] = '\0';
}

AppState::~AppState() = default;

bool AppState::NewProject(const std::string& rootPath) {
    return OpenProject(rootPath);
}

bool AppState::OpenProject(const std::string& rootPath) {
    if (rootPath.empty()) {
        return false;
    }
    std::filesystem::path root = std::filesystem::path(rootPath);
    if (!EnsureProjectFolders(root)) {
        return false;
    }

    auto repo = std::make_unique<infrastructure::FileRepository>((root / "inbox").string(), (root / "notas").string());
    auto ai = std::make_unique<infrastructure::OllamaAdapter>();
    organizerService = std::make_unique<application::OrganizerService>(std::move(repo), std::move(ai));
    projectRoot = root.string();
    std::snprintf(projectPathBuffer, sizeof(projectPathBuffer), "%s", projectRoot.c_str());

    selectedInboxFilename.clear();
    selectedFilename.clear();
    selectedNoteContent.clear();
    unifiedKnowledge.clear();
    consolidatedInsight.reset();
    currentBacklinks.clear();

    RefreshInbox();
    RefreshAllInsights();
    AppendLog("[SYSTEM] Project opened: " + projectRoot + "\n");
    return true;
}

bool AppState::SaveProject() {
    if (projectRoot.empty()) {
        return false;
    }
    if (!EnsureProjectFolders(std::filesystem::path(projectRoot))) {
        return false;
    }
    AppendLog("[SYSTEM] Project saved: " + projectRoot + "\n");
    return true;
}

bool AppState::SaveProjectAs(const std::string& rootPath) {
    if (rootPath.empty()) {
        return false;
    }
    std::filesystem::path newRoot = std::filesystem::path(rootPath);
    if (projectRoot.empty()) {
        return OpenProject(newRoot.string());
    }
    std::filesystem::path currentRoot = std::filesystem::path(projectRoot);
    if (currentRoot == newRoot) {
        return SaveProject();
    }
    if (!CopyProjectData(currentRoot, newRoot)) {
        return false;
    }
    return OpenProject(newRoot.string());
}

bool AppState::CloseProject() {
    organizerService.reset();
    projectRoot.clear();
    projectPathBuffer[0] = '\0';
    selectedInboxFilename.clear();
    selectedFilename.clear();
    selectedNoteContent.clear();
    unifiedKnowledge.clear();
    consolidatedInsight.reset();
    currentBacklinks.clear();
    inboxThoughts.clear();
    allInsights.clear();
    activityHistory.clear();
    currentInsight.reset();
    AppendLog("[SYSTEM] Project closed.\n");
    return true;
}

void AppState::RefreshInbox() {
    if (organizerService) {
        inboxThoughts = organizerService->getRawThoughts();
    }
}

void AppState::RefreshAllInsights() {
    if (organizerService) {
        auto insights = organizerService->getAllInsights();
        allInsights.clear();
        consolidatedInsight.reset();
        for (auto& insight : insights) {
            if (insight.getMetadata().id == application::OrganizerService::kConsolidatedTasksFilename) {
                consolidatedInsight = std::make_unique<domain::Insight>(insight);
            } else {
                allInsights.push_back(std::move(insight));
            }
        }
        activityHistory = organizerService->getActivityHistory();
        std::sort(allInsights.begin(), allInsights.end(), [](const domain::Insight& a, const domain::Insight& b) {
            return a.getMetadata().id < b.getMetadata().id;
        });

        std::ostringstream combined;
        for (size_t i = 0; i < allInsights.size(); ++i) {
            const auto& insight = allInsights[i];
            combined << "## " << insight.getMetadata().id << "\n\n";
            combined << insight.getContent();
            if (i + 1 < allInsights.size()) {
                combined << "\n\n---\n\n";
            }
        }
        unifiedKnowledge = combined.str();
    }
}

void AppState::AppendLog(const std::string& line) {
    std::lock_guard<std::mutex> lock(logMutex);
    outputLog += line;
}

std::string AppState::GetLogSnapshot() {
    std::lock_guard<std::mutex> lock(logMutex);
    return outputLog;
}

} // namespace ideawalker::ui
