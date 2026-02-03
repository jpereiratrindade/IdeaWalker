#include "ui/panels/MainPanels.hpp"
#include "ui/UiUtils.hpp"
#include "ui/UiMarkdownRenderer.hpp"
#include "imgui.h"
#include <vector>

namespace ideawalker::ui {

void DrawExternalFilesTab(AppState& app) {
    auto label = [&app](const char* withEmoji, const char* plain) {
        return app.emojiEnabled ? withEmoji : plain;
    };

    ImGuiTabItemFlags flagsExt = (app.requestedTab == 4) ? ImGuiTabItemFlags_SetSelected : 0;
    if (ImGui::BeginTabItem(label("📂 External Files", "External Files"), NULL, flagsExt)) {
        if (app.requestedTab == 4) app.requestedTab = -1;
        app.activeTab = 4;

        if (app.externalFiles.empty()) {
            ImGui::TextDisabled("No external files open.");
            ImGui::TextDisabled("Use File > Open File... to open .txt or .md files.");
        } else {
            // File Tabs
            if (ImGui::BeginTabBar("ExternalFilesTabs", ImGuiTabBarFlags_AutoSelectNewTabs)) {
                for (int i = 0; i < (int)app.externalFiles.size(); ++i) {
                    bool open = true;
                    if (ImGui::BeginTabItem(app.externalFiles[i].filename.c_str(), &open)) {
                        app.selectedExternalFileIndex = i;
                        
                        if (ImGui::Button(label("💾 Save", "Save"))) {
                            app.SaveExternalFile(i);
                        }
                        ImGui::SameLine();
                        if (ImGui::Button(label("❌ Close", "Close"))) {
                            open = false;
                        }
                        ImGui::SameLine();
                        if (ImGui::Checkbox(label("👁️ Preview", "Preview"), &app.previewMode)) {
                            // toggle
                        }

                        ImGui::Separator();
                        
                        ExternalFile& file = app.externalFiles[i];

                        if (app.previewMode) {
                            ImGui::BeginChild("ExtPreview", ImVec2(0, -10), true);
                            DrawMarkdownPreview(app, file.content, true);
                            ImGui::EndChild();
                        } else {
                            if (InputTextMultilineString("##exteditor", &file.content, ImVec2(-FLT_MIN, -10))) {
                                file.modified = true;
                            }
                        }
                        
                        ImGui::EndTabItem();
                    }
                    if (!open) {
                        app.externalFiles.erase(app.externalFiles.begin() + i);
                        if (app.selectedExternalFileIndex >= i) app.selectedExternalFileIndex--;
                        i--; // adjust index
                    }
                }
                ImGui::EndTabBar();
            }
        }
        ImGui::EndTabItem();
    }
}

} // namespace ideawalker::ui
