/**
 * @file UiRenderer.hpp
 * @brief Main entry point for the Dear ImGui UI rendering.
 */

#pragma once

#include "application/AppController.hpp"

namespace ideawalker::ui {

/**
 * @brief Main UI rendering entry point. Call once per frame.
 * @param app The shared application state.
 */
void DrawUI(application::AppController& app);

} // namespace ideawalker::ui
