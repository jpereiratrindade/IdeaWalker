#include "ui/panels/MainPanels.hpp"
#include "ui/UiUtils.hpp"
#include "ui/UiMarkdownRenderer.hpp"
#include "imgui.h"
#include <vector>

namespace ideawalker::ui {

void DrawExternalFilesTab(AppState& app) {
    auto label = [&app](const char* withEmoji, const char* plain) {
        return app.ui.emojiEnabled ? withEmoji : plain;
    };

    ImGuiTabItemFlags flagsExt = (app.ui.requestedTab == 4) ? ImGuiTabItemFlags_SetSelected : 0;
    if (ImGui::BeginTabItem(label("📂 External Files", "External Files"), NULL, flagsExt)) {
        if (app.ui.requestedTab == 4) app.ui.requestedTab = -1;
        app.ui.activeTab = 4;

        if (app.external.files.empty()) {
            ImGui::TextDisabled("No external files open.");
            ImGui::TextDisabled("Use File > Open File... to open .txt or .md files.");
        } else {
            // File Tabs
            if (ImGui::BeginTabBar("ExternalFilesTabs", ImGuiTabBarFlags_AutoSelectNewTabs)) {
                for (int i = 0; i < (int)app.external.files.size(); ++i) {
                    bool open = true;
                    if (ImGui::BeginTabItem(app.external.files[i].filename.c_str(), &open)) {
                        app.external.selectedIndex = i;
                        
                        if (ImGui::Button(label("💾 Save", "Save"))) {
                            app.SaveExternalFile(i);
                        }
                        ImGui::SameLine();
                        if (ImGui::Button(label("❌ Close", "Close"))) {
                            open = false;
                        }
                        ImGui::SameLine();
                        if (ImGui::Checkbox(label("👁️ Preview", "Preview"), &app.ui.previewMode)) {
                            // toggle
                        }

                        ImGui::Separator();
                        
                        ExternalFile& file = app.external.files[i];

                        if (app.ui.previewMode) {
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
                        app.external.files.erase(app.external.files.begin() + i);
                        if (app.external.selectedIndex >= i) app.external.selectedIndex--;
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
