/**
 * @file UiRenderer.cpp
 * @brief Implementation of the UI rendering system using modular panels.
 */
#include "ui/UiRenderer.hpp"
#include "ui/panels/MainPanels.hpp"
#include "ui/panels/WritingPanels.hpp"

namespace ideawalker::ui {

void DrawUI(AppState& app) {
    // Check if we need to refresh data (e.g., after AI finishing in background)
    if (app.ui.pendingRefresh.exchange(false)) {
        app.RefreshInbox();
        app.RefreshAllInsights();
    }

    // 1. Draw the Main Window (which includes Menu Bar and Workspace/Tabs)
    DrawMainWindow(app);

    // 2. Draw all Modals (Task Details, History, Help, Update, Project Modals)
    DrawAllModals(app);
    
    // 3. Draw Writing Trajectory Panels (Overlay/Separate Windows)
    DrawTrajectoryPanel(app);
    DrawSegmentEditorPanel(app);
    DrawDefensePanel(app);
}

} // namespace ideawalker::ui
