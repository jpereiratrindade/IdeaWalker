#include "WritingPanels.hpp"
#include "../../ui/AppState.hpp"
#include "../../domain/writing/WritingTrajectory.hpp"
#include "../../domain/writing/services/DefensePromptFactory.hpp"
#include "../../domain/writing/services/CoherenceLensService.hpp"
#include "imgui.h"
#include <iostream>

namespace ideawalker::ui {

using namespace ideawalker::domain::writing;

// Helper to generate UUIDs (replicated for UI simplicity)
static std::string generateDefenseUUID() {
    static const char alphanum[] = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";
    std::string s;
    s.reserve(8);
    for (int i = 0; i < 8; ++i) s += alphanum[rand() % (sizeof(alphanum) - 1)];
    return s;
}

void DrawDefensePanel(AppState& state) {
    if (!state.showDefensePanel) return;

    ImGui::SetNextWindowSize(ImVec2(600, 500), ImGuiCond_FirstUseEver);
    if (ImGui::Begin("Defense Mode", &state.showDefensePanel)) {
        
        if (state.activeTrajectoryId.empty()) {
            ImGui::Text("No active trajectory selected.");
            ImGui::End();
            return;
        }

        auto trajPtr = state.writingTrajectoryService->getTrajectory(state.activeTrajectoryId);
        if (!trajPtr) {
            ImGui::Text("Trajectory not found.");
            ImGui::End();
            return;
        }

        ImGui::TextDisabled("Trajectory: %s", trajPtr->getIntent().purpose.c_str());
        ImGui::Separator();

        // 0. Coherence Lens
        if (ImGui::Button("Run Coherence Lens")) {
            state.coherenceIssues = CoherenceLensService::analyze(*trajPtr);
        }
        
        if (!state.coherenceIssues.empty()) {
            ImGui::TextColored(ImVec4(1, 0.5f, 0, 1), "Coherence Issues Detected (%zu):", state.coherenceIssues.size());
            for (const auto& issue : state.coherenceIssues) {
                ImGui::BulletText("[%s] %s (%s)", issue.severity.c_str(), issue.description.c_str(), issue.type.c_str());
            }
            ImGui::Separator();
        }

        // 1. Actions
        if (ImGui::Button("Generate Defense Prompts")) {
            auto prompts = DefensePromptFactory::generatePrompts(*trajPtr);
            for (const auto& p : prompts) {
                // Check if already exists? For MVP just add.
                state.writingTrajectoryService->addDefenseCard(
                    state.activeTrajectoryId, 
                    generateDefenseUUID(), 
                    p.segmentId, 
                    p.prompt, 
                    p.expectedDefensePoints
                );
            }
        }

        ImGui::Separator();

        // 2. Cards List
        const auto& cards = trajPtr->getDefenseCards();
        if (cards.empty()) {
            ImGui::Text("No defense cards generated yet.");
        } else {
            for (const auto& card : cards) {
                ImGui::PushID(card.cardId.c_str());
                
                // Color coding based on status
                ImVec4 color = ImVec4(1,1,1,1);
                std::string statusStr = "Pending";
                if (card.status == DefenseStatus::Rehearsed) { color = ImVec4(1, 1, 0, 1); statusStr = "Rehearsed"; }
                if (card.status == DefenseStatus::Passed) { color = ImVec4(0, 1, 0, 1); statusStr = "Passed"; }

                ImGui::TextColored(color, "[%s]", statusStr.c_str());
                ImGui::SameLine();
                
                // Truncate long prompts for the header
                std::string headerLabel = card.prompt;
                if (headerLabel.length() > 80) headerLabel = headerLabel.substr(0, 77) + "...";
                
                if (ImGui::CollapsingHeader(headerLabel.c_str(), ImGuiTreeNodeFlags_DefaultOpen)) {
                    // Show full prompt wrapped
                    ImGui::PushTextWrapPos(0.0f);
                    ImGui::TextUnformatted(card.prompt.c_str());
                    ImGui::PopTextWrapPos();
                    ImGui::Separator();
                    
                    ImGui::Text("Expected Points:");
                    for(const auto& pt : card.expectedDefensePoints) {
                        ImGui::BulletText("%s", pt.c_str());
                    }

                    if (card.status != DefenseStatus::Passed) {
                        static char defenseBuf[1024] = "";
                        ImGui::Text("Your Defense:");
                        ImGui::InputTextMultiline(("##response" + card.cardId).c_str(), defenseBuf, sizeof(defenseBuf), ImVec2(-FLT_MIN, 100));
                        
                        if (ImGui::Button("Mark Rehearsed")) {
                            state.writingTrajectoryService->updateDefenseStatus(state.activeTrajectoryId, card.cardId, DefenseStatus::Rehearsed, defenseBuf);
                        }
                        ImGui::SameLine();
                        if (ImGui::Button("Pass Defense")) {
                            state.writingTrajectoryService->updateDefenseStatus(state.activeTrajectoryId, card.cardId, DefenseStatus::Passed, "Passed via UI");
                        }
                    } else {
                        ImGui::TextWrapped("Defense Passed! (Locked)");
                    }
                }
                ImGui::Separator();
                ImGui::PopID();
            }
        }

    }
    ImGui::End();
}

} // namespace ideawalker::ui
