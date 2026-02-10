#include "ui/panels/MainPanels.hpp"
#include "ui/UiUtils.hpp"
#include "application/AIProcessingService.hpp"
#include "application/scientific/ScientificIngestionService.hpp"
#include "imgui.h"
#include <iostream>

namespace ideawalker::ui {

void DrawScientificTab(AppState& app) {
    auto label = [&app](const char* withEmoji, const char* plain) {
        return app.ui.emojiEnabled ? withEmoji : plain;
    };

    if (ImGui::BeginTabItem(label("🧪 Scientific", "Scientific"))) {
        app.ui.activeTab = 5; // Assign a new ID for this tab

        if (!app.services.scientificIngestionService) {
            ImGui::TextDisabled("Serviço de Ingestão Científica não disponível.");
            ImGui::EndTabItem();
            return;
        }

        // --- Toolbar ---
        if (ImGui::Button("♻️ Refresh Inbox")) {
            app.ui.scientificInboxArtifacts = app.services.scientificIngestionService->listInboxArtifacts();
            app.ui.scientificInboxSelected.clear();
            app.ui.scientificInboxLoaded = true;
        }
        ImGui::SameLine();
        if (ImGui::Button("Process All")) {
             app.AppendLog("[SYSTEM] Starting scientific ingestion (All)...\n");
             app.services.taskManager->SubmitTask(application::TaskType::Indexing, "Ingestão Científica", [&app](std::shared_ptr<application::TaskStatus> status) {
                auto result = app.services.scientificIngestionService->ingestPending([&app, status](const std::string& s) {
                    app.SetProcessingStatus(s);
                    app.AppendLog("[SCIENTIFIC] " + s + "\n");
                });
                for (const auto& err : result.errors) {
                    app.AppendLog("[SCIENTIFIC][ERRO] " + err + "\n");
                }
                app.ui.pendingRefresh.store(true);
            });
        }
        ImGui::SameLine();
        
        bool hasSelection = !app.ui.scientificInboxSelected.empty();
        if (!hasSelection) ImGui::BeginDisabled();
        if (ImGui::Button("Process Selected")) {
             std::vector<domain::SourceArtifact> selected;
            for (const auto& artifact : app.ui.scientificInboxArtifacts) {
                if (app.ui.scientificInboxSelected.count(artifact.path) > 0) {
                    selected.push_back(artifact);
                }
            }
            if (!selected.empty()) {
                app.AppendLog("[SCIENTIFIC] Starting selected scientific ingestion...\n");
                app.services.taskManager->SubmitTask(application::TaskType::Indexing, "Ingestão Científica (Selecionados)", [selected, &app](std::shared_ptr<application::TaskStatus> status) {
                    auto result = app.services.scientificIngestionService->ingestSelected(selected, false, [&app, status](const std::string& s) {
                        app.SetProcessingStatus(s);
                        app.AppendLog("[SCIENTIFIC] " + s + "\n");
                    });
                    for (const auto& err : result.errors) {
                        app.AppendLog("[SCIENTIFIC][ERRO] " + err + "\n");
                    }
                    app.ui.pendingRefresh.store(true);
                });
            }
        }
        if (!hasSelection) ImGui::EndDisabled();

        ImGui::Separator();

        // --- Split View ---
        static float leftWidth = 400.0f;
        float availableWidth = ImGui::GetContentRegionAvail().x;
        
        // Left Column: Inbox List
        ImGui::BeginChild("ScientificLeft", ImVec2(leftWidth, 0), true);
        ImGui::Text("📥 Inbox (%zu files)", app.ui.scientificInboxArtifacts.size());
        ImGui::Separator();
        
        if (!app.ui.scientificInboxLoaded) {
             app.ui.scientificInboxArtifacts = app.services.scientificIngestionService->listInboxArtifacts();
             app.ui.scientificInboxLoaded = true;
        }

        for (const auto& artifact : app.ui.scientificInboxArtifacts) {
            bool selected = app.ui.scientificInboxSelected.count(artifact.path) > 0;
            ImGui::PushID(artifact.path.c_str());
            if (ImGui::Checkbox("##select", &selected)) {
                if (selected) app.ui.scientificInboxSelected.insert(artifact.path);
                else app.ui.scientificInboxSelected.erase(artifact.path);
            }
            ImGui::SameLine();
            if (ImGui::Selectable(artifact.filename.c_str(), false)) {
                // Click to maybe preview content in future
            }
            ImGui::PopID();
        }
        ImGui::EndChild();

        ImGui::SameLine();
        
        // Splitter
        ImGui::Button("##vsplitter", ImVec2(4.0f, -1));
        if (ImGui::IsItemActive()) {
            leftWidth += ImGui::GetIO().MouseDelta.x;
            if (leftWidth < 100.0f) leftWidth = 100.0f;
            if (leftWidth > availableWidth - 100.0f) leftWidth = availableWidth - 100.0f;
        }
        if (ImGui::IsItemHovered()) ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeEW);

        ImGui::SameLine();

        // Right Column: Details & Validation
        ImGui::BeginChild("ScientificRight", ImVec2(0, 0), true);
        ImGui::Text("📊 Pipeline Status");
        ImGui::Separator();

        ImGui::Text("Bundles Generated: %zu", app.services.scientificIngestionService->getBundlesCount());
        
        if (auto summary = app.services.scientificIngestionService->getLatestValidationSummary()) {
            ImGui::Spacing();
            ImGui::TextColored(ImVec4(0,1,1,1), "📝 Latest Validation Report:");
            ImGui::Text("Status: %s", summary->status.c_str());
            ImGui::Text("Export Allowed: %s", summary->exportAllowed ? "YES" : "NO");
            
            if (summary->errorCount > 0) 
                ImGui::TextColored(ImVec4(1,0,0,1), "Errors: %zu", summary->errorCount);
            else 
                ImGui::Text("Errors: 0");
                
            if (summary->warningCount > 0)
                ImGui::TextColored(ImVec4(1,1,0,1), "Warnings: %zu", summary->warningCount);
            else
                ImGui::Text("Warnings: 0");

            ImGui::Spacing();
            ImGui::TextWrapped("Report Path: %s", summary->path.c_str());
            
            if (ImGui::Button("Print Full Report to Log")) {
                 app.AppendLog("[VE-IW] " + summary->path + "\n");
                 app.AppendLog(summary->reportJson + "\n");
            }
        } else {
            ImGui::TextDisabled("No validation report available.");
        }

        ImGui::EndChild();

        ImGui::EndTabItem();
    }
}

} // namespace ideawalker::ui
