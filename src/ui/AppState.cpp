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

AppState::AppState() {
    ui.outputLog = "Idea Walker v0.1.8-beta - Núcleo DDD inicializado.\n";
 }

AppState::~AppState() {
    ShutdownImNodes();
}

void AppState::InitImNodes() {
    if (!neuralWeb.mainContext) {
        neuralWeb.mainContext = ImNodes::EditorContextCreate();
        ImNodes::EditorContextSet((ImNodesEditorContext*)neuralWeb.mainContext);
        ImNodes::PushAttributeFlag(ImNodesAttributeFlags_EnableLinkDetachWithDragClick);
    }
    
    if (!neuralWeb.previewContext) {
        neuralWeb.previewContext = ImNodes::EditorContextCreate();
    }
}

void AppState::ShutdownImNodes() {
    if (neuralWeb.mainContext) {
        ImNodes::EditorContextFree((ImNodesEditorContext*)neuralWeb.mainContext);
        neuralWeb.mainContext = nullptr;
    }
    if (neuralWeb.previewContext) {
        ImNodes::EditorContextFree((ImNodesEditorContext*)neuralWeb.previewContext);
        neuralWeb.previewContext = nullptr;
    }
}

bool AppState::NewProject(const std::string& rootPath) {
    return OpenProject(rootPath);
}

bool AppState::OpenProject(const std::string& rootPath) {
    if (rootPath.empty()) return false;
    
    std::filesystem::path root(rootPath);
    if (services.projectService) {
        if (!services.projectService->EnsureProjectFolders(root)) {
            return false;
        }
    } else {
        // Fallback for early initialization before services are injected
        try {
            std::filesystem::create_directories(root / "inbox");
            std::filesystem::create_directories(root / "notas");
            std::filesystem::create_directories(root / ".history");
        } catch (...) {
            if (!std::filesystem::exists(root)) return false;
        }
    }

    project.root = root.string();
    std::snprintf(project.pathBuffer, sizeof(project.pathBuffer), "%s", project.root.c_str());

    ui.selectedInboxFilename.clear();
    ui.selectedFilename.clear();
    ui.selectedNoteContent.clear();
    ui.unifiedKnowledge.clear();
    project.consolidatedInsight.reset();
    ui.currentBacklinks.clear();

    AppendLog("[SISTEMA] Pasta de projeto definida: " + project.root + "\n");
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
    if (project.root.empty() || !services.projectService) return false;
    
    if (!services.projectService->EnsureProjectFolders(std::filesystem::path(project.root))) {
        return false;
    }
    AppendLog("[SISTEMA] Projeto salvo: " + project.root + "\n");
    return true;
}

bool AppState::SaveProjectAs(const std::string& rootPath) {
    if (rootPath.empty() || !services.projectService) return false;

    std::filesystem::path newRoot(rootPath);
    if (project.root.empty()) {
        return OpenProject(newRoot.string());
    }
    
    std::filesystem::path currentRoot(project.root);
    if (currentRoot == newRoot) {
        return SaveProject();
    }
    
    if (!services.projectService->CopyProjectData(currentRoot, newRoot)) {
        return false;
    }
    return OpenProject(newRoot.string());
}

bool AppState::CloseProject() {
    services.knowledgeService.reset();
    services.aiProcessingService.reset();
    services.conversationService.reset();
    services.contextAssembler.reset();
    services.ingestionService.reset();
    services.suggestionService.reset();
    services.writingTrajectoryService.reset();
    services.persistenceService.reset();

    project.root.clear();
    project.pathBuffer[0] = '\0';
    ui.selectedInboxFilename.clear();
    ui.selectedFilename.clear();
    ui.selectedNoteContent.clear();
    ui.unifiedKnowledge.clear();
    project.consolidatedInsight.reset();
    ui.currentBacklinks.clear();
    project.inboxThoughts.clear();
    project.allInsights.clear();
    project.activityHistory.clear();
    project.currentInsight.reset();
    AppendLog("[SISTEMA] Projeto fechado.\n");
    return true;
}

void AppState::RefreshInbox() {
    if (services.knowledgeService) {
        project.inboxThoughts = services.knowledgeService->GetRawThoughts();
    }
    RefreshDialogueList();
}

void AppState::RefreshDialogueList() {
    if (services.conversationService) {
        ui.dialogueFiles = services.conversationService->listDialogues();
    }
}

void AppState::AnalyzeSuggestions() {
    if (!services.suggestionService || !services.knowledgeService || ui.isAnalyzingSuggestions) return;
    std::thread([this]() {
        services.suggestionService->indexProject(project.allInsights);
        
        if (!ui.selectedFilename.empty() && !ui.selectedNoteContent.empty()) {
            auto suggestions = services.suggestionService->generateSemanticSuggestions(ui.selectedFilename, ui.selectedNoteContent);
            std::lock_guard<std::mutex> lock(ui.suggestionsMutex);
            ui.currentSuggestions = std::move(suggestions);
        }
        
        ui.isAnalyzingSuggestions = false;
        ui.pendingRefresh = true; 
    }).detach();
}

void AppState::RefreshAllInsights() {
    if (services.knowledgeService) {
        auto insights = services.knowledgeService->GetAllInsights();
        project.allInsights.clear();
        project.consolidatedInsight.reset();
        for (auto& insight : insights) {
            if (insight.getMetadata().id == "_Consolidated_Tasks.md") {
                project.consolidatedInsight = std::make_unique<domain::Insight>(insight);
            } else {
                project.allInsights.push_back(std::move(insight));
            }
        }
        project.activityHistory = services.knowledgeService->GetActivityHistory();
        std::sort(project.allInsights.begin(), project.allInsights.end(), [](const domain::Insight& a, const domain::Insight& b) {
            return a.getMetadata().id < b.getMetadata().id;
        });

        std::ostringstream combined;
        for (size_t i = 0; i < project.allInsights.size(); ++i) {
            const auto& insight = project.allInsights[i];
            combined << "## " << insight.getMetadata().id << "\n\n";
            combined << insight.getContent();
            if (i + 1 < project.allInsights.size()) {
                combined << "\n\n---\n\n";
            }
        }
        ui.unifiedKnowledge = combined.str();
        
        RebuildGraph();
    }
}

void AppState::AppendLog(const std::string& line) {
    std::lock_guard<std::mutex> lock(ui.logMutex);
    ui.outputLog += line;
}

std::string AppState::GetLogSnapshot() {
    std::lock_guard<std::mutex> lock(ui.logMutex);
    return ui.outputLog;
}

void AppState::SetProcessingStatus(const std::string& status) {
    std::lock_guard<std::mutex> lock(ui.statusMutex);
    ui.processingStatus = status;
}

std::string AppState::GetProcessingStatus() {
    std::lock_guard<std::mutex> lock(ui.statusMutex);
    return ui.processingStatus;
}

void AppState::LoadHistory(const std::string& noteId) {
    ui.selectedNoteIdForHistory = noteId;
    ui.selectedHistoryIndex = -1;
    ui.selectedHistoryContent.clear();
    ui.historyVersions.clear();
    if (services.knowledgeService) {
        ui.historyVersions = services.knowledgeService->GetNoteHistory(noteId);
    }
}

void AppState::CheckForUpdates() {
    if (ui.isCheckingUpdates.exchange(true)) return;
    
    std::thread([this]() {
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
                    ui.latestVersion = j["tag_name"];
                    // IDEAWALKER_VERSION is defined in CMakeLists.txt (e.g. "v0.1.7-beta")
                    if (ui.latestVersion != IDEAWALKER_VERSION) {
                        ui.updateAvailable = true;
                    } else {
                        ui.updateAvailable = false;
                    }
                }
            } catch (...) {
                AppendLog("[Sistema] Erro ao verificar atualizações (JSON inválido).\n");
            }
        } else {
             AppendLog("[Sistema] Erro ao verificar atualizações (curl falhou).\n");
        }
        
        ui.isCheckingUpdates.store(false);
        ui.showUpdate = true; // Show results
    }).detach();
}

void AppState::RebuildGraph() {
    if (!services.graphService) return;
    services.graphService->RebuildGraph(project.allInsights, neuralWeb.showTasks, neuralWeb.nodes, neuralWeb.links);
    neuralWeb.initialized = false; 
}

void AppState::UpdateGraphPhysics(const std::unordered_set<int>& selectedNodes) {
    if (!services.graphService) return;
    services.graphService->UpdatePhysics(neuralWeb.nodes, neuralWeb.links, selectedNodes);
}

void AppState::CenterGraph() {
    if (!services.graphService) return;
    services.graphService->CenterGraph(neuralWeb.nodes);
}

void AppState::HandleFileDrop(const std::string& filePath) {
    if (!services.aiProcessingService) {
        AppendLog("[SISTEMA] Drop ignorado: Nenhum projeto aberto ou serviço de IA indisponível.\n");
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
        if (ui.isTranscribing.load()) {
            AppendLog("[SISTEMA] Ocupado: Transcrição já em andamento.\n");
            return;
        }

        ui.isTranscribing.store(true);
        AppendLog("[Transcrição] Iniciada para: " + filePath + "\n");
        
        if (services.aiProcessingService) {
             services.aiProcessingService->TranscribeAudioAsync(filePath);
             AppendLog("[Transcrição] Solicitada em segundo plano.\n");
        }
    } else {
        AppendLog("[SISTEMA] Arquivo não suportado para transcrição: " + ext + "\n");
    }
}

std::string AppState::ExportToMermaid() const {
    if (!services.exportService) return "";
    return services.exportService->ToMermaidMindmap(neuralWeb.nodes, neuralWeb.links);
}

std::string AppState::ExportFullMarkdown() const {
    if (!services.exportService) return "";
    return services.exportService->ToFullMarkdown(project.allInsights, neuralWeb.nodes, neuralWeb.links);
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
    for (size_t i = 0; i < external.files.size(); ++i) {
        if (external.files[i].path == path) {
            external.selectedIndex = i;
            // Update content just in case
            external.files[i].content = buffer.str();
            return;
        }
    }

    external.files.push_back(extFile);
    external.selectedIndex = external.files.size() - 1;
    AppendLog("[Sistema] Arquivo externo aberto: " + path + "\n");
    
    // Switch to External tab (index 4)
    ui.requestedTab = 4; 
}

void AppState::SaveExternalFile(int index) {
    if (index < 0 || index >= static_cast<int>(external.files.size())) return;
    
    auto& extFile = external.files[index];
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
    if (project.root.empty()) return;

    std::filesystem::path configPath = std::filesystem::path(project.root) / "settings.json";
    if (!std::filesystem::exists(configPath)) return;

    try {
        std::ifstream f(configPath);
        nlohmann::json j;
        f >> j;

        if (j.contains("ai_model")) {
            std::string model = j["ai_model"];
            if (services.aiProcessingService && services.aiProcessingService->GetAI()) {
                 services.aiProcessingService->GetAI()->setModel(model);
                 AppendLog("[Config] Modelo restaurado: " + model + "\n");
            }
        }
        
        if (j.contains("video_driver")) {
            project.videoDriverPreference = j["video_driver"];
            AppendLog("[Config] Driver de vídeo preferencial: " + project.videoDriverPreference + " (Requer reinício para aplicar)\n");
        }
    } catch (...) {
        AppendLog("[Config] Erro ao carregar settings.json\n");
    }
}

void AppState::SaveConfig() {
    if (project.root.empty()) return;

    // Persist video_driver using ConfigLoader
    if (!project.videoDriverPreference.empty()) {
        infrastructure::ConfigLoader::SaveVideoDriverPreference(project.root, project.videoDriverPreference);
    }

    // Persist AI Model (merging with existing file)
    std::filesystem::path configPath = std::filesystem::path(project.root) / "settings.json";
    nlohmann::json j;

    if (std::filesystem::exists(configPath)) {
        try {
            std::ifstream f(configPath);
            f >> j;
        } catch (...) {}
    }

    if (services.aiProcessingService && services.aiProcessingService->GetAI()) {
        j["ai_model"] = services.aiProcessingService->GetAI()->getCurrentModel();
    }
    
    try {
        std::ofstream f(configPath);
        f << j.dump(4);
    } catch (const std::exception& e) {
        AppendLog(std::string("[AppState] Erro ao salvar config: ") + e.what() + "\n");
    }
}

void AppState::SetAIModel(const std::string& modelName) {
    project.currentAIModel = modelName;
    if (services.aiProcessingService && services.aiProcessingService->GetAI()) {
        services.aiProcessingService->GetAI()->setModel(modelName);
    }
    SaveConfig();
}

} // namespace ideawalker::ui
