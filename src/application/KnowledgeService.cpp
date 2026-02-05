/**
 * @file KnowledgeService.cpp
 * @brief Implementation of the KnowledgeService class.
 */

#include "application/KnowledgeService.hpp"
#include <algorithm>

namespace ideawalker::application {

KnowledgeService::KnowledgeService(std::unique_ptr<domain::ThoughtRepository> repo)
    : m_repo(std::move(repo)) {}

void KnowledgeService::UpdateNote(const std::string& filename, const std::string& content) {
    m_repo->updateNote(filename, content);
}

void KnowledgeService::ToggleTask(const std::string& filename, int index) {
    auto history = m_repo->fetchHistory();
    for (auto& insight : history) {
        if (insight.getMetadata().id == filename) {
            insight.parseActionablesFromContent();
            insight.toggleActionable(index);
            m_repo->updateNote(filename, insight.getContent());
            break;
        }
    }
}

void KnowledgeService::SetTaskStatus(const std::string& filename, int index, bool completed, bool inProgress) {
    auto history = m_repo->fetchHistory();
    for (auto& insight : history) {
        if (insight.getMetadata().id == filename) {
            insight.parseActionablesFromContent();
            insight.setActionableStatus(index, completed, inProgress);
            m_repo->updateNote(filename, insight.getContent());
            break;
        }
    }
}

std::vector<domain::Insight> KnowledgeService::GetAllInsights() {
    auto insights = m_repo->fetchHistory();
    for (auto& insight : insights) {
        insight.parseActionablesFromContent();
    }
    return insights;
}

std::vector<domain::RawThought> KnowledgeService::GetRawThoughts() {
    return m_repo->fetchInbox();
}

std::map<std::string, int> KnowledgeService::GetActivityHistory() {
    return m_repo->getActivityHistory();
}

std::vector<std::string> KnowledgeService::GetBacklinks(const std::string& filename) {
    return m_repo->getBacklinks(filename);
}

std::vector<std::string> KnowledgeService::GetNoteHistory(const std::string& noteId) {
    return m_repo->getVersions(noteId);
}

std::string KnowledgeService::GetVersionContent(const std::string& versionFilename) {
    return m_repo->getVersionContent(versionFilename);
}

std::string KnowledgeService::GetNoteContent(const std::string& filename) {
    return m_repo->getNoteContent(filename);
}

std::optional<std::string> KnowledgeService::GetObservationContent(const std::string& filename) {
    return m_repo->findObservationContent(filename);
}

} // namespace ideawalker::application
