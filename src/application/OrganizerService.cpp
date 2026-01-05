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

void OrganizerService::updateNote(const std::string& filename, const std::string& content) {
    m_repo->updateNote(filename, content);
}

void OrganizerService::toggleTask(const std::string& filename, int index) {
    // ...
}

std::vector<std::string> OrganizerService::getBacklinks(const std::string& filename) {
    return m_repo->getBacklinks(filename);
}

std::map<std::string, int> OrganizerService::getActivityHistory() {
    return m_repo->getActivityHistory();
}

std::vector<domain::RawThought> OrganizerService::getRawThoughts() {
    return m_repo->fetchInbox();
}

} // namespace ideawalker::application
