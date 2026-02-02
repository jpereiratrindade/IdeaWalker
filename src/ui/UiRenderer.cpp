/**
 * @file UiRenderer.cpp
 * @brief Implementation of the UI rendering system using Dear ImGui and ImNodes.
 */
#include "ui/UiRenderer.hpp"
#include "ui/ConversationPanel.hpp"
#include "ui/panels/WritingPanels.hpp"
#include "ui/UiMarkdownRenderer.hpp"
#include "ui/UiFileBrowser.hpp"
#include "domain/writing/MermaidParser.hpp"

#include "application/OrganizerService.hpp"
#include "application/DocumentIngestionService.hpp"
#include "imgui.h"
#include "imnodes.h"

#include <algorithm>
#include <chrono>
#include <cfloat>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <filesystem>
#include <functional>
#include <mutex>
#include <sstream>
#include <string>
#include <thread>
#include <iostream>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace ideawalker::ui {

namespace {
    constexpr float NODE_MAX_WIDTH = 250.0f;





std::tm ToLocalTime(std::time_t tt) {
    std::tm tm = {};
#if defined(_WIN32)
    localtime_s(&tm, &tt);
#else
    localtime_r(&tt, &tm);
#endif
    return tm;
}

int TextEditCallback(ImGuiInputTextCallbackData* data) {
    if (data->EventFlag == ImGuiInputTextFlags_CallbackResize) {
        auto* str = static_cast<std::string*>(data->UserData);
        str->resize(data->BufTextLen);
        data->Buf = str->data();
    }
    return 0;
}

bool InputTextMultilineString(const char* label, std::string* str, const ImVec2& size, ImGuiInputTextFlags flags = 0) {
    flags |= ImGuiInputTextFlags_CallbackResize;
    flags |= ImGuiInputTextFlags_NoHorizontalScroll; // Enable Word Wrap
    if (str->capacity() == 0) {
        str->reserve(1024);
    }
    return ImGui::InputTextMultiline(label, str->data(), str->capacity() + 1, size, flags, TextEditCallback, str);
}

bool TaskCard(const char* id, const std::string& text, float width) {
    ImGuiStyle& style = ImGui::GetStyle();
    float paddingX = style.FramePadding.x;
    float paddingY = style.FramePadding.y;
    float wrapWidth = width - paddingX * 2.0f;
    if (wrapWidth < 1.0f) {
        wrapWidth = 1.0f;
    }

    ImVec2 textSize = ImGui::CalcTextSize(text.c_str(), nullptr, false, wrapWidth);
    float height = textSize.y + paddingY * 2.0f;

    ImGui::PushID(id);
    ImGui::InvisibleButton("##task", ImVec2(width, height));
    bool hovered = ImGui::IsItemHovered();
    bool active = ImGui::IsItemActive();
    bool clicked = ImGui::IsItemClicked();

    ImU32 bg = ImGui::GetColorU32(active ? ImGuiCol_ButtonActive : (hovered ? ImGuiCol_ButtonHovered : ImGuiCol_Button));
    ImU32 border = ImGui::GetColorU32(ImGuiCol_Border);
    ImVec2 min = ImGui::GetItemRectMin();
    ImVec2 max = ImGui::GetItemRectMax();
    ImDrawList* drawList = ImGui::GetWindowDrawList();
    drawList->AddRectFilled(min, max, bg, 4.0f);
    drawList->AddRect(min, max, border, 4.0f);

    ImVec2 textPos(min.x + paddingX, min.y + paddingY);
    drawList->AddText(ImGui::GetFont(), ImGui::GetFontSize(), textPos,
                      ImGui::GetColorU32(ImGuiCol_Text), text.c_str(), nullptr, wrapWidth);

    ImGui::PopID();
    return clicked;
}




void DrawNodeGraph(AppState& app) {
    auto label = [&app](const char* withEmoji, const char* plain) {
        return app.emojiEnabled ? withEmoji : plain;
    };
    ImNodes::EditorContextSet((ImNodesEditorContext*)app.mainGraphContext);
    ImNodes::BeginNodeEditor();

    // 1. Draw Nodes
    for (auto& node : app.graphNodes) {
        // Set position for physics, but skip if currently selected (let user drag)
        if (app.physicsEnabled && !ImNodes::IsNodeSelected(node.id)) {
            ImNodes::SetNodeGridSpacePos(node.id, ImVec2(node.x, node.y));
        }

        bool isTask = (node.type == NodeType::TASK);
        bool isConcept = (node.type == NodeType::CONCEPT);
        bool isHypothesis = (node.type == NodeType::HYPOTHESIS);

        if (isTask) {
            ImNodes::PushColorStyle(ImNodesCol_NodeBackground, IM_COL32(50, 50, 50, 255));
            ImNodes::PushColorStyle(ImNodesCol_TitleBar, 
                node.isCompleted ? IM_COL32(46, 125, 50, 200) : IM_COL32(230, 81, 0, 200));
        } else if (isConcept) {
             // Dark Purple for Concepts
             ImNodes::PushColorStyle(ImNodesCol_NodeBackground, IM_COL32(40, 30, 60, 255));
             ImNodes::PushColorStyle(ImNodesCol_TitleBar, IM_COL32(100, 60, 150, 200));
        } else if (isHypothesis) {
             // Cyan/Teal for Hypotheses
             ImNodes::PushColorStyle(ImNodesCol_NodeBackground, IM_COL32(0, 50, 50, 255));
             ImNodes::PushColorStyle(ImNodesCol_TitleBar, IM_COL32(0, 150, 150, 200));
        }

        ImNodes::BeginNode(node.id);
        
        ImNodes::BeginNodeTitleBar();
        ImGui::PushTextWrapPos(ImGui::GetCursorPos().x + (node.wrapW - 30.0f)); // Reserve space for padding
        if (isTask) {
             const char* emoji = "📋 "; // To-Do
             if (node.isCompleted) emoji = "✅ ";
             else if (node.isInProgress) emoji = "⏳ ";
             
             ImGui::TextUnformatted(emoji);
             ImGui::SameLine();
        } else if (isHypothesis) {
             ImGui::TextUnformatted("🧪 "); // Science/Experiment emoji
             ImGui::SameLine();
        }
        ImGui::TextUnformatted(node.title.c_str());
        ImGui::PopTextWrapPos();
        ImNodes::EndNodeTitleBar();

        ImNodes::BeginOutputAttribute(node.id << 8); 
        ImGui::Dummy(ImVec2(10, 0));
        ImNodes::EndOutputAttribute();

        ImNodes::BeginInputAttribute((node.id << 8) + 1);
        ImGui::Dummy(ImVec2(10, 0));
        ImNodes::EndInputAttribute();

        ImNodes::EndNode();

        if (isTask || isConcept || isHypothesis) {
            ImNodes::PopColorStyle();
            ImNodes::PopColorStyle();
        }
    }

    // 2. Draw Links
    for (const auto& link : app.graphLinks) {
        int startAttr = (link.startNode << 8);
        int endAttr = (link.endNode << 8) + 1;
        ImNodes::Link(link.id, startAttr, endAttr);
    }

    ImNodes::EndNodeEditor();

    // 3. Sync User Dragging back to AppState
    std::unordered_set<int> selectedNodes;
    for (auto& node : app.graphNodes) {
        if (ImNodes::IsNodeSelected(node.id)) {
            selectedNodes.insert(node.id);
            ImVec2 pos = ImNodes::GetNodeGridSpacePos(node.id);
            node.x = pos.x;
            node.y = pos.y;
            node.vx = 0;
            node.vy = 0;
        }
    }

    // 4. Update Physics
    if (!app.graphNodes.empty() && app.physicsEnabled) {
        app.UpdateGraphPhysics(selectedNodes);
    }
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
        ImGui::PushFont(ImGui::GetIO().Fonts->Fonts[0]); // Bold if available?
        ImGui::TextWrapped("%s", app.selectedTaskTitle.c_str());
        ImGui::PopFont();
        
        ImGui::Spacing();
        ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), "Origem: %s", app.selectedTaskOrigin.c_str());
        ImGui::Separator();

        ImGui::BeginChild("TaskContent", ImVec2(0, 200), true);
        // Uses markdown preview for rich content
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

} // namespace

static void DrawMenuBar(AppState& app) {
    auto label = [&app](const char* withEmoji, const char* plain) {
        return app.emojiEnabled ? withEmoji : plain;
    };
    const bool hasProject = (app.organizerService != nullptr);
    const bool canChangeProject = !app.isProcessing.load();

    if (ImGui::BeginMenuBar()) {
        if (ImGui::BeginMenu("File")) {
            if (ImGui::MenuItem("Novo Projeto...", nullptr, false, canChangeProject)) {
                app.showNewProjectModal = true;
            }
            if (ImGui::MenuItem("Abrir Projeto...", nullptr, false, canChangeProject)) {
                app.showOpenProjectModal = true;
            }
            if (ImGui::MenuItem("Abrir Arquivo...", nullptr, false, !app.isProcessing.load())) {
                app.showOpenFileModal = true;
                // Reset buffer or set default?
                app.openFilePathBuffer[0] = '\0';
            }
            if (ImGui::MenuItem("Salvar Projeto", nullptr, false, hasProject && canChangeProject)) {
                if (!app.SaveProject()) {
                    app.AppendLog("[SYSTEM] Falha ao salvar projeto.\n");
                }
            }
            if (ImGui::MenuItem("Salvar Projeto Como...", nullptr, false, canChangeProject)) {
                app.showSaveAsProjectModal = true;
            }
            if (ImGui::MenuItem(label("🎙️ Transcrever Áudio...", "Transcrever Áudio..."), nullptr, false, hasProject)) {
                app.showTranscriptionModal = true;
                app.transcriptionPathBuffer[0] = '\0'; // Clear previous
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
             
             if (hasProject && app.organizerService->getAI()) {
                 if (ImGui::BeginMenu(label("🧠 Selecionar Modelo de IA", "Selecionar Modelo de IA"))) {
                     auto currentModel = app.organizerService->getAI()->getCurrentModel();
                     static std::vector<std::string> cachedModels;
                     static std::chrono::steady_clock::time_point lastCheck;
                     auto now = std::chrono::steady_clock::now();
                     
                     // Cache model list for 10 seconds to avoid spamming the API on every frame
                     if (cachedModels.empty() || std::chrono::duration_cast<std::chrono::seconds>(now - lastCheck).count() > 10) {
                         cachedModels = app.organizerService->getAI()->getAvailableModels();
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
                         cachedModels.clear(); // Force refresh next frame
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
                    app.outputLog += "[Info] Mapa mental exportado para o clipboard.\n";
                }
                if (ImGui::MenuItem(label("📁 Exportar Full (.md)", "Exportar Completo (.md)"))) {
                    std::string fullMd = app.ExportFullMarkdown();
                    ImGui::SetClipboardText(fullMd.c_str());
                    app.outputLog += "[Info] Conhecimento completo exportado para o clipboard.\n";
                }
                if (ImGui::MenuItem(label("🎯 Centralizar Grafo", "Centralizar Grafo"))) {
                    app.CenterGraph();
                }
                ImGui::EndMenu();
            }
            ImGui::EndMenu();
        }

        ImGui::EndMenuBar();
    }
}

static void DrawTranscriptionModal(AppState& app) {
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

static void DrawSettingsModal(AppState& app) {
    auto label = [&app](const char* withEmoji, const char* plain) {
        return app.emojiEnabled ? withEmoji : plain;
    };
    if (app.showSettingsModal) {
        ImGui::OpenPopup("Preferências");
    }
    if (ImGui::BeginPopupModal("Preferências", &app.showSettingsModal, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::Text("Configurações Gerais");
        ImGui::Separator();
        
        // AI Persona Selection (Removed - Now Autonomous)
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

static void DrawProjectModals(AppState& app) {
    auto label = [&app](const char* withEmoji, const char* plain) {
        return app.emojiEnabled ? withEmoji : plain;
    };
    const bool hasProject = (app.organizerService != nullptr);

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

static void DrawDashboardTab(AppState& app) {
    auto label = [&app](const char* withEmoji, const char* plain) {
        return app.emojiEnabled ? withEmoji : plain;
    };
    const bool hasProject = (app.organizerService != nullptr);

    ImGuiTabItemFlags flags0 = (app.requestedTab == 0) ? ImGuiTabItemFlags_SetSelected : 0;
    if (ImGui::BeginTabItem(label("🎙️ Dashboard & Inbox", "Dashboard & Inbox"), NULL, flags0)) {
        if (app.requestedTab == 0) app.requestedTab = -1;
        app.activeTab = 0;
        if (!hasProject) {
            ImGui::TextDisabled("Nenhum projeto aberto.");
            ImGui::TextDisabled("Use File > New Project ou File > Open Project para comecar.");
        } else {
            ImGui::Spacing();
            
            if (ImGui::Button(label("🔄 Refresh Inbox", "Refresh Inbox"), ImVec2(150, 30))) {
                app.RefreshInbox();
            }
            
            ImGui::Separator();
            
            ImGui::Text("Entrada (%zu ideias):", app.inboxThoughts.size());
            ImGui::BeginChild("InboxList", ImVec2(0, 200), true);
            for (const auto& thought : app.inboxThoughts) {
                bool isSelected = (app.selectedInboxFilename == thought.filename);
                if (ImGui::Selectable(thought.filename.c_str(), isSelected)) {
                    app.selectedInboxFilename = thought.filename;
                }
            }
            ImGui::EndChild();

            ImGui::Spacing();
            
            bool processing = app.isProcessing.load();
            if (processing) ImGui::BeginDisabled();
            auto statusHandler = [&app](const std::string& s) {
                app.SetProcessingStatus(s);
                // Also print to stdout for CLI visibility (Low-end hardware awareness)
                std::cout << "[Organizer] " << s << std::endl;

                // Log relevant AI stages to the system log
                if (s.find("Executando:") != std::string::npos || 
                    s.find("Orquestrador:") != std::string::npos ||
                    s.find("Persona:") != std::string::npos) {
                    app.AppendLog("[AI] " + s + "\n");
                }
            };

            auto startBatch = [&app, statusHandler](bool force) {
                app.isProcessing.store(true);
                app.AppendLog(force ? "[SYSTEM] Starting AI reprocess (batch)...\n" : "[SYSTEM] Starting AI batch processing...\n");
                std::thread([&app, force, statusHandler]() {
                    app.organizerService->processInbox(force, app.fastMode, statusHandler);
                    bool consolidated = app.organizerService->updateConsolidatedTasks();
                    app.isProcessing.store(false);
                    app.SetProcessingStatus("Thinking..."); // Reset
                    app.AppendLog("[SYSTEM] Processing finished.\n");
                    app.AppendLog(consolidated ? "[SYSTEM] Consolidated tasks updated.\n" : "[SYSTEM] Consolidated tasks failed.\n");
                    app.pendingRefresh.store(true);
                }).detach();
            };

            auto startSingle = [&app, statusHandler](const std::string& filename, bool force) {
                app.isProcessing.store(true);
                app.SetProcessingStatus("Thinking...");
                app.AppendLog(force ? "[SYSTEM] Starting AI reprocess for " + filename + "...\n"
                                    : "[SYSTEM] Starting AI processing for " + filename + "...\n");
                std::thread([&app, filename, force, statusHandler]() {
                    auto result = app.organizerService->processInboxItem(filename, force, app.fastMode, statusHandler);
                    switch (result) {
                    case application::OrganizerService::ProcessResult::Processed:
                        app.AppendLog("[SYSTEM] Processing finished for " + filename + ".\n");
                        break;
                    case application::OrganizerService::ProcessResult::SkippedUpToDate:
                        app.AppendLog("[SYSTEM] Skipped (up-to-date): " + filename + ".\n");
                        break;
                    case application::OrganizerService::ProcessResult::NotFound:
                        app.AppendLog("[SYSTEM] Inbox file not found: " + filename + ".\n");
                        break;
                    case application::OrganizerService::ProcessResult::Failed:
                        app.AppendLog("[SYSTEM] Processing failed for " + filename + ".\n");
                        break;
                    }
                    if (result != application::OrganizerService::ProcessResult::NotFound
                        && result != application::OrganizerService::ProcessResult::Failed) {
                        bool consolidated = app.organizerService->updateConsolidatedTasks();
                        app.AppendLog(consolidated ? "[SYSTEM] Consolidated tasks updated.\n" : "[SYSTEM] Consolidated tasks failed.\n");
                    }
                    app.isProcessing.store(false);
                    app.SetProcessingStatus("Thinking..."); // Reset
                    app.pendingRefresh.store(true);
                }).detach();
            };

            const bool hasSelection = !app.selectedInboxFilename.empty();
            const char* runLabel = hasSelection
                ? label("🧠 Run Selected", "Run Selected")
                : label("🧠 Run AI Orchestrator", "Run AI Orchestrator");
            if (ImGui::Button(runLabel, ImVec2(250, 50))) {
                if (hasSelection) {
                    startSingle(app.selectedInboxFilename, false);
                } else {
                    startBatch(false);
                }
            }
            if (hasSelection) {
                ImGui::SameLine();
                if (ImGui::Button(label("🧠 Run All", "Run All"), ImVec2(120, 50))) {
                    startBatch(false);
                }
                ImGui::SameLine();
                if (ImGui::Button(label("🔁 Reprocess Selected", "Reprocess Selected"), ImVec2(180, 50))) {
                    startSingle(app.selectedInboxFilename, true);
                }
            } else {
                ImGui::SameLine();
                if (ImGui::Button(label("🔁 Reprocess All", "Reprocess All"), ImVec2(150, 50))) {
                    startBatch(true);
                }
            }
            if (processing) ImGui::EndDisabled();

            if (app.isProcessing.load()) {
                ImGui::SameLine();
                ImGui::TextColored(ImVec4(1, 1, 0, 1), "⏳ %s", app.GetProcessingStatus().c_str());
            }
            if (app.isTranscribing.load()) {
                ImGui::SameLine();
                ImGui::TextColored(ImVec4(0, 1, 1, 1), "🎙️ Transcribing Audio...");
            }

            ImGui::Separator();
            ImGui::Text("System Log:");
            ImGui::BeginChild("Log", ImVec2(0, -100), true);
            std::string logSnapshot = app.GetLogSnapshot();
            ImGui::TextUnformatted(logSnapshot.c_str());
            if (ImGui::GetScrollY() >= ImGui::GetScrollMaxY())
                ImGui::SetScrollHereY(1.0f);
            ImGui::EndChild();

            ImGui::Separator();
            ImGui::Text("%s", label("📥 Ingestão de Documentos (Observacional)", "Ingestion of Documents (Observational)"));
            if (app.ingestionService) {
                if (ImGui::Button("Sincronizar Inbox & Gerar Observações")) {
                    std::thread([&app]() {
                        app.isProcessing.store(true);
                        app.ingestionService->ingestPending([&app](const std::string& s) {
                            app.SetProcessingStatus(s);
                            app.AppendLog("[INGEST] " + s + "\n");
                        });
                        app.isProcessing.store(false);
                        app.pendingRefresh.store(true);
                    }).detach();
                }
                ImGui::SameLine();
                if (ImGui::IsItemHovered()) ImGui::SetTooltip("Processa .txt, .md, .pdf, .tex na /inbox sem alterar originais.");

                auto observations = app.ingestionService->getObservations();
                ImGui::Text("Observações geradas: %zu", observations.size());
                if (ImGui::IsItemHovered()) {
                    ImGui::SetTooltip("Total de registros sintéticos criados a partir dos documentos processados na Inbox.");
                }
            }

            ImGui::Separator();
            ImGui::Text("%s", label("🔥 Activity Heatmap (Last 30 Days)", "Activity Heatmap (Last 30 Days)"));
            
            // Render Heatmap
            ImDrawList* draw_list = ImGui::GetWindowDrawList();
            ImVec2 p = ImGui::GetCursorScreenPos();
            float size = 15.0f;
            float spacing = 3.0f;
            int maxCount = 0;
            for (const auto& entry : app.activityHistory) {
                if (entry.second > maxCount) maxCount = entry.second;
            }
            const auto now = std::chrono::system_clock::now();
            
            for (int i = 0; i < 30; ++i) {
                ImVec4 color = ImVec4(0.15f, 0.15f, 0.15f, 1.0f);
                auto day = now - std::chrono::hours(24 * (29 - i));
                std::time_t tt = std::chrono::system_clock::to_time_t(day);
                std::tm tm = ToLocalTime(tt);
                char buffer[11];
                std::strftime(buffer, sizeof(buffer), "%Y-%m-%d", &tm);
                auto it = app.activityHistory.find(buffer);
                int count = (it != app.activityHistory.end()) ? it->second : 0;
                if (count > 0) {
                    float intensity = (maxCount > 0) ? (static_cast<float>(count) / maxCount) : 0.0f;
                    float green = 0.3f + (0.7f * intensity);
                    color = ImVec4(0.0f, green, 0.0f, 1.0f);
                }

                draw_list->AddRectFilled(ImVec2(p.x + i * (size + spacing), p.y), 
                                        ImVec2(p.x + i * (size + spacing) + size, p.y + size), 
                                        ImColor(color));
            }
            ImGui::Dummy(ImVec2(0, size + 10));
        }
        ImGui::EndTabItem();
    }
}

static void DrawKnowledgeTab(AppState& app) {
    auto label = [&app](const char* withEmoji, const char* plain) {
        return app.emojiEnabled ? withEmoji : plain;
    };
    const bool hasProject = (app.organizerService != nullptr);

    ImGuiTabItemFlags flags1 = (app.requestedTab == 1) ? ImGuiTabItemFlags_SetSelected : 0;
    if (ImGui::BeginTabItem(label("📚 Organized Knowledge", "Organized Knowledge"), NULL, flags1)) {
        if (app.requestedTab == 1) app.requestedTab = -1;
        bool enteringKnowledge = (app.activeTab != 1);
        app.activeTab = 1;
        if (enteringKnowledge && hasProject && !app.isProcessing.load()) {
            app.RefreshAllInsights();
        }
        if (!hasProject) {
            ImGui::TextDisabled("Nenhum projeto aberto.");
            ImGui::TextDisabled("Use File > New Project ou File > Open Project para comecar.");
        } else {
            ImGui::Checkbox("Visao unificada", &app.unifiedKnowledgeView);
            ImGui::Separator();

            if (app.unifiedKnowledgeView) {
                ImGui::SameLine();
                ImGui::Checkbox("Modo Preview", &app.unifiedPreviewMode);

                ImGui::BeginChild("UnifiedKnowledge", ImVec2(0, 0), true);
                if (app.unifiedKnowledge.empty()) {
                    ImGui::TextDisabled("Nenhum insight disponivel.");
                } else {
                    if (app.unifiedPreviewMode) {
                            DrawMarkdownPreview(app, app.unifiedKnowledge, false);
                    } else {
                            InputTextMultilineString("##unifiedRaw", &app.unifiedKnowledge, ImVec2(-FLT_MIN, -FLT_MIN), ImGuiInputTextFlags_ReadOnly);
                    }
                }
                ImGui::EndChild();
            } else {
                // Coluna da Esquerda: Lista de Arquivos
                ImGui::BeginChild("NotesList", ImVec2(250, 0), true);
                for (auto& insight : app.allInsights) {
                    ImGui::PushID(insight.getMetadata().id.c_str());

                    // Header with History Button
                    ImGui::BeginGroup();
                    if (ImGui::SmallButton(label("🕰️", "Hist"))) {
                        app.showHistoryModal = true;
                        app.LoadHistory(insight.getMetadata().id);
                    }
                    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Ver Trajetória (Histórico)");
                    ImGui::SameLine(0.0f, 6.0f);

                    std::string title = insight.getMetadata().title.empty() ? insight.getMetadata().id : insight.getMetadata().title;
                    bool open = ImGui::CollapsingHeader(
                        (title + "###header").c_str(),
                        ImGuiTreeNodeFlags_DefaultOpen
                    );
                    ImGui::EndGroup();

                    if (open) {
                        const std::string& filename = insight.getMetadata().id;
                        bool isSelected = (app.selectedFilename == filename);
                        if (ImGui::Selectable((filename + "###selectable").c_str(), isSelected)) {
                            app.selectedFilename = filename;
                            app.selectedNoteContent = insight.getContent();
                            std::snprintf(app.saveAsFilename, sizeof(app.saveAsFilename), "%s", filename.c_str());

                            // Update Domain Insight
                            domain::Insight::Metadata meta;
                            meta.id = filename;
                            app.currentInsight = std::make_unique<domain::Insight>(meta, app.selectedNoteContent);
                            app.currentInsight->parseActionablesFromContent();

                            // Fetch Backlinks
                            if (app.organizerService) {
                                app.currentBacklinks = app.organizerService->getBacklinks(filename);
                            }
                        }
                    }
                    ImGui::PopID();
                }
                ImGui::EndChild();

                ImGui::SameLine();

                // Coluna da Direita: Conteudo / Editor
                ImGui::BeginChild("NoteContent", ImVec2(0, 0), true);
                if (!app.selectedFilename.empty()) {
                    if (ImGui::Button(label("💾 Save Changes", "Save Changes"), ImVec2(150, 30))) {
                        app.organizerService->updateNote(app.selectedFilename, app.selectedNoteContent);
                        app.AppendLog("[SYSTEM] Saved changes to " + app.selectedFilename + "\n");
                        app.RefreshAllInsights();
                    }
                    ImGui::SameLine();

                    ImGui::SetNextItemWidth(200.0f);
                    ImGui::InputText("##saveasname", app.saveAsFilename, sizeof(app.saveAsFilename));
                    ImGui::SameLine();
                    if (ImGui::Button(label("📂 Save As", "Save As"), ImVec2(100, 30))) {
                        std::string newName = app.saveAsFilename;
                        if (!newName.empty()) {
                            if (newName.find(".md") == std::string::npos && newName.find(".txt") == std::string::npos) {
                                newName += ".md";
                            }
                            app.organizerService->updateNote(newName, app.selectedNoteContent);
                            app.selectedFilename = newName;
                            app.AppendLog("[SYSTEM] Saved as " + newName + "\n");
                            app.RefreshAllInsights();
                        }
                    }

                    ImGui::Separator();
                    
                    // Editor / Visual Switcher
                    if (ImGui::BeginTabBar("EditorTabs")) {
                        if (ImGui::BeginTabItem(label("📝 Editor", "Editor"))) {
                            app.previewMode = false;
                            ImGui::EndTabItem();
                        }
                        if (ImGui::BeginTabItem(label("👁️ Visual", "Visual"))) {
                            app.previewMode = true;
                            ImGui::EndTabItem();
                        }
                        ImGui::EndTabBar();
                    }

                    if (app.previewMode) {
                        ImGui::BeginChild("PreviewScroll", ImVec2(0, -200), true);
                        DrawMarkdownPreview(app, app.selectedNoteContent, false);
                        ImGui::EndChild();
                    } else {
                        // Editor de texto (InputTextMultiline com Resize)
                        if (InputTextMultilineString("##editor", &app.selectedNoteContent, ImVec2(-FLT_MIN, -200))) {
                            if (app.currentInsight) {
                                app.currentInsight->setContent(app.selectedNoteContent);
                                app.currentInsight->parseActionablesFromContent();
                            }
                        }
                    }
                    
                    ImGui::Separator();
                    ImGui::Text("%s", label("🔗 Backlinks (Mencionado em):", "Backlinks (Mencionado em):"));
                    if (app.currentBacklinks.empty()) {
                        ImGui::TextDisabled("Nenhuma referencia encontrada.");
                    } else {
                        for (const auto& link : app.currentBacklinks) {
                            if (ImGui::Button(link.c_str())) {
                                app.AppendLog("[UI] Jumping to " + link + "\n");
                                app.selectedFilename = link;
                                if (app.organizerService) {
                                    app.selectedNoteContent = app.organizerService->getNoteContent(link);
                                    std::snprintf(app.saveAsFilename, sizeof(app.saveAsFilename), "%s", link.c_str());
                                    
                                    domain::Insight::Metadata meta;
                                    meta.id = link;
                                    app.currentInsight = std::make_unique<domain::Insight>(meta, app.selectedNoteContent);
                                    app.currentInsight->parseActionablesFromContent();
                                    app.currentBacklinks = app.organizerService->getBacklinks(link);
                                    app.AnalyzeSuggestions();
                                }
                            }
                            ImGui::SameLine();
                        }
                        ImGui::NewLine();
                    }

                    ImGui::Separator();
                    ImGui::Text("%s", label("🧠 Ressonância Semântica (Sugestões):", "Semantic Resonance (Suggestions):"));
                    if (app.isAnalyzingSuggestions) {
                        ImGui::TextDisabled("Analisando conexões...");
                    } else if (app.currentSuggestions.empty()) {
                        ImGui::TextDisabled("Nenhuma conexão óbvia detectada.");
                    } else {
                        std::lock_guard<std::mutex> lock(app.suggestionsMutex);
                        for (const auto& sug : app.currentSuggestions) {
                            std::string sugLabel = sug.targetId + " (" + (sug.reasons.empty() ? "" : sug.reasons[0].evidence) + ")";
                            if (ImGui::Button(sugLabel.c_str())) {
                                app.selectedNoteContent += "\n\n[[" + sug.targetId + "]]";
                                if (app.currentInsight) {
                                    app.currentInsight->setContent(app.selectedNoteContent);
                                    // Auto-save to disk to ensure backlinks are detectable immediately
                                    if (app.organizerService) {
                                        app.organizerService->updateNote(app.selectedFilename, app.selectedNoteContent);
                                    }
                                }
                                app.AppendLog("[UI] Conectado a: " + sug.targetId + "\n");
                            }
                            if (ImGui::IsItemHovered()) {
                                ImGui::SetTooltip("Ponte: %s", sug.reasons[0].kind.c_str());
                            }
                            ImGui::SameLine();
                        }
                        ImGui::NewLine();
                    }
                    if (ImGui::SmallButton("Reanalisar Agora")) {
                        app.AnalyzeSuggestions();
                    }
                } else {
                    ImGui::Text("Select a note from the list to view or edit.");
                }
                ImGui::EndChild();
            }
        }

        ImGui::EndTabItem();
    }
}

static void DrawExecutionTab(AppState& app) {
    auto label = [&app](const char* withEmoji, const char* plain) {
        return app.emojiEnabled ? withEmoji : plain;
    };
    const bool hasProject = (app.organizerService != nullptr);

    ImGuiTabItemFlags flags2 = (app.requestedTab == 2) ? ImGuiTabItemFlags_SetSelected : 0;
    if (ImGui::BeginTabItem(label("🏭 Execução", "Execução"), NULL, flags2)) {
        if (app.requestedTab == 2) app.requestedTab = -1;
        app.activeTab = 2;
        if (!hasProject) {
            ImGui::TextDisabled("Nenhum projeto aberto.");
            ImGui::TextDisabled("Use File > New Project ou File > Open Project para comecar.");
        } else {
            if (ImGui::Button(label("🔄 Refresh Tasks", "Refresh Tasks"), ImVec2(120, 30))) {
                app.RefreshAllInsights();
            }
            ImGui::Separator();

            const ImGuiTableFlags tableFlags = ImGuiTableFlags_SizingStretchSame
                | ImGuiTableFlags_NoBordersInBody
                | ImGuiTableFlags_NoSavedSettings
                | ImGuiTableFlags_PadOuterX;
            ImGui::PushStyleVar(ImGuiStyleVar_CellPadding, ImVec2(12.0f, 12.0f));
            ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(8.0f, 8.0f));

            const bool useConsolidatedTasks = app.consolidatedInsight && !app.consolidatedInsight->getActionables().empty();
            std::vector<const domain::Insight*> taskSources;
            if (useConsolidatedTasks) {
                taskSources.push_back(app.consolidatedInsight.get());
            } else {
                for (const auto& insight : app.allInsights) {
                    taskSources.push_back(&insight);
                }
            }

            auto renderColumn = [&](const char* childId,
                                    const char* title,
                                    const ImVec4& titleColor,
                                    const ImVec4& backgroundColor,
                                    auto shouldShow,
                                    bool completed,
                                    bool inProgress) {
                ImGui::TextColored(titleColor, "%s", title);
                ImGui::Dummy(ImVec2(0.0f, 4.0f));
                ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 6.0f);
                ImGui::PushStyleVar(ImGuiStyleVar_ChildBorderSize, 1.0f);
                ImGui::PushStyleColor(ImGuiCol_ChildBg, backgroundColor);
                ImGui::BeginChild(childId, ImVec2(0, 0), true);
                for (const auto* insight : taskSources) {
                    const auto& actionables = insight->getActionables();
                    for (size_t i = 0; i < actionables.size(); ++i) {
                        if (!shouldShow(actionables[i])) {
                            continue;
                        }
                        std::string itemId = std::string(childId) + insight->getMetadata().id + std::to_string(i);
                        float cardWidth = ImGui::GetContentRegionAvail().x;
                        if (cardWidth < 1.0f) {
                            cardWidth = 1.0f;
                        }
                        if (TaskCard(itemId.c_str(), actionables[i].description, cardWidth)) {
                            app.selectedFilename = insight->getMetadata().id;
                            app.selectedNoteContent = insight->getContent();

                            domain::Insight::Metadata meta;
                            meta.id = app.selectedFilename;
                            app.currentInsight = std::make_unique<domain::Insight>(meta, app.selectedNoteContent);
                            app.currentInsight->parseActionablesFromContent();
                        }
                        
                        if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
                            app.showTaskDetailsModal = true;
                            app.selectedTaskTitle = "Detalhes da Tarefa";
                            app.selectedTaskContent = actionables[i].description;
                            app.selectedTaskOrigin = insight->getMetadata().id;
                        }

                        if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort)) {
                            ImGui::BeginTooltip();
                            if (useConsolidatedTasks) {
                                ImGui::Text("Origem: consolidado");
                            } else {
                                ImGui::Text("Origem: %s", insight->getMetadata().id.c_str());
                            }
                            ImGui::EndTooltip();
                        }

                        if (ImGui::BeginDragDropSource()) {
                            std::string payload = insight->getMetadata().id + "|" + std::to_string(i);
                            ImGui::SetDragDropPayload("DND_TASK", payload.c_str(), payload.size() + 1);
                            ImGui::Text("Movendo: %s", actionables[i].description.c_str());
                            ImGui::EndDragDropSource();
                        }
                    }
                }
                ImGui::EndChild();
                if (app.organizerService && ImGui::BeginDragDropTarget()) {
                    if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("DND_TASK")) {
                        std::string data = static_cast<const char*>(payload->Data);
                        size_t sep = data.find('|');
                        std::string filename = data.substr(0, sep);
                        int index = std::stoi(data.substr(sep + 1));
                        app.organizerService->setTaskStatus(filename, index, completed, inProgress);
                        app.pendingRefresh.store(true);
                    }
                    ImGui::EndDragDropTarget();
                }
                ImGui::PopStyleColor();
                ImGui::PopStyleVar(2);
            };

            if (ImGui::BeginTable("ExecutionColumns", 3, tableFlags)) {
                ImGui::TableNextRow();
                ImGui::TableNextColumn();
                renderColumn(
                    "todo",
                    label("📋 A FAZER", "A FAZER"),
                    ImVec4(1, 0.8f, 0, 1),
                    ImVec4(0.08f, 0.09f, 0.10f, 1.0f),
                    [](const domain::Actionable& task) { return !task.isCompleted && !task.isInProgress; },
                    false,
                    false
                );

                ImGui::TableNextColumn();
                renderColumn(
                    "progress",
                    label("⏳ EM ANDAMENTO", "EM ANDAMENTO"),
                    ImVec4(0, 0.7f, 1, 1),
                    ImVec4(0.07f, 0.09f, 0.11f, 1.0f),
                    [](const domain::Actionable& task) { return task.isInProgress; },
                    false,
                    true
                );

                ImGui::TableNextColumn();
                renderColumn(
                    "done",
                    label("✅ FEITO", "FEITO"),
                    ImVec4(0, 1, 0, 1),
                    ImVec4(0.07f, 0.10f, 0.08f, 1.0f),
                    [](const domain::Actionable& task) { return task.isCompleted; },
                    true,
                    false
                );

                ImGui::EndTable();
            }

            ImGui::PopStyleVar(2);
        }
        ImGui::EndTabItem();
    }
}

static void DrawGraphTab(AppState& app) {
    auto label = [&app](const char* withEmoji, const char* plain) {
        return app.emojiEnabled ? withEmoji : plain;
    };
    const bool hasProject = (app.organizerService != nullptr);

    ImGuiTabItemFlags flags3 = (app.requestedTab == 3) ? ImGuiTabItemFlags_SetSelected : 0;
    if (ImGui::BeginTabItem(label("🕸️ Neural Web", "Neural Web"), NULL, flags3)) {
        if (app.requestedTab == 3) app.requestedTab = -1;
        bool enteringGraph = (app.activeTab != 3);
        app.activeTab = 3;

        if (enteringGraph && hasProject) {
            app.RebuildGraph();
        }

        if (!hasProject) {
            ImGui::TextDisabled("Nenhum projeto aberto.");
        } else {
            DrawNodeGraph(app);
        }
        ImGui::EndTabItem();
    }
}

static void DrawExternalFilesTab(AppState& app) {
    auto label = [&app](const char* withEmoji, const char* plain) {
        return app.emojiEnabled ? withEmoji : plain;
    };

    ImGuiTabItemFlags flagsExt = (app.requestedTab == 4) ? ImGuiTabItemFlags_SetSelected : 0;
    if (ImGui::BeginTabItem(label("📂 External Files", "External Files"), NULL, flagsExt)) {
        if (app.requestedTab == 4) app.requestedTab = -1;
        app.activeTab = 4;

        if (app.externalFiles.empty()) {
            ImGui::TextDisabled("No external files open.");
            ImGui::TextDisabled("Use File > Open File... to open .txt or .md files.");
        } else {
            // File Tabs
            if (ImGui::BeginTabBar("ExternalFilesTabs", ImGuiTabBarFlags_AutoSelectNewTabs)) {
                for (int i = 0; i < (int)app.externalFiles.size(); ++i) {
                    bool open = true;
                    if (ImGui::BeginTabItem(app.externalFiles[i].filename.c_str(), &open)) {
                        app.selectedExternalFileIndex = i;
                        
                        if (ImGui::Button(label("💾 Save", "Save"))) {
                            app.SaveExternalFile(i);
                        }
                        ImGui::SameLine();
                        if (ImGui::Button(label("❌ Close", "Close"))) {
                            open = false;
                        }
                        ImGui::SameLine();
                        if (ImGui::Checkbox(label("👁️ Preview", "Preview"), &app.previewMode)) {
                            // toggle
                        }

                        ImGui::Separator();
                        
                        ExternalFile& file = app.externalFiles[i];

                        if (app.previewMode) {
                            ImGui::BeginChild("ExtPreview", ImVec2(0, -10), true);
                            DrawMarkdownPreview(app, file.content, true);
                            ImGui::EndChild();
                        } else {
                            if (InputTextMultilineString("##exteditor", &file.content, ImVec2(-FLT_MIN, -10))) {
                                file.modified = true;
                            }
                        }
                        
                        ImGui::EndTabItem();
                    }
                    if (!open) {
                        app.externalFiles.erase(app.externalFiles.begin() + i);
                        if (app.selectedExternalFileIndex >= i) app.selectedExternalFileIndex--;
                        i--; // adjust index
                    }
                }
                ImGui::EndTabBar();
            }
        }
        ImGui::EndTabItem();
    }
}

static void DrawMainTabs(AppState& app) {
    if (ImGui::BeginTabBar("MyTabs")) {
        DrawDashboardTab(app);
        DrawKnowledgeTab(app);
        DrawExecutionTab(app);
        DrawGraphTab(app);
        DrawExternalFilesTab(app);
        ImGui::EndTabBar();
    }
}

static void DrawHistoryModal(AppState& app) {
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
            if (dotPos != std::string::npos) {
                base = base.substr(0, dotPos);
            }
            size_t usTime = base.find_last_of('_');
            if (usTime == std::string::npos) {
                return filename;
            }
            std::string timePart = base.substr(usTime + 1);
            std::string beforeTime = base.substr(0, usTime);
            size_t usDate = beforeTime.find_last_of('_');
            if (usDate == std::string::npos) {
                return filename;
            }
            std::string datePart = beforeTime.substr(usDate + 1);
            if (datePart.size() != 8 || timePart.size() != 6) {
                return filename;
            }
            for (char c : datePart) {
                if (c < '0' || c > '9') return filename;
            }
            for (char c : timePart) {
                if (c < '0' || c > '9') return filename;
            }

            std::string formatted;
            formatted.reserve(19);
            formatted.append(datePart.substr(0, 4));
            formatted.push_back('-');
            formatted.append(datePart.substr(4, 2));
            formatted.push_back('-');
            formatted.append(datePart.substr(6, 2));
            formatted.push_back(' ');
            formatted.append(timePart.substr(0, 2));
            formatted.push_back(':');
            formatted.append(timePart.substr(2, 2));
            formatted.push_back(':');
            formatted.append(timePart.substr(4, 2));
            return formatted;
        };

        if (app.historyVersions.empty()) {
            ImGui::Text("Nenhuma versão anterior encontrada para esta nota.");
        } else {
            // Split view: List vs Content
            static float w = 200.0f;
            ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0,0));
            ImGui::BeginChild("HistoryList", ImVec2(w, 0), true);
            
            for (int i = 0; i < (int)app.historyVersions.size(); ++i) {
                bool selected = (app.selectedHistoryIndex == i);
                std::string display = FormatHistoryLabel(app.historyVersions[i]);
                
                if (ImGui::Selectable(display.c_str(), selected)) {
                    app.selectedHistoryIndex = i;
                    // Fetch Content
                    if (app.organizerService) {
                        app.selectedHistoryContent = app.organizerService->getVersionContent(app.historyVersions[i]);
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
            ImGui::PopStyleVar();
        }

        if (app.selectedHistoryIndex >= 0 && !app.selectedHistoryContent.empty() && app.organizerService) {
            if (ImGui::Button("Restaurar esta versao")) {
                app.organizerService->updateNote(app.selectedNoteIdForHistory, app.selectedHistoryContent);
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

static void DrawWorkspace(AppState& app) {
    static float chatHeight = 250.0f;
    const float minChatHeight = 80.0f;
    const float maxChatHeight = 800.0f;

    float availableHeight = ImGui::GetContentRegionAvail().y;
    float workspaceHeight = availableHeight;

    if (app.showConversationPanel) {
        workspaceHeight = availableHeight - chatHeight - 4.0f; // 4px for splitter
        if (workspaceHeight < 150.0f) workspaceHeight = 150.0f; // Minimal workspace
    }

    // Main Workspace Area (Top)
    ImGui::BeginChild("MainWorkspace", ImVec2(0, workspaceHeight), false, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
    DrawMainTabs(app);
    ImGui::EndChild();

    // Splitter & Chat (Bottom)
    if (app.showConversationPanel) {
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.1f, 0.1f, 0.1f, 1.0f));
        ImGui::Button("##Splitter", ImVec2(-1, 4.0f));
        ImGui::PopStyleColor();

        if (ImGui::IsItemHovered()) ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeNS);
        if (ImGui::IsItemActive()) {
            chatHeight -= ImGui::GetIO().MouseDelta.y;
            if (chatHeight < minChatHeight) chatHeight = minChatHeight;
            if (chatHeight > maxChatHeight) chatHeight = maxChatHeight;
        }

        ImGui::BeginChild("ConversationDock", ImVec2(0, 0), true);
        ConversationPanel::DrawContent(app);
        ImGui::EndChild();
    }
}

static void DrawAllModals(AppState& app) {
    DrawProjectModals(app);
    DrawTaskDetailsModal(app);
    DrawHistoryModal(app);
}

static void DrawMainWindow(AppState& app) {
    auto label = [&app](const char* withEmoji, const char* plain) {
        return app.emojiEnabled ? withEmoji : plain;
    };
    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(viewport->WorkPos);
    ImGui::SetNextWindowSize(viewport->WorkSize);
    
    // Begin Main Window
    ImGui::Begin("Main", NULL, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_MenuBar);
    
    DrawMenuBar(app);
    DrawWorkspace(app);
    
    ImGui::End();
}

void DrawUI(AppState& app) {
    if (app.pendingRefresh.exchange(false)) {
        app.RefreshInbox();
        app.RefreshAllInsights();
    }

    DrawMainWindow(app);
    DrawAllModals(app);
    
    // Writing Trajectory Panels
    DrawTrajectoryPanel(app);
    DrawSegmentEditorPanel(app);
    DrawDefensePanel(app);
}
} // namespace ideawalker::ui
