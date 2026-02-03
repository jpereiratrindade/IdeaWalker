#include "ui/panels/MainPanels.hpp"
#include "ui/UiUtils.hpp"
#include "ui/UiMarkdownRenderer.hpp"
#include "application/KnowledgeService.hpp"
#include "domain/Insight.hpp"
#include "imgui.h"
#include <memory>
#include <mutex>

namespace ideawalker::ui {

void DrawKnowledgeTab(AppState& app) {
    auto label = [&app](const char* withEmoji, const char* plain) {
        return app.ui.emojiEnabled ? withEmoji : plain;
    };
    const bool hasProject = (app.services.knowledgeService != nullptr);

    ImGuiTabItemFlags flags1 = (app.ui.requestedTab == 1) ? ImGuiTabItemFlags_SetSelected : 0;
    if (ImGui::BeginTabItem(label("📚 Organized Knowledge", "Organized Knowledge"), NULL, flags1)) {
        if (app.ui.requestedTab == 1) app.ui.requestedTab = -1;
        bool enteringKnowledge = (app.ui.activeTab != 1);
        app.ui.activeTab = 1;
        if (enteringKnowledge && hasProject && !app.ui.isProcessing.load()) {
            app.RefreshAllInsights();
        }
        if (!hasProject) {
            ImGui::TextDisabled("Nenhum projeto aberto.");
            ImGui::TextDisabled("Use File > New Project ou File > Open Project para comecar.");
        } else {
            ImGui::Checkbox("Visao unificada", &app.ui.unifiedKnowledgeView);
            ImGui::Separator();

            if (app.ui.unifiedKnowledgeView) {
                ImGui::SameLine();
                ImGui::Checkbox("Modo Preview", &app.ui.unifiedPreviewMode);

                ImGui::BeginChild("UnifiedKnowledge", ImVec2(0, 0), true);
                if (app.ui.unifiedKnowledge.empty()) {
                    ImGui::TextDisabled("Nenhum insight disponivel.");
                } else {
                    if (app.ui.unifiedPreviewMode) {
                            DrawMarkdownPreview(app, app.ui.unifiedKnowledge, false);
                    } else {
                            InputTextMultilineString("##unifiedRaw", &app.ui.unifiedKnowledge, ImVec2(-FLT_MIN, -FLT_MIN), ImGuiInputTextFlags_ReadOnly);
                    }
                }
                ImGui::EndChild();
            } else {
                // Coluna da Esquerda: Lista de Arquivos
                ImGui::BeginChild("NotesList", ImVec2(250, 0), true);
                for (auto& insight : app.project.allInsights) {
                    ImGui::PushID(insight.getMetadata().id.c_str());

                    // Header with History Button
                    ImGui::BeginGroup();
                    if (ImGui::SmallButton(label("🕰️", "Hist"))) {
                        app.ui.showHistory = true;
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
                        bool isSelected = (app.ui.selectedFilename == filename);
                        if (ImGui::Selectable((filename + "###selectable").c_str(), isSelected)) {
                            app.ui.selectedFilename = filename;
                            app.ui.selectedNoteContent = insight.getContent();
                            std::snprintf(app.ui.saveAsFilename, sizeof(app.ui.saveAsFilename), "%s", filename.c_str());

                            // Update Domain Insight
                            domain::Insight::Metadata meta;
                            meta.id = filename;
                            app.project.currentInsight = std::make_unique<domain::Insight>(meta, app.ui.selectedNoteContent);
                            app.project.currentInsight->parseActionablesFromContent();

                            // Fetch Backlinks
                            if (app.services.knowledgeService) {
                                app.ui.currentBacklinks = app.services.knowledgeService->GetBacklinks(filename);
                            }
                        }
                    }
                    ImGui::PopID();
                }
                ImGui::EndChild();

                ImGui::SameLine();

                // Coluna da Direita: Conteudo / Editor
                ImGui::BeginChild("NoteContent", ImVec2(0, 0), true);
                if (!app.ui.selectedFilename.empty()) {
                    if (ImGui::Button(label("💾 Save Changes", "Save Changes"), ImVec2(150, 30))) {
                        app.services.knowledgeService->UpdateNote(app.ui.selectedFilename, app.ui.selectedNoteContent);
                        app.AppendLog("[SYSTEM] Saved changes to " + app.ui.selectedFilename + "\n");
                        app.RefreshAllInsights();
                    }
                    ImGui::SameLine();

                    ImGui::SetNextItemWidth(200.0f);
                    ImGui::InputText("##saveasname", app.ui.saveAsFilename, sizeof(app.ui.saveAsFilename));
                    ImGui::SameLine();
                    if (ImGui::Button(label("📂 Save As", "Save As"), ImVec2(100, 30))) {
                        std::string newName = app.ui.saveAsFilename;
                        if (!newName.empty()) {
                            if (newName.find(".md") == std::string::npos && newName.find(".txt") == std::string::npos) {
                                newName += ".md";
                            }
                            app.services.knowledgeService->UpdateNote(newName, app.ui.selectedNoteContent);
                            app.ui.selectedFilename = newName;
                            app.AppendLog("[SYSTEM] Saved as " + newName + "\n");
                            app.RefreshAllInsights();
                        }
                    }

                    ImGui::Separator();
                    
                    // Editor / Visual Switcher
                    if (ImGui::BeginTabBar("EditorTabs")) {
                        if (ImGui::BeginTabItem(label("📝 Editor", "Editor"))) {
                            app.ui.previewMode = false;
                            ImGui::EndTabItem();
                        }
                        if (ImGui::BeginTabItem(label("👁️ Visual", "Visual"))) {
                            app.ui.previewMode = true;
                            ImGui::EndTabItem();
                        }
                        ImGui::EndTabBar();
                    }

                    if (app.ui.previewMode) {
                        ImGui::BeginChild("PreviewScroll", ImVec2(0, -200), true);
                        DrawMarkdownPreview(app, app.ui.selectedNoteContent, false);
                        ImGui::EndChild();
                    } else {
                        // Editor de texto (InputTextMultiline com Resize)
                        if (InputTextMultilineString("##editor", &app.ui.selectedNoteContent, ImVec2(-FLT_MIN, -200))) {
                            if (app.project.currentInsight) {
                                app.project.currentInsight->setContent(app.ui.selectedNoteContent);
                                app.project.currentInsight->parseActionablesFromContent();
                            }
                        }
                    }
                    
                    ImGui::Separator();
                    ImGui::Text("%s", label("🔗 Backlinks (Mencionado em):", "Backlinks (Mencionado em):"));
                    if (app.ui.currentBacklinks.empty()) {
                        ImGui::TextDisabled("Nenhuma referencia encontrada.");
                    } else {
                        for (const auto& link : app.ui.currentBacklinks) {
                            if (ImGui::Button(link.c_str())) {
                                app.AppendLog("[UI] Jumping to " + link + "\n");
                                app.ui.selectedFilename = link;
                                if (app.services.knowledgeService) {
                                    app.ui.selectedNoteContent = app.services.knowledgeService->GetNoteContent(link);
                                    std::snprintf(app.ui.saveAsFilename, sizeof(app.ui.saveAsFilename), "%s", link.c_str());
                                    
                                    domain::Insight::Metadata meta;
                                    meta.id = link;
                                    app.project.currentInsight = std::make_unique<domain::Insight>(meta, app.ui.selectedNoteContent);
                                    app.project.currentInsight->parseActionablesFromContent();
                                    app.ui.currentBacklinks = app.services.knowledgeService->GetBacklinks(link);
                                    app.AnalyzeSuggestions();
                                }
                            }
                            ImGui::SameLine();
                        }
                        ImGui::NewLine();
                    }

                    ImGui::Separator();
                    ImGui::Text("%s", label("🧠 Ressonância Semântica (Sugestões):", "Semantic Resonance (Suggestions):"));
                    if (app.ui.isAnalyzingSuggestions) {
                        ImGui::TextDisabled("Analisando conexões...");
                    } else if (app.ui.currentSuggestions.empty()) {
                        ImGui::TextDisabled("Nenhuma conexão óbvia detectada.");
                    } else {
                        std::lock_guard<std::mutex> lock(app.ui.suggestionsMutex);
                        for (const auto& sug : app.ui.currentSuggestions) {
                            std::string sugLabel = sug.targetId + " (" + (sug.reasons.empty() ? "" : sug.reasons[0].evidence) + ")";
                            if (ImGui::Button(sugLabel.c_str())) {
                                app.ui.selectedNoteContent += "\n\n[[" + sug.targetId + "]]";
                                if (app.project.currentInsight) {
                                    app.project.currentInsight->setContent(app.ui.selectedNoteContent);
                                    // Auto-save to disk to ensure backlinks are detectable immediately
                                    if (app.services.knowledgeService) {
                                        app.services.knowledgeService->UpdateNote(app.ui.selectedFilename, app.ui.selectedNoteContent);
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

} // namespace ideawalker::ui
