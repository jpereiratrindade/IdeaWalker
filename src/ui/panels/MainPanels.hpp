#pragma once

#include "application/AppController.hpp"

namespace ideawalker::ui {

// Tabs
void DrawDashboardTab(application::AppController& app);
void DrawKnowledgeTab(application::AppController& app);
void DrawExecutionTab(application::AppController& app);
void DrawGraphTab(application::AppController& app);
void DrawExternalFilesTab(application::AppController& app);
void DrawScientificTab(application::AppController& app);
void DrawDocOpsTab(application::AppController& app);

// Components
void DrawNodeGraph(application::AppController& app);
void DrawMainTabs(application::AppController& app);

// Modals
void DrawHistoryModal(application::AppController& app);
void DrawProjectModals(application::AppController& app);
void DrawTaskDetailsModal(application::AppController& app);
void DrawHelpModal(application::AppController& app);
void DrawUpdateModal(application::AppController& app);
void DrawAllModals(application::AppController& app);

// Main Blocks
void DrawMenuBar(application::AppController& app);
void DrawWorkspace(application::AppController& app);
void DrawMainWindow(application::AppController& app);
void DrawStatusBar(application::AppController& app);

} // namespace ideawalker::ui
