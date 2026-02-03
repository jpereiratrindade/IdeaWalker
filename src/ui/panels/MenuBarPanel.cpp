#include "ui/panels/MainPanels.hpp"
#include "ui/UiUtils.hpp"
#include "ui/UiFileBrowser.hpp"
#include "application/OrganizerService.hpp"
#include "imgui.h"
#include <vector>
#include <string>
#include <chrono>
#include <filesystem>

namespace ideawalker::ui {

void DrawMenuBar(AppState& app) {
    auto label = [&app](const char* withEmoji, const char* plain) {
        return app.emojiEnabled ? withEmoji : plain;
    };
    const bool hasProject = (app.services.organizerService != nullptr);
    const bool canChangeProject = !app.isProcessing.load();

    if (ImGui::BeginMenuBar()) {
        if (ImGui::BeginMenu(label("📁 Arquivo", "Arquivo"))) {
            if (ImGui::MenuItem(label("🆕 Novo Projeto...", "Novo Projeto..."), nullptr, false, canChangeProject)) {
                app.showNewProjectModal = true;
            }
            if (ImGui::MenuItem(label("📂 Abrir Projeto...", "Abrir Projeto..."), nullptr, false, canChangeProject)) {
                app.showOpenProjectModal = true;
            }
            if (ImGui::MenuItem(label("💾 Salvar Projeto Como...", "Salvar Projeto Como..."), nullptr, false, hasProject)) {
                app.showSaveAsProjectModal = true;
            }
            ImGui::Separator();
            if (ImGui::MenuItem(label("📄 Abrir Arquivo Externo...", "Abrir Arquivo Externo..."), nullptr, false, true)) {
                app.showOpenFileModal = true;
                app.openFilePathBuffer[0] = '\0';
            }
            if (ImGui::MenuItem(label("🎙️ Transcrever Áudio...", "Transcrever Áudio..."), nullptr, false, hasProject)) {
                app.showTranscriptionModal = true;
                app.transcriptionPathBuffer[0] = '\0'; 
            }
            ImGui::Separator();
            if (ImGui::MenuItem("Fechar Arquivo", nullptr, false, app.selectedExternalFileIndex != -1)) {
                if (app.selectedExternalFileIndex >= 0 && app.selectedExternalFileIndex < (int)app.externalFiles.size()) {
                    app.externalFiles.erase(app.externalFiles.begin() + app.selectedExternalFileIndex);
                    if (app.selectedExternalFileIndex >= (int)app.externalFiles.size()) {
                        app.selectedExternalFileIndex = (int)app.externalFiles.size() - 1;
                    }
                }
            }
            ImGui::Separator();
            if (ImGui::MenuItem("Sair", nullptr, false, canChangeProject)) {
                app.requestExit = true;
            }
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("View")) {
            if (ImGui::MenuItem("Writing Trajectories", nullptr, app.showTrajectoryPanel)) {
                 app.showTrajectoryPanel = !app.showTrajectoryPanel;
            }
             if (ImGui::MenuItem("Defense Mode", nullptr, app.showDefensePanel)) {
                 app.showDefensePanel = !app.showDefensePanel;
            }
            if (ImGui::MenuItem("Segment Editor", nullptr, app.showSegmentEditor)) {
                 app.showSegmentEditor = !app.showSegmentEditor;
            }
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu(label("⚙️ Configurações", "Configurações"))) {
             if (ImGui::MenuItem("Preferências...", nullptr, false, true)) {
                 app.showSettingsModal = true;
             }
             
             if (hasProject && app.services.organizerService->getAI()) {
                 if (ImGui::BeginMenu(label("🧠 Selecionar Modelo de IA", "Selecionar Modelo de IA"))) {
                     auto currentModel = app.services.organizerService->getAI()->getCurrentModel();
                     static std::vector<std::string> cachedModels;
                     static std::chrono::steady_clock::time_point lastCheck;
                     auto now = std::chrono::steady_clock::now();
                     
                     if (cachedModels.empty() || std::chrono::duration_cast<std::chrono::seconds>(now - lastCheck).count() > 10) {
                         cachedModels = app.services.organizerService->getAI()->getAvailableModels();
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
                if (ImGui::MenuItem(label("🕸️ Mostrar Tarefas", "Mostrar Tarefas"), nullptr, app.showTasksInGraph)) {
                    app.showTasksInGraph = !app.showTasksInGraph;
                    app.RebuildGraph();
                }
                if (ImGui::MenuItem(label("🔄 Animação", "Animação"), nullptr, app.physicsEnabled)) {
                    app.physicsEnabled = !app.physicsEnabled;
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
                app.showHelpModal = true;
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
