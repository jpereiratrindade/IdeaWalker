/**
 * @file WritingPanels.hpp
 * @brief UI Panels for the Writing Trajectory Context.
 */

#pragma once

#include "application/AppController.hpp"

namespace ideawalker::ui {

/**
 * @brief Renders the list of writing trajectories.
 */
void DrawTrajectoryPanel(application::AppController& state);

/**
 * @brief Renders the editor for the active writing trajectory.
 */
void DrawSegmentEditorPanel(application::AppController& state);
void DrawDefensePanel(application::AppController& state);

} // namespace ideawalker::ui
