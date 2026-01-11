/**
 * @file WritingPanels.hpp
 * @brief UI Panels for the Writing Trajectory Context.
 */

#pragma once

#include "ui/AppState.hpp"

namespace ideawalker::ui {

/**
 * @brief Renders the list of writing trajectories.
 */
void DrawTrajectoryPanel(AppState& state);

/**
 * @brief Renders the editor for the active writing trajectory.
 */
void DrawSegmentEditorPanel(AppState& state);

} // namespace ideawalker::ui
