#include "ui/panels/MainPanels.hpp"
#include "ui/UiUtils.hpp"
#include "ui/UiMarkdownRenderer.hpp"
#include "ui/UiFileBrowser.hpp"
#include "application/OrganizerService.hpp"
#include "imgui.h"
#include <cstdio>
#include <cstring>
#include <fstream>
#include <sstream>
#include <ctime>
#include <chrono>

namespace ideawalker::ui {

void DrawTranscriptionModal(AppState& app) {
    if (app.showTranscriptionModal) {
        ImGui::OpenPopup("Transcrever Áudio");
    }
    if (ImGui::BeginPopupModal("Transcrever Áudio", &app.showTranscriptionModal, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::Text("Digite o caminho absoluto do arquivo de áudio:");
        ImGui::InputText("Caminho", app.transcriptionPathBuffer, sizeof(app.transcriptionPathBuffer));
        ImGui::TextDisabled("Suporta: .wav, .mp3, .m4a, .ogg, .flac");
        
        ImGui::Separator();
        
        if (ImGui::Button("Transcrever", ImVec2(120, 0))) {
            if (strlen(app.transcriptionPathBuffer) > 0) {
                app.RequestTranscription(app.transcriptionPathBuffer);
                app.showTranscriptionModal = false;
            }
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancelar", ImVec2(120, 0))) {
            app.showTranscriptionModal = false;
        }
        
        ImGui::EndPopup();
    }
}

void DrawSettingsModal(AppState& app) {
    auto label = [&app](const char* withEmoji, const char* plain) {
        return app.emojiEnabled ? withEmoji : plain;
    };
    if (app.showSettingsModal) {
        ImGui::OpenPopup("Preferências");
    }
    if (ImGui::BeginPopupModal("Preferências", &app.showSettingsModal, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::Text("Configurações Gerais");
        ImGui::Separator();
        
        ImGui::Text("%s", label("🧠 Personalidade da IA", "Personalidade da IA"));
        ImGui::TextDisabled("O sistema seleciona automaticamente o melhor perfil cognitivo.");
        
        ImGui::Spacing();
        ImGui::Checkbox(label("⚡ Modo Rápido (CPU Optimization)", "Modo Rápido (CPU Optimization)"), &app.fastMode);
        if (ImGui::IsItemHovered()) {
             ImGui::SetTooltip("Ignora orquestração e faz análise direta. Recomendado para CPUs antigas.");
        }

        ImGui::Separator();
        ImGui::Dummy(ImVec2(0, 10));
        
        if (ImGui::Button("Fechar", ImVec2(120, 0))) {
            app.showSettingsModal = false;
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }
}

void DrawProjectModals(AppState& app) {
    auto label = [&app](const char* withEmoji, const char* plain) {
        return app.emojiEnabled ? withEmoji : plain;
    };
    const bool hasProject = (app.services.organizerService != nullptr);

    // --- Settings Modal ---
    DrawSettingsModal(app);

    // --- Transcription Modal ---
    DrawTranscriptionModal(app);

    if (app.showNewProjectModal) {
        std::filesystem::path defaultPath = ResolveBrowsePath(nullptr, app.projectRoot);
        SetPathBuffer(app.projectPathBuffer, sizeof(app.projectPathBuffer), defaultPath);
        ImGui::OpenPopup("New Project");
        app.showNewProjectModal = false;
    }
    if (ImGui::BeginPopupModal("New Project", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::Text("Pasta do projeto:");
        ImGui::InputText("##newproject", app.projectPathBuffer, sizeof(app.projectPathBuffer));
        DrawFolderBrowser("new_project_browser",
                          app.projectPathBuffer,
                          sizeof(app.projectPathBuffer),
                          app.projectRoot);
        if (ImGui::Button("Criar", ImVec2(120, 0))) {
            if (!app.NewProject(app.projectPathBuffer)) {
                app.AppendLog("[SYSTEM] Falha ao criar projeto.\n");
            }
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancelar", ImVec2(120, 0))) {
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }

    if (app.showOpenProjectModal) {
        std::filesystem::path defaultPath = ResolveBrowsePath(nullptr, app.projectRoot);
        SetPathBuffer(app.projectPathBuffer, sizeof(app.projectPathBuffer), defaultPath);
        ImGui::OpenPopup("Open Project");
        app.showOpenProjectModal = false;
    }
    if (ImGui::BeginPopupModal("Open Project", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::Text("Pasta do projeto:");
        ImGui::InputText("##openproject", app.projectPathBuffer, sizeof(app.projectPathBuffer));
        DrawFolderBrowser("open_project_browser",
                          app.projectPathBuffer,
                          sizeof(app.projectPathBuffer),
                          app.projectRoot);
        if (ImGui::Button("Abrir", ImVec2(120, 0))) {
            if (!app.OpenProject(app.projectPathBuffer)) {
                app.AppendLog("[SYSTEM] Falha ao abrir projeto.\n");
            }
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancelar", ImVec2(120, 0))) {
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }

    if (app.showSaveAsProjectModal) {
        std::filesystem::path defaultPath = ResolveBrowsePath(nullptr, app.projectRoot);
        SetPathBuffer(app.projectPathBuffer, sizeof(app.projectPathBuffer), defaultPath);
        ImGui::OpenPopup("Save Project As");
        app.showSaveAsProjectModal = false;
    }
    if (ImGui::BeginPopupModal("Save Project As", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::Text("Pasta do projeto:");
        ImGui::InputText("##saveprojectas", app.projectPathBuffer, sizeof(app.projectPathBuffer));
        DrawFolderBrowser("save_project_browser",
                          app.projectPathBuffer,
                          sizeof(app.projectPathBuffer),
                          app.projectRoot);
        if (ImGui::Button("Salvar", ImVec2(120, 0))) {
            if (!app.SaveProjectAs(app.projectPathBuffer)) {
                app.AppendLog("[SYSTEM] Falha ao salvar projeto como.\n");
            }
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancelar", ImVec2(120, 0))) {
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }

    if (app.showOpenFileModal) {
        ImGui::OpenPopup("Open File");
        app.showOpenFileModal = false;
    }
    if (ImGui::BeginPopupModal("Open File", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::Text("Caminho do arquivo:");
        ImGui::InputText("##openfilepath", app.openFilePathBuffer, sizeof(app.openFilePathBuffer));
        if (DrawFileBrowser("open_file_browser", 
                        app.openFilePathBuffer, 
                        sizeof(app.openFilePathBuffer), 
                        app.projectRoot.empty() ? "/" : app.projectRoot)) {
            app.OpenExternalFile(app.openFilePathBuffer);
            ImGui::CloseCurrentPopup();
        }
        
        if (ImGui::Button("Abrir", ImVec2(120,0))) {
            app.OpenExternalFile(app.openFilePathBuffer);
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancelar", ImVec2(120,0))) {
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }

    if (hasProject) {
        ImGui::TextDisabled("Project: %s", app.projectRoot.c_str());
    } else {
        ImGui::TextDisabled("Nenhum projeto aberto.");
    }
    ImGui::Separator();
}

void DrawTaskDetailsModal(AppState& app) {
    if (app.showTaskDetailsModal) {
        ImGui::OpenPopup("Detalhes da Tarefa");
    }

    ImVec2 center = ImGui::GetMainViewport()->GetCenter();
    ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSize(ImVec2(600, 400), ImGuiCond_Appearing);

    if (ImGui::BeginPopupModal("Detalhes da Tarefa", &app.showTaskDetailsModal, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::TextColored(ImVec4(0.4f, 0.8f, 1.0f, 1.0f), "Título:");
        ImGui::PushFont(ImGui::GetIO().Fonts->Fonts[0]); 
        ImGui::TextWrapped("%s", app.selectedTaskTitle.c_str());
        ImGui::PopFont();
        
        ImGui::Spacing();
        ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), "Origem: %s", app.selectedTaskOrigin.c_str());
        ImGui::Separator();

        ImGui::BeginChild("TaskContent", ImVec2(0, 200), true);
        DrawMarkdownPreview(app, app.selectedTaskContent, false);
        ImGui::EndChild();

        ImGui::Spacing();
        if (ImGui::Button("Fechar", ImVec2(120, 0))) {
            app.showTaskDetailsModal = false;
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }
}

void DrawUpdateModal(AppState& app) {
    if (app.showUpdateModal) {
        ImGui::OpenPopup("Check for Updates");
    }

    if (ImGui::BeginPopupModal("Check for Updates", &app.showUpdateModal, ImGuiWindowFlags_AlwaysAutoResize)) {
        if (app.isCheckingUpdates.load()) {
            ImGui::Text("Checking GitHub for the latest release...");
            static float f = 0.0f;
            f += ImGui::GetIO().DeltaTime;
            char dots[10];
            int numDots = (int(f * 2.0f) % 4);
            std::memset(dots, '.', numDots);
            dots[numDots] = '\0';
            ImGui::Text("%s", dots);
        } else {
            if (app.updateAvailable) {
                ImGui::TextColored(ImVec4(0, 1, 0, 1), "A new version is available: %s", app.latestVersion.c_str());
                ImGui::Text("Current version: %s", IDEAWALKER_VERSION);
                ImGui::Spacing();
                ImGui::TextWrapped("Visit the GitHub releases page to download the latest version.");
                ImGui::TextDisabled("https://github.com/jpereiratrindade/IdeaWalker/releases");
            } else {
                if (app.latestVersion.empty()) {
                    ImGui::Text("Could not fetch update information.");
                } else {
                    ImGui::Text("You are using the latest version (%s).", IDEAWALKER_VERSION);
                }
            }
            ImGui::Spacing();
            if (ImGui::Button("Close", ImVec2(120, 0))) {
                app.showUpdateModal = false;
                ImGui::CloseCurrentPopup();
            }
        }
        ImGui::EndPopup();
    }
}

void DrawHelpModal(AppState& app) {
    if (app.showHelpModal) {
        ImGui::OpenPopup("Help & Documentation");
    }

    ImVec2 center = ImGui::GetMainViewport()->GetCenter();
    ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSize(ImVec2(900, 700), ImGuiCond_FirstUseEver);

    if (ImGui::BeginPopupModal("Help & Documentation", &app.showHelpModal, ImGuiWindowFlags_NoCollapse)) {
        static std::string helpContent;
        static std::string lastLoadedFile;

        struct HelpDoc { std::string name; std::string path; };
        static std::vector<HelpDoc> docs = {
            {"System Overview", "README.md"},
            {"Changelog", "CHANGELOG.md"},
            {"Technical Guide", "docs/TECHNICAL_GUIDE.md"},
            {"LLM Prompt Guidelines", "docs/LLM_PROMPT_GUIDELINES.md"},
            {"Writing Implementation", "docs/WRITING_TRAJECTORY_IMPLEMENTATION.md"}
        };

        if (lastLoadedFile.empty()) {
            lastLoadedFile = docs[0].path;
            std::ifstream f(docs[0].path);
            if (f.is_open()) {
                std::stringstream ss;
                ss << f.rdbuf();
                helpContent = ss.str();
            }
        }

        static float w = 220.0f;
        ImGui::BeginChild("HelpList", ImVec2(w, -ImGui::GetFrameHeightWithSpacing()), true);
        for (const auto& doc : docs) {
            bool selected = (lastLoadedFile == doc.path);
            if (ImGui::Selectable(doc.name.c_str(), selected)) {
                lastLoadedFile = doc.path;
                std::ifstream f(doc.path);
                if (f.is_open()) {
                    std::stringstream ss;
                    ss << f.rdbuf();
                    helpContent = ss.str();
                } else {
                    helpContent = "Não foi possível carregar: " + doc.path;
                }
            }
        }
        ImGui::EndChild();
        ImGui::SameLine();
        ImGui::BeginChild("HelpView", ImVec2(0, -ImGui::GetFrameHeightWithSpacing()), true);
        if (helpContent.empty()) {
            ImGui::Text("Select a document to read.");
        } else {
            DrawMarkdownPreview(app, helpContent, false);
        }
        ImGui::EndChild();

        ImGui::Spacing();
        if (ImGui::Button("Close", ImVec2(120, 0))) {
            app.showHelpModal = false;
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        ImGui::TextDisabled("(Versão: %s)", IDEAWALKER_VERSION);
        
        ImGui::EndPopup();
    }
}

void DrawHistoryModal(AppState& app) {
    if (app.showHistoryModal) {
        ImGui::OpenPopup("Trajetória da Ideia");
    }

    ImVec2 center = ImGui::GetMainViewport()->GetCenter();
    ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSize(ImVec2(800, 600), ImGuiCond_FirstUseEver);

    if (ImGui::BeginPopupModal("Trajetória da Ideia", &app.showHistoryModal, ImGuiWindowFlags_MenuBar)) {
        if (ImGui::BeginMenuBar()) {
            ImGui::TextDisabled("Evolução Temporal: %s", app.selectedNoteIdForHistory.c_str());
            ImGui::EndMenuBar();
        }

        auto FormatHistoryLabel = [](const std::string& filename) -> std::string {
            std::string base = filename;
            size_t dotPos = base.find_last_of('.');
            if (dotPos != std::string::npos) base = base.substr(0, dotPos);
            size_t usTime = base.find_last_of('_');
            if (usTime == std::string::npos) return filename;
            std::string timePart = base.substr(usTime + 1);
            std::string beforeTime = base.substr(0, usTime);
            size_t usDate = beforeTime.find_last_of('_');
            if (usDate == std::string::npos) return filename;
            std::string datePart = beforeTime.substr(usDate + 1);
            if (datePart.size() != 8 || timePart.size() != 6) return filename;

            std::string formatted;
            formatted.reserve(19);
            formatted.append(datePart.substr(0, 4)).push_back('-');
            formatted.append(datePart.substr(4, 2)).push_back('-');
            formatted.append(datePart.substr(6, 2)).push_back(' ');
            formatted.append(timePart.substr(0, 2)).push_back(':');
            formatted.append(timePart.substr(2, 2)).push_back(':');
            formatted.append(timePart.substr(4, 2));
            return formatted;
        };

        if (app.historyVersions.empty()) {
            ImGui::Text("Nenhuma versão anterior encontrada para esta nota.");
        } else {
            static float w = 200.0f;
            ImGui::BeginChild("HistoryList", ImVec2(w, 0), true);
            for (int i = 0; i < (int)app.historyVersions.size(); ++i) {
                bool selected = (app.selectedHistoryIndex == i);
                std::string display = FormatHistoryLabel(app.historyVersions[i]);
                if (ImGui::Selectable(display.c_str(), selected)) {
                    app.selectedHistoryIndex = i;
                    if (app.services.organizerService) {
                        app.selectedHistoryContent = app.services.organizerService->getVersionContent(app.historyVersions[i]);
                    }
                }
            }
            ImGui::EndChild();
            ImGui::SameLine();
            ImGui::BeginChild("HistoryContent", ImVec2(0, 0), true);
            if (app.selectedHistoryIndex >= 0 && !app.selectedHistoryContent.empty()) {
                InputTextMultilineString("##histcontent", &app.selectedHistoryContent, ImVec2(-FLT_MIN, -FLT_MIN), ImGuiInputTextFlags_ReadOnly);
            } else {
                ImGui::TextDisabled("Selecione uma versão para visualizar.");
            }
            ImGui::EndChild();
        }

        if (app.selectedHistoryIndex >= 0 && !app.selectedHistoryContent.empty() && app.services.organizerService) {
            if (ImGui::Button("Restaurar esta versao")) {
                app.services.organizerService->updateNote(app.selectedNoteIdForHistory, app.selectedHistoryContent);
                app.AppendLog("[SYSTEM] Versao restaurada: " + app.selectedNoteIdForHistory + "\n");
                app.RefreshAllInsights();
                app.LoadHistory(app.selectedNoteIdForHistory);
            }
            ImGui::SameLine();
        }
        if (ImGui::Button("Fechar")) {
            app.showHistoryModal = false;
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }
}

void DrawAllModals(AppState& app) {
    DrawProjectModals(app);
    DrawTaskDetailsModal(app);
    DrawHistoryModal(app);
    DrawHelpModal(app);
    DrawUpdateModal(app);
}

} // namespace ideawalker::ui
