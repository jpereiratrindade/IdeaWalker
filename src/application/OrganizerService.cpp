#include "application/OrganizerService.hpp"
#include <iostream>

namespace ideawalker::application {

OrganizerService::OrganizerService(std::unique_ptr<domain::ThoughtRepository> repo, 
                                   std::unique_ptr<domain::AIService> ai)
    : m_repo(std::move(repo)), m_ai(std::move(ai)) {}

void OrganizerService::processInbox() {
    auto rawThoughts = m_repo->fetchInbox();
    for (const auto& thought : rawThoughts) {
        auto insight = m_ai->processRawThought(thought.content);
        if (insight) {
            m_repo->saveInsight(*insight);
        }
    }
}

std::vector<domain::RawThought> OrganizerService::getRawThoughts() {
    return m_repo->fetchInbox();
}

} // namespace ideawalker::application
