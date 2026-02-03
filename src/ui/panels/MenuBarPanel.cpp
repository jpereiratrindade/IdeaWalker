#include "ui/panels/MainPanels.hpp"
#include "ui/UiUtils.hpp"
#include "ui/UiFileBrowser.hpp"
#include "application/KnowledgeService.hpp"
#include "application/AIProcessingService.hpp"
#include "imgui.h"
#include <vector>
#include <string>
#include <chrono>
#include <filesystem>

namespace ideawalker::ui {

void DrawMenuBar(AppState& app) {
    auto label = [&app](const char* withEmoji, const char* plain) {
        return app.ui.emojiEnabled ? withEmoji : plain;
    };
    const bool hasProject = (app.services.knowledgeService != nullptr);
    const bool canChangeProject = !app.ui.isProcessing.load();

    if (ImGui::BeginMenuBar()) {
        if (ImGui::BeginMenu(label("📁 Arquivo", "Arquivo"))) {
            if (ImGui::MenuItem(label("🆕 Novo Projeto...", "Novo Projeto..."), nullptr, false, canChangeProject)) {
                app.ui.showNewProject = true;
            }
            if (ImGui::MenuItem(label("📂 Abrir Projeto...", "Abrir Projeto..."), nullptr, false, canChangeProject)) {
                app.ui.showOpenProject = true;
            }
            if (ImGui::MenuItem(label("💾 Salvar Projeto Como...", "Salvar Projeto Como..."), nullptr, false, hasProject)) {
                app.ui.showSaveAsProject = true;
            }
            ImGui::Separator();
            if (ImGui::MenuItem(label("📄 Abrir Arquivo Externo...", "Abrir Arquivo Externo..."), nullptr, false, true)) {
                app.ui.showOpenFile = true;
                app.ui.openFilePathBuffer[0] = '\0';
            }
            if (ImGui::MenuItem(label("🎙️ Transcrever Áudio...", "Transcrever Áudio..."), nullptr, false, hasProject)) {
                app.ui.showTranscription = true;
                app.ui.transcriptionPathBuffer[0] = '\0'; 
            }
            ImGui::Separator();
            if (ImGui::MenuItem("Fechar Arquivo", nullptr, false, app.external.selectedIndex != -1)) {
                if (app.external.selectedIndex >= 0 && app.external.selectedIndex < (int)app.external.files.size()) {
                    app.external.files.erase(app.external.files.begin() + app.external.selectedIndex);
                    if (app.external.selectedIndex >= (int)app.external.files.size()) {
                        app.external.selectedIndex = (int)app.external.files.size() - 1;
                    }
                }
            }
            ImGui::Separator();
            if (ImGui::MenuItem("Sair", nullptr, false, canChangeProject)) {
                app.ui.requestExit = true;
            }
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("View")) {
            if (ImGui::MenuItem("Writing Trajectories", nullptr, app.ui.showTrajectoryPanel)) {
                 app.ui.showTrajectoryPanel = !app.ui.showTrajectoryPanel;
            }
             if (ImGui::MenuItem("Defense Mode", nullptr, app.ui.showDefensePanel)) {
                 app.ui.showDefensePanel = !app.ui.showDefensePanel;
            }
            if (ImGui::MenuItem("Segment Editor", nullptr, app.ui.showSegmentEditor)) {
                 app.ui.showSegmentEditor = !app.ui.showSegmentEditor;
            }
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu(label("⚙️ Configurações", "Configurações"))) {
             if (ImGui::MenuItem("Preferências...", nullptr, false, true)) {
                 app.ui.showSettings = true;
             }
             
             if (hasProject && app.services.aiProcessingService->GetAI()) {
                 if (ImGui::BeginMenu(label("🧠 Selecionar Modelo de IA", "Selecionar Modelo de IA"))) {
                     auto currentModel = app.services.aiProcessingService->GetAI()->getCurrentModel();
                     static std::vector<std::string> cachedModels;
                     static std::chrono::steady_clock::time_point lastCheck;
                     auto now = std::chrono::steady_clock::now();
                     
                     if (cachedModels.empty() || std::chrono::duration_cast<std::chrono::seconds>(now - lastCheck).count() > 10) {
                         cachedModels = app.services.aiProcessingService->GetAI()->getAvailableModels();
                         lastCheck = now;
                     }

                     if (cachedModels.empty()) {
                         ImGui::TextDisabled("Nenhum modelo encontrado.");
                     } else {
                         for (const auto& model : cachedModels) {
                             bool isSelected = (model == currentModel);
                             if (ImGui::MenuItem(model.c_str(), nullptr, isSelected)) {
                                 app.SetAIModel(model);
                                 app.AppendLog("[Sistema] Modelo de IA alterado (e salvo) para: " + model + "\n");
                             }
                         }
                     }
                     ImGui::Separator();
                     if (ImGui::MenuItem(label("🔄 Atualizar Lista", "Atualizar Lista"))) {
                         cachedModels.clear(); 
                     }
                     ImGui::EndMenu();
                 }
             }

             ImGui::EndMenu();
        }

        if (ImGui::BeginMenu(label("🛠️ Ferramentas", "Ferramentas"))) {
            if (ImGui::BeginMenu(label("🕸️ Configurações do Grafo", "Configurações do Grafo"))) {
                if (ImGui::MenuItem(label("🕸️ Mostrar Tarefas", "Mostrar Tarefas"), nullptr, app.neuralWeb.showTasks)) {
                    app.neuralWeb.showTasks = !app.neuralWeb.showTasks;
                    app.RebuildGraph();
                }
                if (ImGui::MenuItem(label("🔄 Animação", "Animação"), nullptr, app.neuralWeb.physicsEnabled)) {
                    app.neuralWeb.physicsEnabled = !app.neuralWeb.physicsEnabled;
                }
                ImGui::Separator();
                if (ImGui::MenuItem(label("📤 Exportar Mermaid", "Exportar Mermaid"))) {
                    std::string mermaid = app.ExportToMermaid();
                    ImGui::SetClipboardText(mermaid.c_str());
                    app.AppendLog("[Info] Mapa mental exportado para o clipboard.\n");
                }
                if (ImGui::MenuItem(label("📁 Exportar Full (.md)", "Exportar Completo (.md)"))) {
                    std::string fullMd = app.ExportFullMarkdown();
                    ImGui::SetClipboardText(fullMd.c_str());
                    app.AppendLog("[Info] Conhecimento completo exportado para o clipboard.\n");
                }
                if (ImGui::MenuItem(label("🎯 Centralizar Grafo", "Centralizar Grafo"))) {
                    app.CenterGraph();
                }
                ImGui::EndMenu();
            }
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu(label("❓ Ajuda", "Ajuda"))) {
            if (ImGui::MenuItem(label("📘 Documentação...", "Documentação..."))) {
                app.ui.showHelp = true;
            }
            if (ImGui::MenuItem(label("🔄 Verificar Atualizações", "Verificar Atualizações"))) {
                app.CheckForUpdates();
            }
            ImGui::Separator();
            ImGui::TextDisabled("Versão: %s", IDEAWALKER_VERSION);
            ImGui::EndMenu();
        }

        ImGui::EndMenuBar();
    }
}

} // namespace ideawalker::ui
