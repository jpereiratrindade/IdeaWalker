/**
 * @file UiRenderer.hpp
 * @brief Main entry point for the Dear ImGui UI rendering.
 */

#pragma once

#include "ui/AppState.hpp"

namespace ideawalker::ui {

/**
 * @brief Main UI rendering entry point. Call once per frame.
 * @param app The shared application state.
 */
void DrawUI(AppState& app);

} // namespace ideawalker::ui
