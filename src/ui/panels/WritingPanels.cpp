/**
 * @file WritingPanels.cpp
 * @brief Implementation of Writing Trajectory UI Panels.
 */

#include "WritingPanels.hpp"
#include "imgui.h"
#include <vector>
#include <string>

namespace ideawalker::ui {

using namespace ideawalker::domain::writing;

// Helper for inputs
static char purposeBuf[256] = "";
static char audienceBuf[128] = "";
static char claimBuf[512] = "";

static char segmentTitleBuf[128] = "";
static char segmentContentBuf[4096 * 4] = ""; // 16KB buffer for content

static char rationaleBuf[512] = "";
static int selectedOp = 0; // index for revision op

void DrawTrajectoryPanel(AppState& state) {
    if (!state.showTrajectoryPanel) return;

    ImGui::Begin("Writing Trajectories", &state.showTrajectoryPanel);

    if (ImGui::Button("New Trajectory")) {
        ImGui::OpenPopup("CreateTrajectoryPopup");
    }

    ImGui::Separator();

    if (state.writingTrajectoryService) {
        auto trajectories = state.writingTrajectoryService->getAllTrajectories();
        
        for (const auto& traj : trajectories) {
            std::string label = traj.getIntent().purpose + " (" + StageToString(traj.getStage()) + ")";
            if (ImGui::Selectable(label.c_str(), state.activeTrajectoryId == traj.getId())) {
                state.activeTrajectoryId = traj.getId();
                state.showSegmentEditor = true;
            }
            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip("Audience: %s\nClaim: %s", 
                    traj.getIntent().audience.c_str(), 
                    traj.getIntent().coreClaim.c_str());
            }
        }
    } else {
        ImGui::TextColored(ImVec4(1,0,0,1), "Service not initialized");
    }

    // Modal for creation
    if (ImGui::BeginPopupModal("CreateTrajectoryPopup", NULL, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::InputText("Purpose", purposeBuf, IM_ARRAYSIZE(purposeBuf));
        ImGui::InputText("Audience", audienceBuf, IM_ARRAYSIZE(audienceBuf));
        ImGui::InputTextMultiline("Core Claim", claimBuf, IM_ARRAYSIZE(claimBuf));

        if (ImGui::Button("Create", ImVec2(120, 0))) {
            if (state.writingTrajectoryService) {
                std::string id = state.writingTrajectoryService->createTrajectory(
                    purposeBuf, audienceBuf, claimBuf, "");
                state.activeTrajectoryId = id;
                state.showSegmentEditor = true;
                
                // Clear buffers
                purposeBuf[0] = '\0';
                audienceBuf[0] = '\0';
                claimBuf[0] = '\0';
                
                ImGui::CloseCurrentPopup();
            }
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel", ImVec2(120, 0))) {
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }

    ImGui::End();
}

void DrawSegmentEditorPanel(AppState& state) {
    if (!state.showSegmentEditor) return;
    if (state.activeTrajectoryId.empty()) return;

    auto trajPtr = state.writingTrajectoryService->getTrajectory(state.activeTrajectoryId);
    if (!trajPtr) {
        ImGui::Text("Error loading trajectory.");
        return;
    }
    const auto& traj = *trajPtr;

    ImGui::SetNextWindowSize(ImVec2(800, 600), ImGuiCond_FirstUseEver);
    ImGui::Begin(("Editor: " + traj.getIntent().purpose).c_str(), &state.showSegmentEditor);

    ImGui::Text("Stage: %s", StageToString(traj.getStage()).c_str());
    ImGui::SameLine();
    if (ImGui::Button("Export (Markdown)")) {
         // Call export service (TODO)
         state.AppendLog("Export functionality not wired yet.\n");
    }

    ImGui::Separator();

    // Split Layout: Left (Outline/Segments), Right (Editor)
    static std::string selectedSegmentId = "";
    
    ImGui::Columns(2, "WritingColumns");
    
    // Left Column
    ImGui::Text("Segments");
    if (ImGui::Button("+ Add Segment")) {
        ImGui::OpenPopup("AddSegmentPopup");
    }

    const auto& segments = traj.getSegments();
    // Sort segments? Map is sorted by ID, but we might want custom order later.
    // For now iterate map.
    int i = 0;
    for (const auto& [id, seg] : segments) {
        if (ImGui::Selectable((seg.title + "##" + id).c_str(), selectedSegmentId == id)) {
            selectedSegmentId = id;
            // Load content into buffer
            std::snprintf(segmentContentBuf, IM_ARRAYSIZE(segmentContentBuf), "%s", seg.content.c_str());
        }
        i++;
    }

    ImGui::NextColumn();

    // Right Column
    if (!selectedSegmentId.empty()) {
        auto it = segments.find(selectedSegmentId);
        if (it != segments.end()) {
            const auto& seg = it->second;
            ImGui::Text("Editing: %s (v%d)", seg.title.c_str(), seg.version);
            ImGui::SameLine();
            ImGui::TextColored(ImVec4(0.5,0.5,0.5,1), "[%s]", SourceTagToString(seg.source).c_str());
            
            ImGui::InputTextMultiline("##editor", segmentContentBuf, IM_ARRAYSIZE(segmentContentBuf), 
                ImVec2(-FLT_MIN, -100)); // Leave space for rationale
            
            ImGui::Separator();
            ImGui::Text("Revision Rationale (Mandatory)");
            
            const char* ops[] = { "Clarify", "Compress", "Expand", "Reorganize", "Cite", "Remove", "Reframe", "Correction" };
            ImGui::Combo("Operation", &selectedOp, ops, IM_ARRAYSIZE(ops));
            ImGui::SameLine();
            ImGui::InputText("Reason", rationaleBuf, IM_ARRAYSIZE(rationaleBuf));

            if (ImGui::Button("Save Revision", ImVec2(120, 0))) {
                if (strlen(rationaleBuf) == 0) {
                    ImGui::OpenPopup("RationaleRequired");
                } else {
                    // Save
                    RevisionOperation op = static_cast<RevisionOperation>(selectedOp);
                    state.writingTrajectoryService->reviseSegment(
                        state.activeTrajectoryId, 
                        selectedSegmentId, 
                        segmentContentBuf, 
                        op, 
                        rationaleBuf
                    );
                    state.AppendLog("Revision saved for segment: " + seg.title + "\n");
                    // Clear rationale
                    rationaleBuf[0] = '\0';
                }
            }
        }
    } else {
        ImGui::Text("Select a segment to edit.");
    }
    
    // Popups
    if (ImGui::BeginPopupModal("RationaleRequired", NULL, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::Text("You must provide a rationale for this change.\nThis is crucial for the trajectory context.");
        if (ImGui::Button("OK", ImVec2(120, 0))) { 
            ImGui::CloseCurrentPopup(); 
        }
        ImGui::EndPopup();
    }
    
    if (ImGui::BeginPopupModal("AddSegmentPopup", NULL, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::InputText("Title", segmentTitleBuf, IM_ARRAYSIZE(segmentTitleBuf));
        if (ImGui::Button("Add", ImVec2(120, 0))) {
            state.writingTrajectoryService->addSegment(state.activeTrajectoryId, segmentTitleBuf, "");
            ImGui::CloseCurrentPopup();
            segmentTitleBuf[0] = '\0';
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel")) ImGui::CloseCurrentPopup();
        ImGui::EndPopup();
    }

    ImGui::End();
}

} // namespace ideawalker::ui
