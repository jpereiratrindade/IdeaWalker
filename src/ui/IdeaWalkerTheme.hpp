/**
 * @file IdeaWalkerTheme.hpp
 * @brief Unified visual theme for the IdeaWalker ImGui interface.
 *
 * Apply once at startup with IdeaWalkerTheme::Apply().
 * All color references should use constants from this file to ensure
 * a consistent, coherent design across all panels.
 *
 * Palette: Dark violet/indigo — inspired by deep-focus cognitive environments.
 */

#pragma once

#include "imgui.h"

namespace ideawalker::ui::Theme {

// ─── Color Palette ───────────────────────────────────────────────────────────

/// Base background — near-black with a subtle cool tint
constexpr ImVec4 BG_BASE          = {0.07f, 0.07f, 0.10f, 1.0f};  // #111119
/// Panel/child background — slightly lighter than base
constexpr ImVec4 BG_PANEL         = {0.11f, 0.11f, 0.16f, 1.0f};  // #1C1C29
/// Element background (inputs, frames)
constexpr ImVec4 BG_ELEMENT       = {0.15f, 0.15f, 0.21f, 1.0f};  // #262635
/// Hovered element background
constexpr ImVec4 BG_ELEMENT_HOVER = {0.20f, 0.20f, 0.28f, 1.0f};  // #333347
/// Active/pressed element background
constexpr ImVec4 BG_ELEMENT_ACTIVE= {0.24f, 0.22f, 0.34f, 1.0f};  // #3D3857

/// Primary accent — violet
constexpr ImVec4 ACCENT_PRIMARY   = {0.47f, 0.30f, 0.90f, 1.0f};  // #7850E5
/// Hovered accent
constexpr ImVec4 ACCENT_HOVER     = {0.58f, 0.42f, 1.00f, 1.0f};  // #946BFF
/// Active/pressed accent
constexpr ImVec4 ACCENT_ACTIVE    = {0.36f, 0.22f, 0.72f, 1.0f};  // #5C38B8
/// Title/header bar accent
constexpr ImVec4 ACCENT_TITLE     = {0.18f, 0.14f, 0.32f, 1.0f};  // #2E2452

/// Primary text — off-white for comfortable reading
constexpr ImVec4 TEXT_PRIMARY     = {0.92f, 0.92f, 0.95f, 1.0f};  // #EBEBF2
/// Secondary/muted text
constexpr ImVec4 TEXT_MUTED       = {0.55f, 0.55f, 0.65f, 1.0f};  // #8C8CA6
/// Disabled text
constexpr ImVec4 TEXT_DISABLED    = {0.35f, 0.35f, 0.42f, 1.0f};  // #59596B

/// Semantic — success
constexpr ImVec4 COLOR_SUCCESS    = {0.22f, 0.78f, 0.42f, 1.0f};  // #38C76B
/// Semantic — warning
constexpr ImVec4 COLOR_WARNING    = {0.95f, 0.72f, 0.10f, 1.0f};  // #F2B81A
/// Semantic — error
constexpr ImVec4 COLOR_ERROR      = {0.90f, 0.25f, 0.25f, 1.0f};  // #E64040
/// Semantic — info / in-progress
constexpr ImVec4 COLOR_INFO       = {0.30f, 0.65f, 0.95f, 1.0f};  // #4DA6F2

/// Separator / border color
constexpr ImVec4 COLOR_BORDER     = {0.22f, 0.22f, 0.30f, 1.0f};  // #38384D

// ─── Helpers ─────────────────────────────────────────────────────────────────

/// Converts an ImVec4 color to ImU32 (for use with ImDrawList)
inline ImU32 ToU32(const ImVec4& c) {
    return IM_COL32(
        static_cast<int>(c.x * 255),
        static_cast<int>(c.y * 255),
        static_cast<int>(c.z * 255),
        static_cast<int>(c.w * 255)
    );
}

// ─── Application ─────────────────────────────────────────────────────────────

/**
 * @brief Applies the IdeaWalker theme to the current ImGui context.
 *
 * Call once after ImGui::CreateContext() and before the render loop.
 * Sets colors, rounding, spacing and font size globals.
 */
inline void Apply() {
    ImGuiStyle& s = ImGui::GetStyle();

    // ── Geometry ──
    s.WindowRounding     = 8.0f;
    s.ChildRounding      = 6.0f;
    s.FrameRounding      = 6.0f;
    s.PopupRounding      = 6.0f;
    s.ScrollbarRounding  = 8.0f;
    s.GrabRounding       = 6.0f;
    s.TabRounding        = 6.0f;

    s.WindowBorderSize   = 1.0f;
    s.FrameBorderSize    = 0.0f;
    s.PopupBorderSize    = 1.0f;

    s.FramePadding       = ImVec2(10.0f, 6.0f);
    s.ItemSpacing        = ImVec2(8.0f, 6.0f);
    s.ItemInnerSpacing   = ImVec2(6.0f, 4.0f);
    s.WindowPadding      = ImVec2(12.0f, 12.0f);
    s.IndentSpacing      = 18.0f;
    s.ScrollbarSize      = 10.0f;
    s.GrabMinSize        = 8.0f;

    // ── Colors ──
    auto* c = s.Colors;

    c[ImGuiCol_WindowBg]             = BG_BASE;
    c[ImGuiCol_ChildBg]              = BG_PANEL;
    c[ImGuiCol_PopupBg]              = BG_PANEL;
    c[ImGuiCol_MenuBarBg]            = ACCENT_TITLE;

    c[ImGuiCol_Border]               = COLOR_BORDER;
    c[ImGuiCol_BorderShadow]         = {0.0f, 0.0f, 0.0f, 0.0f};

    c[ImGuiCol_FrameBg]              = BG_ELEMENT;
    c[ImGuiCol_FrameBgHovered]       = BG_ELEMENT_HOVER;
    c[ImGuiCol_FrameBgActive]        = BG_ELEMENT_ACTIVE;

    c[ImGuiCol_TitleBg]              = ACCENT_TITLE;
    c[ImGuiCol_TitleBgActive]        = ACCENT_ACTIVE;
    c[ImGuiCol_TitleBgCollapsed]     = {ACCENT_TITLE.x, ACCENT_TITLE.y, ACCENT_TITLE.z, 0.6f};

    c[ImGuiCol_ScrollbarBg]          = BG_PANEL;
    c[ImGuiCol_ScrollbarGrab]        = BG_ELEMENT_HOVER;
    c[ImGuiCol_ScrollbarGrabHovered] = ACCENT_PRIMARY;
    c[ImGuiCol_ScrollbarGrabActive]  = ACCENT_ACTIVE;

    c[ImGuiCol_CheckMark]            = ACCENT_PRIMARY;
    c[ImGuiCol_SliderGrab]           = ACCENT_PRIMARY;
    c[ImGuiCol_SliderGrabActive]     = ACCENT_ACTIVE;

    c[ImGuiCol_Button]               = BG_ELEMENT;
    c[ImGuiCol_ButtonHovered]        = ACCENT_HOVER;
    c[ImGuiCol_ButtonActive]         = ACCENT_ACTIVE;

    c[ImGuiCol_Header]               = {ACCENT_PRIMARY.x, ACCENT_PRIMARY.y, ACCENT_PRIMARY.z, 0.30f};
    c[ImGuiCol_HeaderHovered]        = {ACCENT_PRIMARY.x, ACCENT_PRIMARY.y, ACCENT_PRIMARY.z, 0.50f};
    c[ImGuiCol_HeaderActive]         = ACCENT_PRIMARY;

    c[ImGuiCol_Separator]            = COLOR_BORDER;
    c[ImGuiCol_SeparatorHovered]     = ACCENT_HOVER;
    c[ImGuiCol_SeparatorActive]      = ACCENT_PRIMARY;

    c[ImGuiCol_ResizeGrip]           = {ACCENT_PRIMARY.x, ACCENT_PRIMARY.y, ACCENT_PRIMARY.z, 0.20f};
    c[ImGuiCol_ResizeGripHovered]    = {ACCENT_PRIMARY.x, ACCENT_PRIMARY.y, ACCENT_PRIMARY.z, 0.60f};
    c[ImGuiCol_ResizeGripActive]     = ACCENT_PRIMARY;

    c[ImGuiCol_Tab]                  = BG_ELEMENT;
    c[ImGuiCol_TabHovered]           = ACCENT_HOVER;
    c[ImGuiCol_TabActive]            = ACCENT_PRIMARY;
    c[ImGuiCol_TabUnfocused]         = BG_PANEL;
    c[ImGuiCol_TabUnfocusedActive]   = BG_ELEMENT_HOVER;

    c[ImGuiCol_PlotLines]            = ACCENT_PRIMARY;
    c[ImGuiCol_PlotLinesHovered]     = ACCENT_HOVER;
    c[ImGuiCol_PlotHistogram]        = ACCENT_PRIMARY;
    c[ImGuiCol_PlotHistogramHovered] = ACCENT_HOVER;

    c[ImGuiCol_TableHeaderBg]        = ACCENT_TITLE;
    c[ImGuiCol_TableBorderStrong]    = COLOR_BORDER;
    c[ImGuiCol_TableBorderLight]     = {COLOR_BORDER.x, COLOR_BORDER.y, COLOR_BORDER.z, 0.4f};
    c[ImGuiCol_TableRowBg]           = {0.0f, 0.0f, 0.0f, 0.0f};
    c[ImGuiCol_TableRowBgAlt]        = {1.0f, 1.0f, 1.0f, 0.03f};

    c[ImGuiCol_TextSelectedBg]       = {ACCENT_PRIMARY.x, ACCENT_PRIMARY.y, ACCENT_PRIMARY.z, 0.35f};
    c[ImGuiCol_DragDropTarget]       = COLOR_WARNING;
    c[ImGuiCol_NavHighlight]         = ACCENT_PRIMARY;

    c[ImGuiCol_Text]                 = TEXT_PRIMARY;
    c[ImGuiCol_TextDisabled]         = TEXT_DISABLED;
}

} // namespace ideawalker::ui::Theme
