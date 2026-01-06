#include "ui/AppState.hpp"

#include "application/OrganizerService.hpp"
#include "infrastructure/FileRepository.hpp"
#include "infrastructure/OllamaAdapter.hpp"

#include <algorithm>
#include <cstdio>
#include <cmath>
#include <filesystem>
#include <regex>
#include <sstream>
#include <unordered_map>

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
        
        RebuildGraph();
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

void AppState::RebuildGraph() {
    graphNodes.clear();
    graphLinks.clear();
    graphInitialized = false;

    if (allInsights.empty()) return;

    std::unordered_map<std::string, int> nameToId;
    int nextId = 0;

    // 1. Create Nodes
    for (const auto& insight : allInsights) {
        GraphNode node;
        node.id = nextId++;
        node.title = insight.getMetadata().id;
        
        // Random initial position or circle layout
        float angle = (float(node.id) / allInsights.size()) * 2.0f * 3.14159f;
        float radius = 200.0f + (node.id * 10.0f); 
        node.x = std::cos(angle) * radius;
        node.y = std::sin(angle) * radius;
        node.vx = 0.0f;
        node.vy = 0.0f;

        graphNodes.push_back(node);
        nameToId[node.title] = node.id;
    }

    // 2. Create Links
    int linkIdCounter = 0;
    std::regex linkRegex(R"(\[\[(.*?)\]\])");
    for (const auto& insight : allInsights) {
        std::string content = insight.getContent();
        std::string sourceName = insight.getMetadata().id;
        int sourceId = nameToId[sourceName];

        auto words_begin = std::sregex_iterator(content.begin(), content.end(), linkRegex);
        auto words_end = std::sregex_iterator();

        for (std::sregex_iterator i = words_begin; i != words_end; ++i) {
            std::smatch match = *i;
            std::string targetName = match[1].str();
            
            // Normalize target name if needed (e.g., remove alias)
            // For now, assume exact match
            
            auto it = nameToId.find(targetName);
            if (it != nameToId.end()) {
                GraphLink link;
                link.id = linkIdCounter++;
                link.startNode = sourceId;
                link.endNode = it->second;
                graphLinks.push_back(link);
            }
        }
    }
}

void AppState::UpdateGraphPhysics() {
    const float repulsion = 1000.0f;
    const float springLength = 150.0f;
    const float springK = 0.05f;
    const float damping = 0.90f;
    const float dt = 0.16f; // Fixed small step for stability

    // Reset forces
    std::vector<std::pair<float, float>> forces(graphNodes.size(), {0.0f, 0.0f});

    // 1. Repulsion (Nodes repel each other)
    for (size_t i = 0; i < graphNodes.size(); ++i) {
        for (size_t j = i + 1; j < graphNodes.size(); ++j) {
            float dx = graphNodes[i].x - graphNodes[j].x;
            float dy = graphNodes[i].y - graphNodes[j].y;
            float distSq = dx*dx + dy*dy;
            if (distSq < 0.01f) distSq = 0.01f; // Avoid singularity

            float dist = std::sqrt(distSq);
            float force = repulsion / distSq;
            
            float fx = (dx / dist) * force;
            float fy = (dy / dist) * force;

            forces[i].first += fx;
            forces[i].second += fy;
            forces[j].first -= fx;
            forces[j].second -= fy;
        }
    }

    // 2. Attraction (Links pull nodes together)
    for (const auto& link : graphLinks) {
        // Find node indices (IDs correspond to index in graphNodes because we generated them sequentially)
        // Safety check if IDs get complex later
        if (link.startNode >= graphNodes.size() || link.endNode >= graphNodes.size()) continue;

        GraphNode& n1 = graphNodes[link.startNode];
        GraphNode& n2 = graphNodes[link.endNode];

        float dx = n2.x - n1.x;
        float dy = n2.y - n1.y;
        float dist = std::sqrt(dx*dx + dy*dy);
        if (dist < 0.01f) dist = 0.01f;

        float displacement = dist - springLength;
        float force = displacement * springK;

        float fx = (dx / dist) * force;
        float fy = (dy / dist) * force;

        forces[link.startNode].first += fx;
        forces[link.startNode].second += fy;
        forces[link.endNode].first -= fx;
        forces[link.endNode].second -= fy;
    }

    // 3. Center Gravity (Weak pull to 0,0)
    for (size_t i = 0; i < graphNodes.size(); ++i) {
        float dist = std::sqrt(graphNodes[i].x*graphNodes[i].x + graphNodes[i].y*graphNodes[i].y);
        if (dist > 0.01f) {
            forces[i].first -= (graphNodes[i].x / dist) * 0.5f;
            forces[i].second -= (graphNodes[i].y / dist) * 0.5f;
        }
    }

    // 4. Update Positions
    for (size_t i = 0; i < graphNodes.size(); ++i) {
        auto& node = graphNodes[i];
        node.vx = (node.vx + forces[i].first * dt) * damping;
        node.vy = (node.vy + forces[i].second * dt) * damping;

        node.x += node.vx * dt;
        node.y += node.vy * dt;

        // Clamp generic bounds if needed, but not strictly required
    }
}

} // namespace ideawalker::ui
