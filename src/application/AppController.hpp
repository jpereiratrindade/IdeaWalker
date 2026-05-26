/**
 * @file AppController.hpp
 * @brief Application Controller linking UI ViewModel to Application Services.
 */

#pragma once

#include <string>
#include <functional>
#include <unordered_set>
#include "application/AppServices.hpp"
#include "ui/ViewModel.hpp"

namespace ideawalker::application {

class AppController {
public:
    AppController();
    ~AppController();

    // The factory to build services when project opens
    std::function<application::AppServices(const std::string&)> servicesFactory;

    // Initialization and shutdown
    void InitImNodes();
    void ShutdownImNodes();

    // Access to ViewModel
    ui::ViewModel& GetViewModel() { return m_viewModel; }
    const ui::ViewModel& GetViewModel() const { return m_viewModel; }
    
    // Quick access to underlying services if panels need it directly
    application::AppServices& GetServices() { return m_services; }
    const application::AppServices& GetServices() const { return m_services; }

    // Project Lifecycle
    bool NewProject(const std::string& rootPath);
    bool OpenProject(const std::string& rootPath);
    bool SaveProject();
    bool SaveProjectAs(const std::string& rootPath);
    bool CloseProject();

    // Data Actions
    void RefreshInbox();
    void RefreshDialogueList();
    void RefreshAllInsights();
    void AnalyzeSuggestions();

    // Graph
    void RebuildGraph();
    void UpdateGraphPhysics(const std::unordered_set<int>& selectedNodes = {});
    void CenterGraph();

    // Files and Exports
    void HandleFileDrop(const std::string& filePath);
    void RequestTranscription(const std::string& filePath);
    std::string ExportToMermaid() const;
    std::string ExportFullMarkdown() const;
    void OpenExternalFile(const std::string& path);
    void SaveExternalFile(int index);

    // Config & History & Updates
    void LoadConfig();
    void SaveConfig();
    void SetAIModel(const std::string& modelName);
    void LoadHistory(const std::string& noteId);
    void CheckForUpdates();

    // Services injection
    void InjectServices(application::AppServices&& newServices);

private:
    ui::ViewModel m_viewModel;
    application::AppServices m_services;
};

} // namespace ideawalker::application
