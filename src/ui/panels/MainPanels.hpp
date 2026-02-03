#pragma once

#include "ui/AppState.hpp"

namespace ideawalker::ui {

// Tabs
void DrawDashboardTab(AppState& app);
void DrawKnowledgeTab(AppState& app);
void DrawExecutionTab(AppState& app);
void DrawGraphTab(AppState& app);
void DrawExternalFilesTab(AppState& app);

// Components
void DrawNodeGraph(AppState& app);
void DrawMainTabs(AppState& app);

// Modals
void DrawHistoryModal(AppState& app);
void DrawProjectModals(AppState& app);
void DrawTaskDetailsModal(AppState& app);
void DrawHelpModal(AppState& app);
void DrawUpdateModal(AppState& app);
void DrawAllModals(AppState& app);

// Main Blocks
void DrawMenuBar(AppState& app);
void DrawWorkspace(AppState& app);
void DrawMainWindow(AppState& app);

} // namespace ideawalker::ui
