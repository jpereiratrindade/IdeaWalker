#include "ui/panels/MainPanels.hpp"
#include "application/OrganizerService.hpp"
#include "imgui.h"
#include "imnodes.h"
#include <unordered_set>

namespace ideawalker::ui {

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
    for (auto& node : app.graphNodes) {
        if (ImNodes::IsNodeSelected(node.id)) {
            ImVec2 pos = ImNodes::GetNodeGridSpacePos(node.id);
            node.x = pos.x;
            node.y = pos.y;
            node.vx = 0;
            node.vy = 0;
        }
    }
}

void DrawGraphTab(AppState& app) {
    auto label = [&app](const char* withEmoji, const char* plain) {
        return app.emojiEnabled ? withEmoji : plain;
    };
    const bool hasProject = (app.services.organizerService != nullptr);

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

} // namespace ideawalker::ui
