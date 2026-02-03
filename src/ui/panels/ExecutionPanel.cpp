#include "ui/panels/MainPanels.hpp"
#include "ui/UiUtils.hpp"
#include "application/KnowledgeService.hpp"
#include "domain/Insight.hpp"
#include "imgui.h"
#include <vector>
#include <string>

namespace ideawalker::ui {

void DrawExecutionTab(AppState& app) {
    auto label = [&app](const char* withEmoji, const char* plain) {
        return app.ui.emojiEnabled ? withEmoji : plain;
    };
    const bool hasProject = (app.services.knowledgeService != nullptr);

    ImGuiTabItemFlags flags2 = (app.ui.requestedTab == 2) ? ImGuiTabItemFlags_SetSelected : 0;
    if (ImGui::BeginTabItem(label("🏭 Execução", "Execução"), NULL, flags2)) {
        if (app.ui.requestedTab == 2) app.ui.requestedTab = -1;
        app.ui.activeTab = 2;
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

            const bool useConsolidatedTasks = app.project.consolidatedInsight && !app.project.consolidatedInsight->getActionables().empty();
            std::vector<const domain::Insight*> taskSources;
            if (useConsolidatedTasks) {
                taskSources.push_back(app.project.consolidatedInsight.get());
            } else {
                for (const auto& insight : app.project.allInsights) {
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
                            app.ui.selectedFilename = insight->getMetadata().id;
                            app.ui.selectedNoteContent = insight->getContent();

                            domain::Insight::Metadata meta;
                            meta.id = app.ui.selectedFilename;
                            app.project.currentInsight = std::make_unique<domain::Insight>(meta, app.ui.selectedNoteContent);
                            app.project.currentInsight->parseActionablesFromContent();
                        }
                        
                        if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
                            app.ui.showTaskDetails = true;
                            app.ui.selectedTaskTitle = "Detalhes da Tarefa";
                            app.ui.selectedTaskContent = actionables[i].description;
                            app.ui.selectedTaskOrigin = insight->getMetadata().id;
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
                if (app.services.knowledgeService && ImGui::BeginDragDropTarget()) {
                    if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("DND_TASK")) {
                        std::string data = static_cast<const char*>(payload->Data);
                        size_t sep = data.find('|');
                        std::string filename = data.substr(0, sep);
                        int index = std::stoi(data.substr(sep + 1));
                        app.services.knowledgeService->SetTaskStatus(filename, index, completed, inProgress);
                        app.ui.pendingRefresh.store(true);
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

} // namespace ideawalker::ui
