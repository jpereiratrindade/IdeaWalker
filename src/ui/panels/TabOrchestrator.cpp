#include "ui/panels/MainPanels.hpp"
#include "ui/ConversationPanel.hpp"
#include "imgui.h"

namespace ideawalker::ui {

void DrawMainTabs(AppState& app) {
    if (ImGui::BeginTabBar("MyTabs")) {
        DrawDashboardTab(app);
        DrawKnowledgeTab(app);
        DrawExecutionTab(app);
        DrawGraphTab(app);
        DrawExternalFilesTab(app);
        ImGui::EndTabBar();
    }
}

void DrawWorkspace(AppState& app) {
    static float chatHeight = 250.0f;
    const float minChatHeight = 80.0f;
    const float maxChatHeight = 800.0f;

    float availableHeight = ImGui::GetContentRegionAvail().y;
    float workspaceHeight = availableHeight;

    if (app.ui.showConversation) {
        workspaceHeight = availableHeight - chatHeight - 4.0f; // 4px for splitter
        if (workspaceHeight < 150.0f) workspaceHeight = 150.0f; // Minimal workspace
    }

    // Main Workspace Area (Top)
    ImGui::BeginChild("MainWorkspace", ImVec2(0, workspaceHeight), false, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
    DrawMainTabs(app);
    ImGui::EndChild();

    // Splitter & Chat (Bottom)
    if (app.ui.showConversation) {
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

void DrawMainWindow(AppState& app) {
    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(viewport->WorkPos);
    ImGui::SetNextWindowSize(viewport->WorkSize);
    
    // Begin Main Window
    ImGui::Begin("Main", NULL, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_MenuBar);
    
    DrawMenuBar(app);
    DrawWorkspace(app);
    
    ImGui::End();
}

} // namespace ideawalker::ui
