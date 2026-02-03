/**
 * @file AppServices.hpp
 * @brief Container for application-level services to facilitate dependency injection.
 */

#pragma once

#include <memory>
#include "application/OrganizerService.hpp"
#include "application/ConversationService.hpp"
#include "application/ContextAssembler.hpp"
#include "application/DocumentIngestionService.hpp"
#include "application/SuggestionService.hpp"
#include "application/writing/WritingTrajectoryService.hpp"
#include "application/GraphService.hpp"
#include "application/ProjectService.hpp"
#include "application/KnowledgeExportService.hpp"
#include "infrastructure/PersistenceService.hpp"

namespace ideawalker::application {

struct AppServices {
    std::unique_ptr<OrganizerService> organizerService;
    std::unique_ptr<ConversationService> conversationService;
    std::unique_ptr<ContextAssembler> contextAssembler;
    std::unique_ptr<DocumentIngestionService> ingestionService;
    std::unique_ptr<SuggestionService> suggestionService;
    std::unique_ptr<writing::WritingTrajectoryService> writingTrajectoryService;
    std::unique_ptr<GraphService> graphService;
    std::unique_ptr<ProjectService> projectService;
    std::unique_ptr<KnowledgeExportService> exportService;
    std::shared_ptr<infrastructure::PersistenceService> persistenceService;
};

} // namespace ideawalker::application
