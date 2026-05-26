/**
 * @file StatusBarPanel.cpp
 * @brief Global status bar rendered at the bottom of the main window.
 *
 * Displays at a glance: active project name, current AI model,
 * running tasks with animated spinner, and the application version.
 *
 * Usage — call once per frame inside the main window, before ImGui::End():
 * @code
 *   DrawStatusBar(app);
 * @endcode
 */

#include "ui/panels/MainPanels.hpp"
#include "ui/IdeaWalkerTheme.hpp"
#include "imgui.h"
#include <cmath>
#include <string>
#include <filesystem>

namespace ideawalker::ui {

namespace {

/// Returns a simple ASCII spinner frame based on wall-clock time.
static const char* SpinnerFrame() {
    static const char* frames[] = { "|", "/", "-", "\\" };
    const double t = ImGui::GetTime();
    const int idx = static_cast<int>(t * 6.0) % 4;
    return frames[idx];
}

/// Truncates a string to maxLen chars, appending "…" if needed.
static std::string Truncate(const std::string& s, size_t maxLen) {
    if (s.size() <= maxLen) return s;
    return s.substr(0, maxLen - 1) + "\xe2\x80\xa6"; // UTF-8 ellipsis
}

} // namespace

/**
 * @brief Draws the global status bar at the very bottom of the calling ImGui window.
 *
 * Must be called while a parent window is open (between Begin/End).
 * The bar overlays the bottom edge of the window with a fixed-height child region.
 */
void DrawStatusBar(application::AppController& app) {
    constexpr float BAR_HEIGHT = 24.0f;

    // Reserve space at the bottom of the parent window for the status bar
    const float available = ImGui::GetContentRegionAvail().y;
    ImGui::SetCursorPosY(ImGui::GetCursorPosY() + available - BAR_HEIGHT);

    // Background
    ImGui::PushStyleColor(ImGuiCol_ChildBg, Theme::ACCENT_TITLE);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(8.0f, 3.0f));
    ImGui::BeginChild("##StatusBar", ImVec2(-1.0f, BAR_HEIGHT), false,
                      ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);

    // ── Left section: project name ──────────────────────────────────────────
    const bool hasProject = !app.GetViewModel().project.root.empty();
    std::string projectLabel;
    if (hasProject) {
        const std::string name = std::filesystem::path(app.GetViewModel().project.root).filename().string();
        projectLabel = "\xf0\x9f\x93\x81 " + Truncate(name, 28); // 📁
    } else {
        projectLabel = "\xe2\x96\xaa Sem projeto";
    }

    ImGui::PushStyleColor(ImGuiCol_Text, Theme::TEXT_PRIMARY);
    ImGui::Text("%s", projectLabel.c_str());

    // ── Separator ───────────────────────────────────────────────────────────
    ImGui::SameLine();
    ImGui::PushStyleColor(ImGuiCol_Text, Theme::TEXT_MUTED);
    ImGui::Text("  |  ");
    ImGui::PopStyleColor();

    // ── Center: AI model ────────────────────────────────────────────────────
    std::string modelLabel;
    if (hasProject && app.GetServices().aiProcessingService && app.GetServices().aiProcessingService->GetAI()) {
        const std::string model = app.GetServices().aiProcessingService->GetAI()->getCurrentModel();
        modelLabel = "\xf0\x9f\xa4\x96 " + Truncate(model, 24); // 🤖
    } else {
        modelLabel = "\xf0\x9f\xa4\x96 \xe2\x80\x94"; // 🤖 —
    }

    ImGui::SameLine();
    ImGui::PushStyleColor(ImGuiCol_Text, Theme::TEXT_MUTED);
    ImGui::Text("%s", modelLabel.c_str());
    ImGui::PopStyleColor();

    // ── Task spinner ─────────────────────────────────────────────────────────
    if (app.GetServices().taskManager) {
        const auto activeTasks = app.GetServices().taskManager->GetActiveTasks();
        if (!activeTasks.empty()) {
            ImGui::SameLine();
            ImGui::PushStyleColor(ImGuiCol_Text, Theme::COLOR_WARNING);
            const auto& task = activeTasks[0];
            const int percent = static_cast<int>(task->progress.load() * 100.0f);
            ImGui::Text("  %s %s %d%%", SpinnerFrame(),
                        Truncate(task->description, 20).c_str(), percent);
            ImGui::PopStyleColor();

            // Extra count if more than one task
            if (activeTasks.size() > 1) {
                ImGui::SameLine();
                ImGui::PushStyleColor(ImGuiCol_Text, Theme::TEXT_MUTED);
                ImGui::Text("+%zu", activeTasks.size() - 1);
                ImGui::PopStyleColor();
            }
        }
    }

    // ── Right section: version (right-aligned) ───────────────────────────────
    const char* version = IDEAWALKER_VERSION;
    const float versionWidth = ImGui::CalcTextSize(version).x + 12.0f;
    const float rightX = ImGui::GetWindowWidth() - versionWidth;
    if (rightX > ImGui::GetCursorPosX() + 40.0f) {
        ImGui::SameLine(rightX);
        ImGui::PushStyleColor(ImGuiCol_Text, Theme::TEXT_MUTED);
        ImGui::Text("%s", version);
        ImGui::PopStyleColor();
    }

    ImGui::PopStyleColor(); // TEXT_PRIMARY
    ImGui::EndChild();
    ImGui::PopStyleVar();
    ImGui::PopStyleColor(); // ChildBg
}

} // namespace ideawalker::ui
