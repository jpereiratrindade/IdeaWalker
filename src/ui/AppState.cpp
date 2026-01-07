/**
 * @file AppState.cpp
 * @brief Implementation of the AppState class and state management logic.
 */
#include "ui/AppState.hpp"

#include "application/OrganizerService.hpp"
#include "infrastructure/FileRepository.hpp"
#include "infrastructure/FileRepository.hpp"
#include "infrastructure/OllamaAdapter.hpp"
#include "infrastructure/FileRepository.hpp"
#include "infrastructure/OllamaAdapter.hpp"
#include "infrastructure/WhisperCppAdapter.hpp"
#include "infrastructure/PathUtils.hpp"

#include <algorithm>
#include <cstdio>
#include <cmath>
#include <filesystem>
#include <filesystem>
#include <fstream>
#include <regex>
#include <sstream>
#include <unordered_map>
#include <unordered_set>

#include "imnodes.h"

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
    : outputLog("Idea Walker v0.1.0-alpha - Núcleo DDD inicializado.\n") {
    saveAsFilename[0] = '\0';
    projectPathBuffer[0] = '\0';

    // Initialize ImNodes contexts in InitImNodes() instead
}

AppState::~AppState() {
    // Cleanup in ShutdownImNodes()
}

void AppState::InitImNodes() {
    if (!mainGraphContext) {
        mainGraphContext = ImNodes::EditorContextCreate();
        ImNodes::EditorContextSet((ImNodesEditorContext*)mainGraphContext);
        ImNodes::PushAttributeFlag(ImNodesAttributeFlags_EnableLinkDetachWithDragClick);
    }
    
    if (!previewGraphContext) {
        previewGraphContext = ImNodes::EditorContextCreate();
    }
}

void AppState::ShutdownImNodes() {
    if (mainGraphContext) {
        ImNodes::EditorContextFree((ImNodesEditorContext*)mainGraphContext);
        mainGraphContext = nullptr;
    }
    if (previewGraphContext) {
        ImNodes::EditorContextFree((ImNodesEditorContext*)previewGraphContext);
        previewGraphContext = nullptr;
    }
}

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
    
    // Pure C++ Implementation using Whisper.cpp
    // Model expected at standard XDG location: ~/.local/share/IdeaWalker/models/ggml-medium.bin
    // Fallback to project root if needed (optional, but XDG is preferred now)
    auto modelsDir = infrastructure::PathUtils::GetModelsDir();
    std::string modelPath = (modelsDir / "ggml-medium.bin").string();
    
    // Check if model exists in XDG, if not check local project for backward compatibility checking
    if (!std::filesystem::exists(modelPath)) {
         if (std::filesystem::exists(root / "ggml-medium.bin")) {
             modelPath = (root / "ggml-medium.bin").string();
         }
    }
    std::string inboxPath = (root / "inbox").string();

    auto transcriber = std::make_unique<infrastructure::WhisperCppAdapter>(modelPath, inboxPath);

    organizerService = std::make_unique<application::OrganizerService>(std::move(repo), std::move(ai), std::move(transcriber));
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
    RefreshAllInsights();
    AppendLog("[SISTEMA] Projeto aberto: " + projectRoot + "\n");
    return true;
}

bool AppState::SaveProject() {
    if (projectRoot.empty()) {
        return false;
    }
    if (!EnsureProjectFolders(std::filesystem::path(projectRoot))) {
        return false;
    }
    AppendLog("[SISTEMA] Projeto salvo: " + projectRoot + "\n");
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
    AppendLog("[SISTEMA] Projeto fechado.\n");
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

void AppState::SetProcessingStatus(const std::string& status) {
    std::lock_guard<std::mutex> lock(processingStatusMutex);
    processingStatus = status;
}

std::string AppState::GetProcessingStatus() {
    std::lock_guard<std::mutex> lock(processingStatusMutex);
    return processingStatus;
}

void AppState::RebuildGraph() {
    graphNodes.clear();
    graphLinks.clear();
    graphInitialized = false;

    if (allInsights.empty()) return;

    std::unordered_map<std::string, int> nameToId;
    int nextId = 0;

    // 1. Create Nodes and internal Task Links
    int linkIdCounter = 0;
    for (const auto& insight : allInsights) {
        GraphNode node;
        node.id = nextId++;
        node.type = NodeType::INSIGHT;
        node.title = insight.getMetadata().title.empty() ? insight.getMetadata().id : insight.getMetadata().title;
        
        float angle = (float(node.id) / allInsights.size()) * 2.0f * 3.14159f;
        float radius = 100.0f + (rand() % 200); 
        node.x = 800.0f + std::cos(angle) * radius;
        node.y = 450.0f + std::sin(angle) * radius;
        node.vx = node.vy = 0.0f;

        graphNodes.push_back(node);
        int parentId = node.id;
        
        nameToId[insight.getMetadata().id] = node.id;
        if (!insight.getMetadata().title.empty()) {
            nameToId[insight.getMetadata().title] = node.id;
        }

        // Add Tasks as sub-nodes if enabled
        if (showTasksInGraph) {
            for (const auto& task : insight.getActionables()) {
                GraphNode taskNode;
                taskNode.id = nextId++;
                taskNode.type = NodeType::TASK;
                taskNode.title = task.description;
                taskNode.isCompleted = task.isCompleted;
                taskNode.isInProgress = task.isInProgress;

                // Offset slightly from parent
                float tAngle = (float(rand()) / RAND_MAX) * 2.0f * 3.14159f;
                taskNode.x = node.x + std::cos(tAngle) * 50.0f;
                taskNode.y = node.y + std::sin(tAngle) * 50.0f;
                taskNode.vx = taskNode.vy = 0.0f;

                graphNodes.push_back(taskNode);

                // Create Link: Parent -> Task
                graphLinks.push_back({linkIdCounter++, parentId, taskNode.id});
            }
        }
    }

    // 2. Create Cross-Note Links
    // 2. Create Cross-Note Links
    
    // Pre-calculate which titles are worth searching (at least 4 chars to avoid tiny common words)
    std::vector<std::pair<std::string, int>> searchableTitles;
    for (const auto& insight : allInsights) {
        std::string title = insight.getMetadata().title;
        if (title.length() >= 4) {
            searchableTitles.push_back({title, nameToId[title]});
        }
    }

    for (const auto& insight : allInsights) {
        std::string content = insight.getContent();
        std::string sourceName = insight.getMetadata().id;
        int sourceId = nameToId[sourceName];

        // 2a. Explicit [[Wikilinks]] (High Priority)
        // Parse references from content (newly added method)
        // Note: const_cast is ugly but we need to parse. Ideally Insight does this on load, but we do it here for now.
        const_cast<domain::Insight&>(insight).parseReferencesFromContent();
        
        std::unordered_set<int> linkedNodes;
        for (const auto& ref : insight.getReferences()) {
            std::string targetName = ref;
            // Trim
            targetName.erase(0, targetName.find_first_not_of(" \t\n\r"));
            size_t end = targetName.find_last_not_of(" \t\n\r");
            if (end != std::string::npos) targetName.erase(end + 1);
            
            if (targetName.empty()) continue;

            int targetId = -1;
            auto it = nameToId.find(targetName);
            if (it == nameToId.end()) it = nameToId.find(targetName + ".md");

            if (it != nameToId.end()) {
                targetId = it->second;
            } else {
                // CONCEPT NODE CREATION (Ghost Node)
                GraphNode conceptNode;
                conceptNode.id = nextId++;
                conceptNode.type = NodeType::CONCEPT; // New type
                conceptNode.title = targetName;
                
                // Position near the source node but with some chaos
                float angle = (float(rand()) / RAND_MAX) * 2.0f * 3.14159f;
                conceptNode.x = graphNodes[nameToId[sourceName]].x + std::cos(angle) * 150.0f;
                conceptNode.y = graphNodes[nameToId[sourceName]].y + std::sin(angle) * 150.0f;
                conceptNode.vx = conceptNode.vy = 0.0f;

                graphNodes.push_back(conceptNode);
                targetId = conceptNode.id;
                nameToId[targetName] = targetId; // Register so others link to same concept
            }

            if (targetId != sourceId && targetId != -1) {
                if (linkedNodes.find(targetId) == linkedNodes.end()) {
                    GraphLink link{linkIdCounter++, sourceId, targetId};
                    graphLinks.push_back(link);
                    linkedNodes.insert(targetId);
                }
            }
        }

        // 2b. Implicit Semantic Links (Search titles in text)
        for (const auto& [title, targetId] : searchableTitles) {
            if (targetId == sourceId) continue;
            if (linkedNodes.find(targetId) != linkedNodes.end()) continue;

            // Simple case-insensitive search or exact search
            if (content.find(title) != std::string::npos) {
                GraphLink link{linkIdCounter++, sourceId, targetId};
                graphLinks.push_back(link);
                linkedNodes.insert(targetId);
            }
        }
    }
}

void AppState::UpdateGraphPhysics(const std::unordered_set<int>& selectedNodes) {
    const float repulsionInsight = 1000.0f; 
    const float repulsionTask = 200.0f;
    const float springLengthInsight = 300.0f; // Distance between linked notes
    const float springLengthTask = 80.0f;    // Tasks stay close to parent
    const float springK = 0.06f;
    const float damping = 0.60f;
    
    const float centerX = 800.0f;
    const float centerY = 450.0f;

    std::vector<std::pair<float, float>> forces(graphNodes.size(), {0.0f, 0.0f});

    // 1. Repulsion
    for (size_t i = 0; i < graphNodes.size(); ++i) {
        for (size_t j = i + 1; j < graphNodes.size(); ++j) {
            float dx = graphNodes[i].x - graphNodes[j].x;
            float dy = graphNodes[i].y - graphNodes[j].y;
            float distSq = dx*dx + dy*dy;
            if (distSq < 400.0f) distSq = 400.0f; 

            float dist = std::sqrt(distSq);
            
            // Adjust repulsion based on node types
            float r = (graphNodes[i].type == NodeType::INSIGHT && graphNodes[j].type == NodeType::INSIGHT) 
                      ? repulsionInsight : repulsionTask;
            
            float force = r / dist; 

            float fx = (dx / dist) * force;
            float fy = (dy / dist) * force;

            forces[i].first += fx;
            forces[i].second += fy;
            forces[j].first -= fx;
            forces[j].second -= fy;
        }
    }

    // 2. Attraction
    for (const auto& link : graphLinks) {
        if (link.startNode >= graphNodes.size() || link.endNode >= graphNodes.size()) continue;

        GraphNode& n1 = graphNodes[link.startNode];
        GraphNode& n2 = graphNodes[link.endNode];

        float dx = n2.x - n1.x;
        float dy = n2.y - n1.y;
        float dist = std::sqrt(dx*dx + dy*dy);
        if (dist < 1.0f) dist = 1.0f;

        // Tasks use shorter spring
        float targetLen = (n1.type == NodeType::TASK || n2.type == NodeType::TASK) 
                          ? springLengthTask : springLengthInsight;
        
        float force = (dist - targetLen) * springK;

        float fx = (dx / dist) * force;
        float fy = (dy / dist) * force;

        forces[link.startNode].first += fx;
        forces[link.startNode].second += fy;
        forces[link.endNode].first -= fx;
        forces[link.endNode].second -= fy;
    }

    // 3. Center Gravity
    for (size_t i = 0; i < graphNodes.size(); ++i) {
        if (graphNodes[i].type == NodeType::TASK) continue; // Tasks follow parents, don't pull to center directly

        float dx = centerX - graphNodes[i].x;
        float dy = centerY - graphNodes[i].y;
        float dist = std::sqrt(dx*dx + dy*dy);
        
        if (dist > 10.0f) {
            float strength = 0.02f; 
            forces[i].first += (dx / dist) * dist * strength; 
            forces[i].second += (dy / dist) * dist * strength;
        }
    }

    // 4. Update Positions
    for (size_t i = 0; i < graphNodes.size(); ++i) {
        auto& node = graphNodes[i];
        
        node.vx = (node.vx + forces[i].first) * damping;
        node.vy = (node.vy + forces[i].second) * damping;

        float v = std::sqrt(node.vx*node.vx + node.vy*node.vy);
        const float maxV = 8.0f;
        if (v > maxV) {
            node.vx = (node.vx / v) * maxV;
            node.vy = (node.vy / v) * maxV;
        }

        if (selectedNodes.find(node.id) != selectedNodes.end()) {
            node.vx = 0;
            node.vy = 0;
            continue; 
        }

        node.x += node.vx;
        node.y += node.vy;

        // 5. Bounds Clamping (Keep nodes within a reasonable visible area)
        if (node.x < 50.0f) { node.x = 50.0f; node.vx *= -0.5f; }
        if (node.x > 1550.0f) { node.x = 1550.0f; node.vx *= -0.5f; }
        if (node.y < 50.0f) { node.y = 50.0f; node.vy *= -0.5f; }
        if (node.y > 850.0f) { node.y = 850.0f; node.vy *= -0.5f; }
    }
}

void AppState::CenterGraph() {
    for (auto& node : graphNodes) {
        node.x = 800.0f + (rand() % 100 - 50);
        node.y = 450.0f + (rand() % 100 - 50);
        node.vx = 0;
        node.vy = 0;
    }
}

void AppState::HandleFileDrop(const std::string& filePath) {
    if (!organizerService) {
        AppendLog("[SISTEMA] Drop ignorado: Nenhum projeto aberto.\n");
        return;
    }

    std::string pathObj = filePath;
    // Check extension
    std::string ext = "";
    size_t dot = pathObj.find_last_of('.');
    if (dot != std::string::npos) {
        ext = pathObj.substr(dot);
        std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
    }

    if (ext == ".wav" || ext == ".mp3" || ext == ".m4a") {
        if (isTranscribing.load()) {
            AppendLog("[SISTEMA] Ocupado: Transcrição já em andamento.\n");
            return;
        }

        isTranscribing.store(true);
        AppendLog("[Transcrição] Iniciada para: " + filePath + "\n");
        
        organizerService->transcribeAudio(filePath,
            [this](std::string textPath) {
                AppendLog("[Transcrição] Sucesso! Texto salvo em: " + textPath + "\n");
                isTranscribing.store(false);
                pendingRefresh.store(true);
            },
            [this](std::string error) {
                AppendLog("[Transcrição] Erro: " + error + "\n");
                isTranscribing.store(false);
            }
        );
    } else {
        AppendLog("[SISTEMA] Drop ignorado: Tipo de arquivo não suportado (" + ext + ")\n");
    }
}

std::string AppState::ExportToMermaid() const {
    std::stringstream ss;
    ss << "# IdeaWalker Neural Web - Exportação\n\n";
    ss << "```mermaid\n";
    ss << "mindmap\n";
    ss << "  root((IdeaWalker Neural Web))\n";

    // Build hierarchical map from app links
    for (const auto& node : graphNodes) {
        if (node.type == NodeType::INSIGHT) {
            ss << "    node_" << node.id << "[" << node.title << "]\n";
            
            // Find its tasks
            for (const auto& link : graphLinks) {
                if (link.startNode == node.id) {
                    const auto& targetNode = graphNodes[link.endNode];
                    if (targetNode.type == NodeType::TASK) {
                        const char* emoji = "📋 "; // TODO
                        if (targetNode.isCompleted) emoji = "✅ "; // DONE
                        else if (targetNode.isInProgress) emoji = "⏳ "; // DOING

                        ss << "      " << (targetNode.isCompleted ? " ((" : " (")
                           << emoji << targetNode.title << (targetNode.isCompleted ? ")) " : ") ") << "\n";
                    }
                }
            }
        }
    }

    ss << "```\n";
    return ss.str();
}

std::string AppState::ExportFullMarkdown() const {
    std::stringstream ss;
    ss << "# IdeaWalker - Exportação da Base de Conhecimento\n";
    ss << "Data: " << __DATE__ << "\n\n";

    ss << "## 🕸️ Neural Web (Fluxograma Mermaid)\n\n";
    ss << "```mermaid\n";
    ss << "graph TD\n";
    // Add all nodes
    for (const auto& node : graphNodes) {
        if (node.type == NodeType::INSIGHT) {
            ss << "  N" << node.id << "[" << node.title << "]\n";
        }
    }
    // Add cross-links
    for (const auto& link : graphLinks) {
        const auto& start = graphNodes[link.startNode];
        const auto& end = graphNodes[link.endNode];
        if (start.type == NodeType::INSIGHT && end.type == NodeType::INSIGHT) {
            ss << "  N" << link.startNode << " --> N" << link.endNode << "\n";
        }
    }
    ss << "```\n\n";

    ss << "## 🧠 Mapa Mental (Tarefas e Ideias)\n\n";
    ss << ExportToMermaid() << "\n\n";

    ss << "## 📝 Conteúdo dos Documentos\n\n";
    for (const auto& insight : allInsights) {
        ss << "### " << (insight.getMetadata().title.empty() ? insight.getMetadata().id : insight.getMetadata().title) << "\n";
        ss << "ID: `" << insight.getMetadata().id << "`\n\n";
        ss << insight.getContent() << "\n\n";
        ss << "---\n\n";
    }

    return ss.str();
}

void AppState::OpenExternalFile(const std::string& path) {
    std::ifstream file(path);
    if (!file.is_open()) {
        AppendLog("[Erro] Não foi possível abrir o arquivo: " + path + "\n");
        return;
    }
    std::stringstream buffer;
    buffer << file.rdbuf();
    
    ExternalFile extFile;
    extFile.path = path;
    extFile.filename = std::filesystem::path(path).filename().string();
    extFile.content = buffer.str();
    extFile.modified = false;
    
    // Check if already open
    for (size_t i = 0; i < externalFiles.size(); ++i) {
        if (externalFiles[i].path == path) {
            selectedExternalFileIndex = i;
            // Update content just in case
            externalFiles[i].content = buffer.str();
            return;
        }
    }

    externalFiles.push_back(extFile);
    selectedExternalFileIndex = externalFiles.size() - 1;
    AppendLog("[Sistema] Arquivo externo aberto: " + path + "\n");
    
    // Switch to External tab (index 4)
    requestedTab = 4; 
}

void AppState::SaveExternalFile(int index) {
    if (index < 0 || index >= static_cast<int>(externalFiles.size())) return;
    
    auto& extFile = externalFiles[index];
    std::ofstream file(extFile.path);
    if (file.is_open()) {
        file << extFile.content;
        extFile.modified = false;
        AppendLog("[Sistema] Arquivo externo salvo: " + extFile.path + "\n");
    } else {
        AppendLog("[Erro] Não foi possível salvar o arquivo: " + extFile.path + "\n");
    }
}

} // namespace ideawalker::ui
