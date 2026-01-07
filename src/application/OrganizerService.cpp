/**
 * @file OrganizerService.cpp
 * @brief Implementation of the OrganizerService class.
 */
#include "application/OrganizerService.hpp"
#include <cctype>
#include <sstream>

namespace ideawalker::application {

namespace {

std::string NormalizeToId(const std::string& filename) {
    std::string base = filename;
    size_t lastDot = base.find_last_of('.');
    if (lastDot != std::string::npos) {
        base = base.substr(0, lastDot);
    }

    std::string id;
    id.reserve(base.size());
    for (char ch : base) {
        unsigned char c = static_cast<unsigned char>(ch);
        if (std::isalnum(c) || ch == '-' || ch == '_') {
            id.push_back(ch);
        } else {
            id.push_back('_');
        }
    }

    if (id.empty()) {
        id = "note";
    }

    return id;
}

std::string FilterTaskLines(const std::string& text) {
    std::stringstream ss(text);
    std::ostringstream out;
    std::string line;
    while (std::getline(ss, line)) {
        if (line.rfind("- [", 0) == 0) {
            out << line << "\n";
        }
    }
    return out.str();
}

} // namespace

OrganizerService::OrganizerService(std::unique_ptr<domain::ThoughtRepository> repo, 
                                   std::unique_ptr<domain::AIService> ai,
                                   std::unique_ptr<domain::TranscriptionService> transcriber)
    : m_repo(std::move(repo)), m_ai(std::move(ai)), m_transcriber(std::move(transcriber)) {}

void OrganizerService::processInbox(bool force) {
    auto rawThoughts = m_repo->fetchInbox();
    for (const auto& thought : rawThoughts) {
        std::string insightId = NormalizeToId(thought.filename);
        if (!force && !m_repo->shouldProcess(thought, insightId)) {
            continue;
        }
        auto insight = m_ai->processRawThought(thought.content);
        if (insight) {
            auto meta = insight->getMetadata();
            meta.id = insightId;
            domain::Insight normalized(meta, insight->getContent());
            m_repo->saveInsight(normalized);
        }
    }
}

OrganizerService::ProcessResult OrganizerService::processInboxItem(const std::string& filename, bool force) {
    auto rawThoughts = m_repo->fetchInbox();
    for (const auto& thought : rawThoughts) {
        if (thought.filename != filename) {
            continue;
        }

        std::string insightId = NormalizeToId(thought.filename);
        if (!force && !m_repo->shouldProcess(thought, insightId)) {
            return ProcessResult::SkippedUpToDate;
        }

        auto insight = m_ai->processRawThought(thought.content);
        if (!insight) {
            return ProcessResult::Failed;
        }

        auto meta = insight->getMetadata();
        meta.id = insightId;
        domain::Insight normalized(meta, insight->getContent());
        m_repo->saveInsight(normalized);
        return ProcessResult::Processed;
    }

    return ProcessResult::NotFound;
}

bool OrganizerService::updateConsolidatedTasks() {
    auto insights = m_repo->fetchHistory();
    std::ostringstream taskList;
    bool hasTasks = false;

    for (auto& insight : insights) {
        if (insight.getMetadata().id == kConsolidatedTasksFilename) {
            continue;
        }
        insight.parseActionablesFromContent();
        for (const auto& task : insight.getActionables()) {
            char status = ' ';
            if (task.isCompleted) status = 'x';
            else if (task.isInProgress) status = '/';
            taskList << "- [" << status << "] " << task.description << " (origem: " << insight.getMetadata().id << ")\n";
            hasTasks = true;
        }
    }

    if (!hasTasks) {
        m_repo->updateNote(kConsolidatedTasksFilename, "# Tarefas Consolidadas\n\n");
        return true;
    }

    auto consolidated = m_ai->consolidateTasks(taskList.str());
    if (!consolidated) {
        return false;
    }

    std::string filtered = FilterTaskLines(*consolidated);
    if (filtered.empty()) {
        return false;
    }

    std::string content = "# Tarefas Consolidadas\n\n" + filtered;
    m_repo->updateNote(kConsolidatedTasksFilename, content);
    return true;
}

void OrganizerService::updateNote(const std::string& filename, const std::string& content) {
    m_repo->updateNote(filename, content);
}

void OrganizerService::toggleTask(const std::string& filename, int index) {
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

void OrganizerService::setTaskStatus(const std::string& filename, int index, bool completed, bool inProgress) {
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

std::vector<domain::Insight> OrganizerService::getAllInsights() {
    auto insights = m_repo->fetchHistory();
    for (auto& insight : insights) {
        insight.parseActionablesFromContent();
    }
    return insights;
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

void OrganizerService::transcribeAudio(const std::string& audioPath, 
                                      std::function<void(std::string)> onSuccess, 
                                      std::function<void(std::string)> onError) {
    if (m_transcriber) {
        m_transcriber->transcribeAsync(audioPath, onSuccess, onError);
    } else {
        if (onError) onError("Transcription service not initialized.");
    }
}



void OrganizerService::setAIPersona(domain::AIPersona persona) {
    if (m_ai) {
        m_ai->setPersona(persona);
    }
}

} // namespace ideawalker::application
