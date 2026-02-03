#include "ui/panels/MainPanels.hpp"
#include "ui/UiUtils.hpp"
#include "ui/UiMarkdownRenderer.hpp"
#include "application/OrganizerService.hpp"
#include "application/DocumentIngestionService.hpp"
#include "imgui.h"
#include <iostream>
#include <thread>
#include <chrono>

namespace ideawalker::ui {

void DrawDashboardTab(AppState& app) {
    auto label = [&app](const char* withEmoji, const char* plain) {
        return app.emojiEnabled ? withEmoji : plain;
    };
    const bool hasProject = (app.services.organizerService != nullptr);

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
                std::cout << "[Organizer] " << s << std::endl;

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
                    app.services.organizerService->processInbox(force, app.fastMode, statusHandler);
                    bool consolidated = app.services.organizerService->updateConsolidatedTasks();
                    app.isProcessing.store(false);
                    app.SetProcessingStatus("Thinking..."); 
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
                    auto result = app.services.organizerService->processInboxItem(filename, force, app.fastMode, statusHandler);
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
                        bool consolidated = app.services.organizerService->updateConsolidatedTasks();
                        app.AppendLog(consolidated ? "[SYSTEM] Consolidated tasks updated.\n" : "[SYSTEM] Consolidated tasks failed.\n");
                    }
                    app.isProcessing.store(false);
                    app.SetProcessingStatus("Thinking..."); 
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
            if (app.services.ingestionService) {
                if (ImGui::Button("Sincronizar Inbox & Gerar Observações")) {
                    std::thread([&app]() {
                        app.isProcessing.store(true);
                        app.services.ingestionService->ingestPending([&app](const std::string& s) {
                            app.SetProcessingStatus(s);
                            app.AppendLog("[INGEST] " + s + "\n");
                        });
                        app.isProcessing.store(false);
                        app.pendingRefresh.store(true);
                    }).detach();
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

} // namespace ideawalker::ui
