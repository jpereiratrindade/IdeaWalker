/**
 * @file AppController.cpp
 * @brief Implementation of AppController for linking UI to Services.
 */

#include "application/AppController.hpp"

#include <algorithm>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <regex>
#include <sstream>
#include "infrastructure/ConfigLoader.hpp"
#include <nlohmann/json.hpp>

#include "imnodes.h"

#ifndef IDEAWALKER_VERSION
#define IDEAWALKER_VERSION "Unknown" // Fallback if missing
#endif

namespace ideawalker::application {

AppController::AppController() {
    m_viewModel.ui.outputLog = std::string("Idea Walker ") + IDEAWALKER_VERSION + " - Núcleo DDD inicializado.\n";
}

AppController::~AppController() {
    ShutdownImNodes();
}

void AppController::InitImNodes() {
    if (!m_viewModel.neuralWeb.mainContext) {
        m_viewModel.neuralWeb.mainContext = ImNodes::EditorContextCreate();
        ImNodes::EditorContextSet((ImNodesEditorContext*)m_viewModel.neuralWeb.mainContext);
        ImNodes::PushAttributeFlag(ImNodesAttributeFlags_EnableLinkDetachWithDragClick);
    }
    
    if (!m_viewModel.neuralWeb.previewContext) {
        m_viewModel.neuralWeb.previewContext = ImNodes::EditorContextCreate();
    }
}

void AppController::ShutdownImNodes() {
    if (m_viewModel.neuralWeb.mainContext) {
        ImNodes::EditorContextFree((ImNodesEditorContext*)m_viewModel.neuralWeb.mainContext);
        m_viewModel.neuralWeb.mainContext = nullptr;
    }
    if (m_viewModel.neuralWeb.previewContext) {
        ImNodes::EditorContextFree((ImNodesEditorContext*)m_viewModel.neuralWeb.previewContext);
        m_viewModel.neuralWeb.previewContext = nullptr;
    }
}

bool AppController::NewProject(const std::string& rootPath) {
    return OpenProject(rootPath);
}

bool AppController::OpenProject(const std::string& rootPath) {
    if (rootPath.empty()) return false;
    
    std::filesystem::path root(rootPath);
    if (m_services.projectService) {
        if (!m_services.projectService->EnsureProjectFolders(root)) {
            return false;
        }
    } else {
        // Fallback for early initialization before services are injected
        try {
            std::filesystem::create_directories(root / "inbox");
            std::filesystem::create_directories(root / "inbox" / "scientific");
            std::filesystem::create_directories(root / "notas");
            std::filesystem::create_directories(root / ".history");
            std::filesystem::create_directories(root / "observations" / "scientific");
            std::filesystem::create_directories(root / "strata" / "consumables");
        } catch (...) {
            if (!std::filesystem::exists(root)) return false;
        }
    }

    m_viewModel.project.root = root.string();
    std::snprintf(m_viewModel.project.pathBuffer, sizeof(m_viewModel.project.pathBuffer), "%s", m_viewModel.project.root.c_str());

    m_viewModel.ui.selectedInboxFilename.clear();
    m_viewModel.ui.selectedFilename.clear();
    m_viewModel.ui.selectedNoteContent.clear();
    m_viewModel.ui.unifiedKnowledge.clear();
    m_viewModel.project.consolidatedInsight.reset();
    m_viewModel.ui.currentBacklinks.clear();

    m_viewModel.AppendLog("[SISTEMA] Pasta de projeto definida: " + m_viewModel.project.root + "\n");

    const bool hasServices =
        m_services.knowledgeService || m_services.aiProcessingService || m_services.conversationService || m_services.persistenceService;
    if (servicesFactory && hasServices) {
        InjectServices(servicesFactory(m_viewModel.project.root));
    }
    return true;
}

void AppController::InjectServices(application::AppServices&& newServices) {
    m_services = std::move(newServices);
    
    RefreshInbox();
    RefreshAllInsights();
    RefreshDialogueList();
    LoadConfig(); // Load project-specific settings
    AnalyzeSuggestions();
    m_viewModel.AppendLog("[SISTEMA] Serviços injetados e inicializados.\n");
}

bool AppController::SaveProject() {
    if (m_viewModel.project.root.empty() || !m_services.projectService) return false;
    
    if (!m_services.projectService->EnsureProjectFolders(std::filesystem::path(m_viewModel.project.root))) {
        return false;
    }
    m_viewModel.AppendLog("[SISTEMA] Projeto salvo: " + m_viewModel.project.root + "\n");
    return true;
}

bool AppController::SaveProjectAs(const std::string& rootPath) {
    if (rootPath.empty() || !m_services.projectService) return false;

    std::filesystem::path newRoot(rootPath);
    if (m_viewModel.project.root.empty()) {
        return OpenProject(newRoot.string());
    }
    
    std::filesystem::path currentRoot(m_viewModel.project.root);
    if (currentRoot == newRoot) {
        return SaveProject();
    }
    
    if (!m_services.projectService->CopyProjectData(currentRoot, newRoot)) {
        return false;
    }
    return OpenProject(newRoot.string());
}

bool AppController::CloseProject() {
    m_services.knowledgeService.reset();
    m_services.aiProcessingService.reset();
    m_services.conversationService.reset();
    m_services.contextAssembler.reset();
    m_services.ingestionService.reset();
    m_services.scientificIngestionService.reset();
    m_services.suggestionService.reset();
    m_services.writingTrajectoryService.reset();
    m_services.persistenceService.reset();

    m_viewModel.project.root.clear();
    m_viewModel.project.pathBuffer[0] = '\0';
    m_viewModel.ui.selectedInboxFilename.clear();
    m_viewModel.ui.selectedFilename.clear();
    m_viewModel.ui.selectedNoteContent.clear();
    m_viewModel.ui.unifiedKnowledge.clear();
    m_viewModel.project.consolidatedInsight.reset();
    m_viewModel.ui.currentBacklinks.clear();
    m_viewModel.project.inboxThoughts.clear();
    m_viewModel.project.allInsights.clear();
    m_viewModel.project.activityHistory.clear();
    m_viewModel.project.currentInsight.reset();
    m_viewModel.AppendLog("[SISTEMA] Projeto fechado.\n");
    return true;
}

void AppController::RefreshInbox() {
    if (m_services.knowledgeService) {
        m_viewModel.project.inboxThoughts = m_services.knowledgeService->GetRawThoughts();
    }
    RefreshDialogueList();
}

void AppController::RefreshDialogueList() {
    if (m_services.conversationService) {
        m_viewModel.ui.dialogueFiles = m_services.conversationService->listDialogues();
    }
}

void AppController::AnalyzeSuggestions() {
    if (!m_services.suggestionService || !m_services.knowledgeService || m_viewModel.ui.isAnalyzingSuggestions) return;
    if (!m_services.taskManager) return;

    m_viewModel.ui.isAnalyzingSuggestions = true;
    m_services.taskManager->SubmitTask(
        application::TaskType::Indexing,
        "Análise de Sugestões Semânticas",
        [this](auto /*status*/) {
            m_services.suggestionService->indexProject(m_viewModel.project.allInsights);

            if (!m_viewModel.ui.selectedFilename.empty() && !m_viewModel.ui.selectedNoteContent.empty()) {
                auto suggestions = m_services.suggestionService->generateSemanticSuggestions(
                    m_viewModel.ui.selectedFilename, m_viewModel.ui.selectedNoteContent);
                std::lock_guard<std::mutex> lock(m_viewModel.ui.suggestionsMutex);
                m_viewModel.ui.currentSuggestions = std::move(suggestions);
            }

            m_viewModel.ui.isAnalyzingSuggestions = false;
            m_viewModel.ui.pendingRefresh = true;
        });
}

void AppController::RefreshAllInsights() {
    if (m_services.knowledgeService) {
        auto insights = m_services.knowledgeService->GetAllInsights();
        m_viewModel.project.allInsights.clear();
        m_viewModel.project.consolidatedInsight.reset();
        for (auto& insight : insights) {
            if (insight.getMetadata().id == "_Consolidated_Tasks.md") {
                m_viewModel.project.consolidatedInsight = std::make_unique<domain::Insight>(insight);
            } else {
                m_viewModel.project.allInsights.push_back(std::move(insight));
            }
        }
        m_viewModel.project.activityHistory = m_services.knowledgeService->GetActivityHistory();
        std::sort(m_viewModel.project.allInsights.begin(), m_viewModel.project.allInsights.end(), [](const domain::Insight& a, const domain::Insight& b) {
            return a.getMetadata().id < b.getMetadata().id;
        });

        std::ostringstream combined;
        for (size_t i = 0; i < m_viewModel.project.allInsights.size(); ++i) {
            const auto& insight = m_viewModel.project.allInsights[i];
            combined << "## " << insight.getMetadata().id << "\n\n";
            combined << insight.getContent();
            if (i + 1 < m_viewModel.project.allInsights.size()) {
                combined << "\n\n---\n\n";
            }
        }
        m_viewModel.ui.unifiedKnowledge = combined.str();
        
        RebuildGraph();
    }
}

void AppController::LoadHistory(const std::string& noteId) {
    m_viewModel.ui.selectedNoteIdForHistory = noteId;
    m_viewModel.ui.selectedHistoryIndex = -1;
    m_viewModel.ui.selectedHistoryContent.clear();
    m_viewModel.ui.historyVersions.clear();
    if (m_services.knowledgeService) {
        m_viewModel.ui.historyVersions = m_services.knowledgeService->GetNoteHistory(noteId);
    }
}

void AppController::CheckForUpdates() {
    if (m_viewModel.ui.isCheckingUpdates.exchange(true)) return;
    if (!m_services.taskManager) {
        m_viewModel.ui.isCheckingUpdates.store(false);
        return;
    }

    m_services.taskManager->SubmitTask(
        application::TaskType::UpdateCheck,
        "Verificação de Atualizações",
        [this](auto /*status*/) {
            std::string command = "curl -s https://api.github.com/repos/jpereiratrindade/IdeaWalker/releases/latest";

            FILE* pipe = popen(command.c_str(), "r");
            if (pipe) {
                char buffer[128];
                std::string result;
                while (fgets(buffer, sizeof(buffer), pipe) != NULL) {
                    result += buffer;
                }
                pclose(pipe);

                try {
                    auto j = nlohmann::json::parse(result);
                    if (j.contains("tag_name")) {
                        m_viewModel.ui.latestVersion = j["tag_name"];
                        m_viewModel.ui.updateAvailable = (m_viewModel.ui.latestVersion != IDEAWALKER_VERSION);
                    }
                } catch (...) {
                    m_viewModel.AppendLog("[Sistema] Erro ao verificar atualizações (JSON inválido).\n");
                }
            } else {
                m_viewModel.AppendLog("[Sistema] Erro ao verificar atualizações (curl falhou).\n");
            }

            m_viewModel.ui.isCheckingUpdates.store(false);
            m_viewModel.ui.showUpdate = true;
        });
}

void AppController::RebuildGraph() {
    if (!m_services.graphService) return;
    m_services.graphService->RebuildGraph(m_viewModel.project.allInsights, m_viewModel.neuralWeb.showTasks, m_viewModel.neuralWeb.nodes, m_viewModel.neuralWeb.links);
    m_viewModel.neuralWeb.initialized = false; 
}

void AppController::UpdateGraphPhysics(const std::unordered_set<int>& selectedNodes) {
    if (!m_services.graphService) return;
    m_services.graphService->UpdatePhysics(m_viewModel.neuralWeb.nodes, m_viewModel.neuralWeb.links, selectedNodes);
}

void AppController::CenterGraph() {
    if (!m_services.graphService) return;
    m_services.graphService->CenterGraph(m_viewModel.neuralWeb.nodes);
}

void AppController::HandleFileDrop(const std::string& filePath) {
    if (!m_services.aiProcessingService) {
        m_viewModel.AppendLog("[SISTEMA] Drop ignorado: Nenhum projeto aberto ou serviço de IA indisponível.\n");
        return;
    }
    RequestTranscription(filePath);
}

void AppController::RequestTranscription(const std::string& filePath) {
    std::string pathObj = filePath;
    std::string ext = "";
    size_t dot = pathObj.find_last_of('.');
    if (dot != std::string::npos) {
        ext = pathObj.substr(dot);
        std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
    }

    if (ext == ".wav" || ext == ".mp3" || ext == ".m4a" || ext == ".ogg" || ext == ".flac") {
        if (m_viewModel.ui.isTranscribing.load()) {
            m_viewModel.AppendLog("[SISTEMA] Ocupado: Transcrição já em andamento.\n");
            return;
        }

        m_viewModel.ui.isTranscribing.store(true);
        m_viewModel.AppendLog("[Transcrição] Iniciada para: " + filePath + "\n");
        
        if (m_services.aiProcessingService) {
             m_services.aiProcessingService->TranscribeAudioAsync(filePath);
             m_viewModel.AppendLog("[Transcrição] Solicitada em segundo plano.\n");
        }
    } else {
        m_viewModel.AppendLog("[SISTEMA] Arquivo não suportado para transcrição: " + ext + "\n");
    }
}

std::string AppController::ExportToMermaid() const {
    if (!m_services.exportService) return "";
    return m_services.exportService->ToMermaidMindmap(m_viewModel.neuralWeb.nodes, m_viewModel.neuralWeb.links);
}

std::string AppController::ExportFullMarkdown() const {
    if (!m_services.exportService) return "";
    return m_services.exportService->ToFullMarkdown(m_viewModel.project.allInsights, m_viewModel.neuralWeb.nodes, m_viewModel.neuralWeb.links);
}

void AppController::OpenExternalFile(const std::string& path) {
    std::ifstream file(path);
    if (!file.is_open()) {
        m_viewModel.AppendLog("[Erro] Não foi possível abrir o arquivo: " + path + "\n");
        return;
    }
    std::stringstream buffer;
    buffer << file.rdbuf();
    
    ui::ExternalFile extFile;
    extFile.path = path;
    extFile.filename = std::filesystem::path(path).filename().string();
    extFile.content = buffer.str();
    extFile.modified = false;
    
    // Check if already open
    for (size_t i = 0; i < m_viewModel.external.files.size(); ++i) {
        if (m_viewModel.external.files[i].path == path) {
            m_viewModel.external.selectedIndex = i;
            m_viewModel.external.files[i].content = buffer.str();
            return;
        }
    }

    m_viewModel.external.files.push_back(extFile);
    m_viewModel.external.selectedIndex = m_viewModel.external.files.size() - 1;
    m_viewModel.AppendLog("[Sistema] Arquivo externo aberto: " + path + "\n");
    
    m_viewModel.ui.requestedTab = 4; 
}

void AppController::SaveExternalFile(int index) {
    if (index < 0 || index >= static_cast<int>(m_viewModel.external.files.size())) return;
    
    auto& extFile = m_viewModel.external.files[index];
    std::ofstream file(extFile.path);
    if (file.is_open()) {
        file << extFile.content;
        extFile.modified = false;
        m_viewModel.AppendLog("[Sistema] Arquivo externo salvo: " + extFile.path + "\n");
    } else {
        m_viewModel.AppendLog("[Erro] Não foi possível salvar o arquivo: " + extFile.path + "\n");
    }
}

void AppController::LoadConfig() {
    if (m_viewModel.project.root.empty()) return;

    std::filesystem::path configPath = std::filesystem::path(m_viewModel.project.root) / "settings.json";
    if (!std::filesystem::exists(configPath)) return;

    try {
        std::ifstream f(configPath);
        nlohmann::json j;
        f >> j;

        if (j.contains("ai_model")) {
            std::string model = j["ai_model"];
            if (m_services.aiProcessingService && m_services.aiProcessingService->GetAI()) {
                 m_services.aiProcessingService->GetAI()->setModel(model);
                 m_viewModel.AppendLog("[Config] Modelo restaurado (" + configPath.string() + "): " + model + "\n");
            }
        } else {
             m_viewModel.AppendLog("[Config] Nenhuma preferencia de IA encontrada em " + configPath.string() + ". Mantendo atual.\n");
        }
        
        if (j.contains("video_driver")) {
            m_viewModel.project.videoDriverPreference = j["video_driver"];
            m_viewModel.AppendLog("[Config] Driver de vídeo preferencial: " + m_viewModel.project.videoDriverPreference + " (Requer reinício para aplicar)\n");
        }
    } catch (...) {
        m_viewModel.AppendLog("[Config] Erro ao carregar settings.json\n");
    }
}

void AppController::SaveConfig() {
    if (m_viewModel.project.root.empty()) return;

    if (!m_viewModel.project.videoDriverPreference.empty()) {
        infrastructure::ConfigLoader::SaveVideoDriverPreference(m_viewModel.project.root, m_viewModel.project.videoDriverPreference);
    }

    if (m_services.aiProcessingService && m_services.aiProcessingService->GetAI()) {
        infrastructure::ConfigLoader::SaveAIModelPreference(m_viewModel.project.root, m_services.aiProcessingService->GetAI()->getCurrentModel());
    }
}

void AppController::SetAIModel(const std::string& modelName) {
    m_viewModel.project.currentAIModel = modelName;
    if (m_services.aiProcessingService && m_services.aiProcessingService->GetAI()) {
        m_services.aiProcessingService->GetAI()->setModel(modelName);
    }
    SaveConfig();
}

} // namespace ideawalker::application
