#include "ui/panels/MainPanels.hpp"
#include "ui/UiUtils.hpp"
#include "ui/UiMarkdownRenderer.hpp"
#include "application/KnowledgeService.hpp"
#include "application/AIProcessingService.hpp"
#include "application/DocumentIngestionService.hpp"
#include "imgui.h"
#include <iostream>
#include <thread>
#include <chrono>

namespace ideawalker::ui {

void DrawDashboardTab(AppState& app) {
    auto label = [&app](const char* withEmoji, const char* plain) {
        return app.ui.emojiEnabled ? withEmoji : plain;
    };
    const bool hasProject = (app.services.knowledgeService != nullptr);

    ImGuiTabItemFlags flags0 = (app.ui.requestedTab == 0) ? ImGuiTabItemFlags_SetSelected : 0;
    if (ImGui::BeginTabItem(label("🎙️ Dashboard & Inbox", "Dashboard & Inbox"), NULL, flags0)) {
        if (app.ui.requestedTab == 0) app.ui.requestedTab = -1;
        app.ui.activeTab = 0;
        if (!hasProject) {
            ImGui::TextDisabled("Nenhum projeto aberto.");
            ImGui::TextDisabled("Use File > New Project ou File > Open Project para comecar.");
        } else {
            ImGui::Spacing();
            
            if (ImGui::Button(label("🔄 Refresh Inbox", "Refresh Inbox"), ImVec2(150, 30))) {
                app.RefreshInbox();
            }
            
            ImGui::Separator();
            
            ImGui::Text("Entrada (%zu ideias):", app.project.inboxThoughts.size());
            ImGui::BeginChild("InboxList", ImVec2(0, 200), true);
            for (const auto& thought : app.project.inboxThoughts) {
                bool isSelected = (app.ui.selectedInboxFilename == thought.filename);
                if (ImGui::Selectable(thought.filename.c_str(), isSelected)) {
                    app.ui.selectedInboxFilename = thought.filename;
                }
            }
            ImGui::EndChild();

            ImGui::Spacing();
            
            bool processing = app.ui.isProcessing.load();
            if (processing) ImGui::BeginDisabled();
            auto statusHandler = [&app](const std::string& s) {
                app.SetProcessingStatus(s);
                std::cout << "[Organizer] " << s << std::endl;

                if (s.find("Executando:") != std::string::npos || 
                    s.find("Orquestrador:") != std::string::npos ||
                    s.find("Persona:") != std::string::npos) {
                    app.AppendLog("[AI] " + s + "\n");
                }
            };

            auto startBatch = [&app](bool force) {
                app.AppendLog(force ? "[SYSTEM] Starting AI reprocess (batch)...\n" : "[SYSTEM] Starting AI batch processing...\n");
                app.services.aiProcessingService->ProcessInboxAsync(force, app.ui.fastMode);
            };

            auto startSingle = [&app](const std::string& filename, bool force) {
                app.AppendLog(force ? "[SYSTEM] Starting AI reprocess for " + filename + "...\n"
                                    : "[SYSTEM] Starting AI processing for " + filename + "...\n");
                app.services.aiProcessingService->ProcessItemAsync(filename, force, app.ui.fastMode);
            };

            const bool hasSelection = !app.ui.selectedInboxFilename.empty();
            const char* runLabel = hasSelection
                ? label("🧠 Run Selected", "Run Selected")
                : label("🧠 Run AI Orchestrator", "Run AI Orchestrator");
            if (ImGui::Button(runLabel, ImVec2(250, 50))) {
                if (hasSelection) {
                    startSingle(app.ui.selectedInboxFilename, false);
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
                    startSingle(app.ui.selectedInboxFilename, true);
                }
            } else {
                ImGui::SameLine();
                if (ImGui::Button(label("🔁 Reprocess All", "Reprocess All"), ImVec2(150, 50))) {
                    startBatch(true);
                }
            }
            if (processing) ImGui::EndDisabled();

            if (app.services.taskManager) {
                auto activeTasks = app.services.taskManager->GetActiveTasks();
                for (const auto& task : activeTasks) {
                    ImGui::SameLine();
                    ImGui::TextColored(ImVec4(1, 1, 0, 1), "⏳ [%s] %.0f%%", task->description.c_str(), task->progress.load() * 100.0f);
                }
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
            if (app.services.ingestionService) {
                if (ImGui::Button("Sincronizar Inbox & Gerar Observações")) {
                    app.AppendLog("[SYSTEM] Starting document ingestion...\n");
                    app.services.taskManager->SubmitTask(application::TaskType::Indexing, "Ingestão de Documentos", [&app](std::shared_ptr<application::TaskStatus> status) {
                        app.services.ingestionService->ingestPending([&app, status](const std::string& s) {
                            app.SetProcessingStatus(s);
                            app.AppendLog("[INGEST] " + s + "\n");
                        });
                        app.ui.pendingRefresh.store(true);
                    });
                }
                ImGui::SameLine();
                if (ImGui::IsItemHovered()) ImGui::SetTooltip("Processa .txt, .md, .pdf, .tex na /inbox sem alterar originais.");

                auto observations = app.services.ingestionService->getObservations();
                ImGui::Text("Observações geradas: %zu", observations.size());
                if (ImGui::IsItemHovered()) {
                    ImGui::SetTooltip("Total de registros sintéticos criados a partir dos documentos processados na Inbox.");
                }
            }

            ImGui::Separator();
            ImGui::Text("%s", label("🔥 Activity Heatmap (Last 30 Days)", "Activity Heatmap (Last 30 Days)"));
            
            ImDrawList* draw_list = ImGui::GetWindowDrawList();
            ImVec2 p = ImGui::GetCursorScreenPos();
            float size = 15.0f;
            float spacing = 3.0f;
            int maxCount = 0;
            for (const auto& entry : app.project.activityHistory) {
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
                auto it = app.project.activityHistory.find(buffer);
                int count = (it != app.project.activityHistory.end()) ? it->second : 0;
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

} // namespace ideawalker::ui
