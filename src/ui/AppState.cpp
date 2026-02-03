/**
 * @file AppState.cpp
 * @brief Implementation of the AppState class and state management logic.
 */
#include "ui/AppState.hpp"

#include <algorithm>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <regex>
#include <sstream>
#include <thread>
#include "infrastructure/ConfigLoader.hpp"
#include <nlohmann/json.hpp>

#include "imnodes.h"

namespace ideawalker::ui {

namespace {

// Helper functions moved to application::ProjectService
} // namespace

AppState::AppState()
    : outputLog("Idea Walker v0.1.5-beta - Núcleo DDD inicializado.\n") {
    saveAsFilename[0] = '\0';
    projectPathBuffer[0] = '\0';

    // Initialize ImNodes contexts in InitImNodes() instead
}

AppState::~AppState() {
    ShutdownImNodes();
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
    if (rootPath.empty() || !services.projectService) return false;
    
    std::filesystem::path root(rootPath);
    if (!services.projectService->EnsureProjectFolders(root)) {
        return false;
    }

    projectRoot = root.string();
    std::snprintf(projectPathBuffer, sizeof(projectPathBuffer), "%s", projectRoot.c_str());

    selectedInboxFilename.clear();
    selectedFilename.clear();
    selectedNoteContent.clear();
    unifiedKnowledge.clear();
    consolidatedInsight.reset();
    currentBacklinks.clear();

    AppendLog("[SISTEMA] Pasta de projeto definida: " + projectRoot + "\n");
    return true;
}

void AppState::InjectServices(application::AppServices&& newServices) {
    services = std::move(newServices);
    
    RefreshInbox();
    RefreshAllInsights();
    RefreshDialogueList();
    LoadConfig(); // Load project-specific settings
    AnalyzeSuggestions();
    AppendLog("[SISTEMA] Serviços injetados e inicializados.\n");
}

bool AppState::SaveProject() {
    if (projectRoot.empty() || !services.projectService) return false;
    
    if (!services.projectService->EnsureProjectFolders(std::filesystem::path(projectRoot))) {
        return false;
    }
    AppendLog("[SISTEMA] Projeto salvo: " + projectRoot + "\n");
    return true;
}

bool AppState::SaveProjectAs(const std::string& rootPath) {
    if (rootPath.empty() || !services.projectService) return false;

    std::filesystem::path newRoot(rootPath);
    if (projectRoot.empty()) {
        return OpenProject(newRoot.string());
    }
    
    std::filesystem::path currentRoot(projectRoot);
    if (currentRoot == newRoot) {
        return SaveProject();
    }
    
    if (!services.projectService->CopyProjectData(currentRoot, newRoot)) {
        return false;
    }
    return OpenProject(newRoot.string());
}

bool AppState::CloseProject() {
    services.organizerService.reset();
    services.conversationService.reset();
    services.contextAssembler.reset();
    services.ingestionService.reset();
    services.suggestionService.reset();
    services.writingTrajectoryService.reset();
    services.persistenceService.reset();

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
    if (services.organizerService) {
        inboxThoughts = services.organizerService->getRawThoughts();
    }
    RefreshDialogueList();
}

void AppState::RefreshDialogueList() {
    if (services.conversationService) {
        dialogueFiles = services.conversationService->listDialogues();
    }
}

void AppState::AnalyzeSuggestions() {
    if (!services.suggestionService || !services.organizerService || isAnalyzingSuggestions) return;

    isAnalyzingSuggestions = true;
    std::thread([this]() {
        services.suggestionService->indexProject(allInsights);
        
        if (!selectedFilename.empty() && !selectedNoteContent.empty()) {
            auto suggestions = services.suggestionService->generateSemanticSuggestions(selectedFilename, selectedNoteContent);
            std::lock_guard<std::mutex> lock(suggestionsMutex);
            currentSuggestions = std::move(suggestions);
        }
        
        isAnalyzingSuggestions = false;
        pendingRefresh = true; 
    }).detach();
}

void AppState::RefreshAllInsights() {
    if (services.organizerService) {
        auto insights = services.organizerService->getAllInsights();
        allInsights.clear();
        consolidatedInsight.reset();
        for (auto& insight : insights) {
            if (insight.getMetadata().id == application::OrganizerService::kConsolidatedTasksFilename) {
                consolidatedInsight = std::make_unique<domain::Insight>(insight);
            } else {
                allInsights.push_back(std::move(insight));
            }
        }
        activityHistory = services.organizerService->getActivityHistory();
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

void AppState::LoadHistory(const std::string& noteId) {
    selectedNoteIdForHistory = noteId;
    selectedHistoryIndex = -1;
    selectedHistoryContent.clear();
    historyVersions.clear();
    if (services.organizerService) {
        historyVersions = services.organizerService->getNoteHistory(noteId);
    }
}

void AppState::CheckForUpdates() {
    if (isCheckingUpdates.exchange(true)) return;
    
    std::thread([this]() {
        // Fallback to curl for now to avoid OpenSSL issues in C++ build
        // curl -s https://api.github.com/repos/jpereiratrindade/IdeaWalker/releases/latest
        std::string command = "curl -s https://api.github.com/repos/jpereiratrindade/IdeaWalker/releases/latest";
        
        FILE* pipe = popen(command.c_str(), "r");
        if (pipe) {
            char buffer[128];
            std::string result = "";
            while (fgets(buffer, sizeof(buffer), pipe) != NULL) {
                result += buffer;
            }
            pclose(pipe);
            
            try {
                auto j = nlohmann::json::parse(result);
                if (j.contains("tag_name")) {
                    latestVersion = j["tag_name"];
                    // IDEAWALKER_VERSION is defined in CMakeLists.txt (e.g. "v0.1.6-beta")
                    if (latestVersion != IDEAWALKER_VERSION) {
                        updateAvailable = true;
                    } else {
                        updateAvailable = false;
                    }
                }
            } catch (...) {
                AppendLog("[Sistema] Erro ao verificar atualizações (JSON inválido).\n");
            }
        } else {
             AppendLog("[Sistema] Erro ao verificar atualizações (curl falhou).\n");
        }
        
        isCheckingUpdates.store(false);
        showUpdateModal = true; // Show results
    }).detach();
}

void AppState::RebuildGraph() {
    if (!services.graphService) return;
    services.graphService->RebuildGraph(allInsights, showTasksInGraph, graphNodes, graphLinks);
    graphInitialized = false; 
}

void AppState::UpdateGraphPhysics(const std::unordered_set<int>& selectedNodes) {
    if (!services.graphService) return;
    services.graphService->UpdatePhysics(graphNodes, graphLinks, selectedNodes);
}

void AppState::CenterGraph() {
    if (!services.graphService) return;
    services.graphService->CenterGraph(graphNodes);
}

void AppState::HandleFileDrop(const std::string& filePath) {
    if (!services.organizerService) {
        AppendLog("[SISTEMA] Drop ignorado: Nenhum projeto aberto.\n");
        return;
    }
    RequestTranscription(filePath);
}

void AppState::RequestTranscription(const std::string& filePath) {
    std::string pathObj = filePath;
    // Check extension
    std::string ext = "";
    size_t dot = pathObj.find_last_of('.');
    if (dot != std::string::npos) {
        ext = pathObj.substr(dot);
        std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
    }

    if (ext == ".wav" || ext == ".mp3" || ext == ".m4a" || ext == ".ogg" || ext == ".flac") {
        if (isTranscribing.load()) {
            AppendLog("[SISTEMA] Ocupado: Transcrição já em andamento.\n");
            return;
        }

        isTranscribing.store(true);
        AppendLog("[Transcrição] Iniciada para: " + filePath + "\n");
        
        services.organizerService->transcribeAudio(filePath,
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
        AppendLog("[SISTEMA] Arquivo não suportado para transcrição: " + ext + "\n");
    }
}

std::string AppState::ExportToMermaid() const {
    if (!services.exportService) return "";
    return services.exportService->ToMermaidMindmap(graphNodes, graphLinks);
}

std::string AppState::ExportFullMarkdown() const {
    if (!services.exportService) return "";
    return services.exportService->ToFullMarkdown(allInsights, graphNodes, graphLinks);
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

void AppState::LoadConfig() {
    if (projectRoot.empty()) return;

    std::filesystem::path configPath = std::filesystem::path(projectRoot) / "settings.json";
    if (!std::filesystem::exists(configPath)) return;

    try {
        std::ifstream f(configPath);
        nlohmann::json j;
        f >> j;

        if (j.contains("ai_model")) {
            std::string model = j["ai_model"];
            if (services.organizerService && services.organizerService->getAI()) {
                 services.organizerService->getAI()->setModel(model);
                 AppendLog("[Config] Modelo restaurado: " + model + "\n");
            }
        }
        
        if (j.contains("video_driver")) {
            videoDriverPreference = j["video_driver"];
            AppendLog("[Config] Driver de vídeo preferencial: " + videoDriverPreference + " (Requer reinício para aplicar)\n");
        }
    } catch (...) {
        AppendLog("[Config] Erro ao carregar settings.json\n");
    }
}

void AppState::SaveConfig() {
    if (projectRoot.empty()) return;

    // Persist video_driver using ConfigLoader
    if (!videoDriverPreference.empty()) {
        infrastructure::ConfigLoader::SaveVideoDriverPreference(projectRoot, videoDriverPreference);
    }

    // Persist AI Model (merging with existing file)
    std::filesystem::path configPath = std::filesystem::path(projectRoot) / "settings.json";
    nlohmann::json j;

    if (std::filesystem::exists(configPath)) {
        try {
            std::ifstream f(configPath);
            f >> j;
        } catch (...) {}
    }

    if (services.organizerService && services.organizerService->getAI()) {
        j["ai_model"] = services.organizerService->getAI()->getCurrentModel();
    }
    
    try {
        std::ofstream f(configPath);
        f << j.dump(4);
    } catch (const std::exception& e) {
        AppendLog(std::string("[AppState] Erro ao salvar config: ") + e.what() + "\n");
    }
}

void AppState::SetAIModel(const std::string& modelName) {
    currentAIModel = modelName;
    if (services.organizerService) {
        services.organizerService->getAI()->setModel(modelName);
    }
    SaveConfig();
}

} // namespace ideawalker::ui
