#include "ui/panels/MainPanels.hpp"
#include "ui/UiUtils.hpp"
#include "ui/UiMarkdownRenderer.hpp"
#include "application/KnowledgeService.hpp"
#include "domain/Insight.hpp"
#include "imgui.h"
#include <memory>
#include <mutex>

namespace ideawalker::ui {

void DrawKnowledgeTab(application::AppController& app) {
    auto label = [&app](const char* withEmoji, const char* plain) {
        return app.GetViewModel().ui.emojiEnabled ? withEmoji : plain;
    };
    const bool hasProject = (app.GetServices().knowledgeService != nullptr);

    ImGuiTabItemFlags flags1 = (app.GetViewModel().ui.requestedTab == 1) ? ImGuiTabItemFlags_SetSelected : 0;
    if (ImGui::BeginTabItem(label("📚 Organized Knowledge", "Organized Knowledge"), NULL, flags1)) {
        if (app.GetViewModel().ui.requestedTab == 1) app.GetViewModel().ui.requestedTab = -1;
        bool enteringKnowledge = (app.GetViewModel().ui.activeTab != 1);
        app.GetViewModel().ui.activeTab = 1;
        if (enteringKnowledge && hasProject && !app.GetViewModel().ui.isProcessing.load()) {
            app.RefreshAllInsights();
        }
        if (!hasProject) {
            ImGui::TextDisabled("Nenhum projeto aberto.");
            ImGui::TextDisabled("Use File > New Project ou File > Open Project para comecar.");
        } else {
            ImGui::Checkbox("Visao unificada", &app.GetViewModel().ui.unifiedKnowledgeView);
            ImGui::Separator();

            if (app.GetViewModel().ui.unifiedKnowledgeView) {
                ImGui::SameLine();
                ImGui::Checkbox("Modo Preview", &app.GetViewModel().ui.unifiedPreviewMode);

                ImGui::BeginChild("UnifiedKnowledge", ImVec2(0, 0), true);
                if (app.GetViewModel().ui.unifiedKnowledge.empty()) {
                    ImGui::TextDisabled("Nenhum insight disponivel.");
                } else {
                    if (app.GetViewModel().ui.unifiedPreviewMode) {
                            DrawMarkdownPreview(app, app.GetViewModel().ui.unifiedKnowledge, false);
                    } else {
                            InputTextMultilineString("##unifiedRaw", &app.GetViewModel().ui.unifiedKnowledge, ImVec2(-FLT_MIN, -FLT_MIN), ImGuiInputTextFlags_ReadOnly);
                    }
                }
                ImGui::EndChild();
            } else {
                // Coluna da Esquerda: Lista de Arquivos
                ImGui::BeginChild("NotesList", ImVec2(250, 0), true);
                
                // UX: Search filter
                static char searchFilter[128] = "";
                ImGui::PushItemWidth(-1.0f);
                ImGui::InputTextWithHint("##Search", label("🔍 Buscar notas...", "Buscar notas..."), searchFilter, sizeof(searchFilter));
                ImGui::PopItemWidth();
                ImGui::Separator();
                
                std::string filterLower = searchFilter;
                std::transform(filterLower.begin(), filterLower.end(), filterLower.begin(), ::tolower);

                for (auto& insight : app.GetViewModel().project.allInsights) {
                    std::string title = insight.getMetadata().title.empty() ? insight.getMetadata().id : insight.getMetadata().title;
                    
                    if (!filterLower.empty()) {
                        std::string titleLower = title;
                        std::transform(titleLower.begin(), titleLower.end(), titleLower.begin(), ::tolower);
                        std::string filenameLower = insight.getMetadata().id;
                        std::transform(filenameLower.begin(), filenameLower.end(), filenameLower.begin(), ::tolower);
                        if (titleLower.find(filterLower) == std::string::npos && filenameLower.find(filterLower) == std::string::npos) {
                            continue; // Skip this item if it doesn't match the search
                        }
                    }

                    ImGui::PushID(insight.getMetadata().id.c_str());

                    // Header with History Button
                    ImGui::BeginGroup();
                    if (ImGui::SmallButton(label("🕰️", "Hist"))) {
                        app.GetViewModel().ui.showHistory = true;
                        app.LoadHistory(insight.getMetadata().id);
                    }
                    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Ver Trajetória (Histórico)");
                    ImGui::SameLine(0.0f, 6.0f);

                    bool open = ImGui::CollapsingHeader(
                        (title + "###header").c_str(),
                        ImGuiTreeNodeFlags_DefaultOpen
                    );
                    ImGui::EndGroup();

                    if (open) {
                        const std::string& filename = insight.getMetadata().id;
                        bool isSelected = (app.GetViewModel().ui.selectedFilename == filename);
                        if (ImGui::Selectable((filename + "###selectable").c_str(), isSelected)) {
                            app.GetViewModel().ui.selectedFilename = filename;
                            app.GetViewModel().ui.selectedNoteContent = insight.getContent();
                            std::snprintf(app.GetViewModel().ui.saveAsFilename, sizeof(app.GetViewModel().ui.saveAsFilename), "%s", filename.c_str());

                            // Update Domain Insight
                            domain::Insight::Metadata meta;
                            meta.id = filename;
                            app.GetViewModel().project.currentInsight = std::make_unique<domain::Insight>(meta, app.GetViewModel().ui.selectedNoteContent);
                            app.GetViewModel().project.currentInsight->parseActionablesFromContent();

                            // Fetch Backlinks
                            if (app.GetServices().knowledgeService) {
                                app.GetViewModel().ui.currentBacklinks = app.GetServices().knowledgeService->GetBacklinks(filename);
                            }
                        }
                    }
                    ImGui::PopID();
                }
                ImGui::EndChild();

                ImGui::SameLine();

                // Coluna da Direita: Conteudo / Editor
                ImGui::BeginChild("NoteContent", ImVec2(0, 0), true);
                if (!app.GetViewModel().ui.selectedFilename.empty()) {
                    auto doSave = [&app]() {
                        app.GetServices().knowledgeService->UpdateNote(app.GetViewModel().ui.selectedFilename, app.GetViewModel().ui.selectedNoteContent);
                        app.GetViewModel().AppendLog("[SYSTEM] Saved changes to " + app.GetViewModel().ui.selectedFilename + "\n");
                        app.RefreshAllInsights();
                    };

                    // UX: Keyboard shortcut Ctrl+S
                    if (ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows) && 
                        ImGui::GetIO().KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_S)) {
                        doSave();
                    }

                    if (ImGui::Button(label("💾 Save Changes", "Save Changes"), ImVec2(150, 30))) {
                        doSave();
                    }
                    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Atalho: Ctrl+S");
                    ImGui::SameLine();

                    ImGui::SetNextItemWidth(200.0f);
                    ImGui::InputText("##saveasname", app.GetViewModel().ui.saveAsFilename, sizeof(app.GetViewModel().ui.saveAsFilename));
                    ImGui::SameLine();
                    if (ImGui::Button(label("📂 Save As", "Save As"), ImVec2(100, 30))) {
                        std::string newName = app.GetViewModel().ui.saveAsFilename;
                        if (!newName.empty()) {
                            if (newName.find(".md") == std::string::npos && newName.find(".txt") == std::string::npos) {
                                newName += ".md";
                            }
                            app.GetServices().knowledgeService->UpdateNote(newName, app.GetViewModel().ui.selectedNoteContent);
                            app.GetViewModel().ui.selectedFilename = newName;
                            app.GetViewModel().AppendLog("[SYSTEM] Saved as " + newName + "\n");
                            app.RefreshAllInsights();
                        }
                    }

                    ImGui::Separator();
                    
                    // Editor / Visual Switcher
                    if (ImGui::BeginTabBar("EditorTabs")) {
                        if (ImGui::BeginTabItem(label("📝 Editor", "Editor"))) {
                            app.GetViewModel().ui.previewMode = false;
                            ImGui::EndTabItem();
                        }
                        if (ImGui::BeginTabItem(label("👁️ Visual", "Visual"))) {
                            app.GetViewModel().ui.previewMode = true;
                            ImGui::EndTabItem();
                        }
                        ImGui::EndTabBar();
                    }

                    if (app.GetViewModel().ui.previewMode) {
                        ImGui::BeginChild("PreviewScroll", ImVec2(0, -200), true);
                        DrawMarkdownPreview(app, app.GetViewModel().ui.selectedNoteContent, false);
                        ImGui::EndChild();
                    } else {
                        // Editor de texto (InputTextMultiline com Resize)
                        if (InputTextMultilineString("##editor", &app.GetViewModel().ui.selectedNoteContent, ImVec2(-FLT_MIN, -200))) {
                            if (app.GetViewModel().project.currentInsight) {
                                app.GetViewModel().project.currentInsight->setContent(app.GetViewModel().ui.selectedNoteContent);
                                app.GetViewModel().project.currentInsight->parseActionablesFromContent();
                            }
                        }
                    }
                    
                    ImGui::Separator();
                    ImGui::Text("%s", label("🔗 Backlinks (Mencionado em):", "Backlinks (Mencionado em):"));
                    if (app.GetViewModel().ui.currentBacklinks.empty()) {
                        ImGui::TextDisabled("Nenhuma referencia encontrada.");
                    } else {
                        for (const auto& link : app.GetViewModel().ui.currentBacklinks) {
                            if (ImGui::Button(link.c_str())) {
                                app.GetViewModel().AppendLog("[UI] Jumping to " + link + "\n");
                                app.GetViewModel().ui.selectedFilename = link;
                                if (app.GetServices().knowledgeService) {
                                    app.GetViewModel().ui.selectedNoteContent = app.GetServices().knowledgeService->GetNoteContent(link);
                                    std::snprintf(app.GetViewModel().ui.saveAsFilename, sizeof(app.GetViewModel().ui.saveAsFilename), "%s", link.c_str());
                                    
                                    domain::Insight::Metadata meta;
                                    meta.id = link;
                                    app.GetViewModel().project.currentInsight = std::make_unique<domain::Insight>(meta, app.GetViewModel().ui.selectedNoteContent);
                                    app.GetViewModel().project.currentInsight->parseActionablesFromContent();
                                    app.GetViewModel().ui.currentBacklinks = app.GetServices().knowledgeService->GetBacklinks(link);
                                    app.AnalyzeSuggestions();
                                }
                            }
                            ImGui::SameLine();
                        }
                        ImGui::NewLine();
                    }

                    ImGui::Separator();
                    ImGui::Text("%s", label("🧠 Ressonância Semântica (Sugestões):", "Semantic Resonance (Suggestions):"));
                    if (app.GetViewModel().ui.isAnalyzingSuggestions) {
                        ImGui::TextDisabled("Analisando conexões...");
                    } else if (app.GetViewModel().ui.currentSuggestions.empty()) {
                        ImGui::TextDisabled("Nenhuma conexão óbvia detectada.");
                    } else {
                        std::lock_guard<std::mutex> lock(app.GetViewModel().ui.suggestionsMutex);
                        for (const auto& sug : app.GetViewModel().ui.currentSuggestions) {
                            std::string sugLabel = sug.targetId + " (" + (sug.reasons.empty() ? "" : sug.reasons[0].evidence) + ")";
                            if (ImGui::Button(sugLabel.c_str())) {
                                app.GetViewModel().ui.selectedNoteContent += "\n\n[[" + sug.targetId + "]]";
                                if (app.GetViewModel().project.currentInsight) {
                                    app.GetViewModel().project.currentInsight->setContent(app.GetViewModel().ui.selectedNoteContent);
                                    // Auto-save to disk to ensure backlinks are detectable immediately
                                    if (app.GetServices().knowledgeService) {
                                        app.GetServices().knowledgeService->UpdateNote(app.GetViewModel().ui.selectedFilename, app.GetViewModel().ui.selectedNoteContent);
                                    }
                                }
                                app.GetViewModel().AppendLog("[UI] Conectado a: " + sug.targetId + "\n");
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

} // namespace ideawalker::ui
