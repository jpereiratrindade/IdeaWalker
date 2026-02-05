/**
 * @file AIProcessingService.cpp
 * @brief Implementation of the AIProcessingService class.
 */

#include "application/AIProcessingService.hpp"
#include "application/scientific/ScientificIngestionService.hpp"
#include <sstream>
#include <iostream>

namespace ideawalker::application {

AIProcessingService::AIProcessingService(KnowledgeService& knowledge,
                                         std::shared_ptr<domain::AIService> ai,
                                         std::shared_ptr<AsyncTaskManager> taskManager,
                                         std::unique_ptr<domain::TranscriptionService> transcriber,
                                         std::shared_ptr<scientific::ScientificIngestionService> scientificService)
    : m_knowledge(knowledge), m_ai(std::move(ai)), m_taskManager(std::move(taskManager)), m_transcriber(std::move(transcriber)), m_scientificService(std::move(scientificService)) {}

std::string AIProcessingService::NormalizeToId(const std::string& filename) {
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
    return id.empty() ? "note" : id;
}

std::string AIProcessingService::FilterTaskLines(const std::string& text) {
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

void AIProcessingService::ProcessInboxAsync(bool force, bool fastMode) {
    m_taskManager->SubmitTask(TaskType::AI_Processing, "Processando Inbox", [this, force, fastMode](std::shared_ptr<TaskStatus> status) {
        auto rawThoughts = m_knowledge.GetRawThoughts();
        size_t total = rawThoughts.size();
        for (size_t i = 0; i < total; ++i) {
            const auto& thought = rawThoughts[i];
            std::string insightId = NormalizeToId(thought.filename);
            
            if (!force && !m_knowledge.GetRepository().shouldProcess(thought, insightId)) {
                status->progress = static_cast<float>(i + 1) / total;
                continue;
            }

            auto insight = m_ai->processRawThought(thought.content, fastMode, [status](const std::string& msg) {
                // Update status if needed
            });

            if (insight) {
                auto meta = insight->getMetadata();
                
                // INTENT ROUTING
                bool isScientific = false;
                for (const auto& tag : meta.tags) {
                    if (tag == "#ScientificObserver") isScientific = true;
                }

                if (isScientific && m_scientificService) {
                     // Intent: Scientific Ingestion (Candidate Bundle)
                     m_scientificService->ingestScientificBundle(insight->getContent(), insightId);
                } else {
                    // Intent: Standard Note Persistence
                    meta.id = insightId;
                    domain::Insight normalized(meta, insight->getContent());
                    m_knowledge.GetRepository().saveInsight(normalized);
                }
            }
            status->progress = static_cast<float>(i + 1) / total;
        }
        // Auto-consolidate after batch
        ConsolidateTasksAsync();
    });
}

void AIProcessingService::ProcessItemAsync(const std::string& filename, bool force, bool fastMode) {
    m_taskManager->SubmitTask(TaskType::AI_Processing, "Processando: " + filename, [this, filename, force, fastMode](std::shared_ptr<TaskStatus> status) {
        auto rawThoughts = m_knowledge.GetRawThoughts();
        for (const auto& thought : rawThoughts) {
            if (thought.filename == filename) {
                std::string insightId = NormalizeToId(thought.filename);
                if (!force && !m_knowledge.GetRepository().shouldProcess(thought, insightId)) {
                    return;
                }

                // Injection: Check for existing Narrative Observation
                std::cout << "[AI] Checking context for: " << thought.filename << std::endl;
                auto observation = m_knowledge.GetObservationContent(thought.filename);
                std::string processedContent = thought.content;
                
                if (observation && !observation->empty()) {
                    std::cout << "[AI] Context FOUND (" << observation->size() << " bytes). Injecting..." << std::endl;
                    processedContent += "\n\n[CONTEXTO PRE-EXISTENTE (Observação Narrativa)]\n" + *observation + "\n[FIM DO CONTEXTO]\n";
                } else {
                    std::cout << "[AI] No context found or empty." << std::endl;
                }

                auto insight = m_ai->processRawThought(processedContent, fastMode);
                if (insight) {
                    auto meta = insight->getMetadata();
                    
                    // INTENT ROUTING
                    bool isScientific = false;
                    for (const auto& tag : meta.tags) {
                        if (tag == "#ScientificObserver") isScientific = true;
                    }

                    if (isScientific && m_scientificService) {
                         // Intent: Scientific Ingestion (Candidate Bundle)
                         m_scientificService->ingestScientificBundle(insight->getContent(), insightId);
                    } else {
                        // Intent: Standard Note Persistence
                        meta.id = insightId;
                        domain::Insight normalized(meta, insight->getContent());
                        m_knowledge.GetRepository().saveInsight(normalized);
                    }
                }
                break;
            }
        }
        ConsolidateTasksAsync();
    });
}

void AIProcessingService::ConsolidateTasksAsync() {
    m_taskManager->SubmitTask(TaskType::AI_Processing, "Consolidando Tarefas", [this](std::shared_ptr<TaskStatus> status) {
        auto insights = m_knowledge.GetAllInsights();
        std::ostringstream taskList;
        bool hasTasks = false;

        for (auto& insight : insights) {
            if (insight.getMetadata().id == "_Consolidated_Tasks.md") continue;
            
            insight.parseActionablesFromContent();
            for (const auto& task : insight.getActionables()) {
                char s = task.isCompleted ? 'x' : (task.isInProgress ? '/' : ' ');
                taskList << "- [" << s << "] " << task.description << " (origem: " << insight.getMetadata().id << ")\n";
                hasTasks = true;
            }
        }

        if (!hasTasks) {
            m_knowledge.UpdateNote("_Consolidated_Tasks.md", "# Tarefas Consolidadas\n\n");
            return;
        }

        auto consolidated = m_ai->consolidateTasks(taskList.str());
        if (consolidated) {
            std::string filtered = FilterTaskLines(*consolidated);
            if (!filtered.empty()) {
                m_knowledge.UpdateNote("_Consolidated_Tasks.md", "# Tarefas Consolidadas\n\n" + filtered);
            }
        }
    });
}

void AIProcessingService::TranscribeAudioAsync(const std::string& audioPath) {
    if (!m_transcriber) return;
    
    m_taskManager->SubmitTask(TaskType::Transcription, "Transcrevendo: " + audioPath, [this, audioPath](std::shared_ptr<TaskStatus> status) {
        // Since m_transcriber might have its own async, we need to be careful.
        // But here we wrap it in our AsyncTaskManager.
        m_transcriber->transcribeAsync(audioPath, 
            [this](const std::string& text) {
                // Success - the original transcriber usually saves to file
            },
            [status](const std::string& err) {
                status->failed = true;
                status->errorMessage = err;
            }
        );
    });
}

} // namespace ideawalker::application
