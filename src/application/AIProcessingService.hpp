/**
 * @file AIProcessingService.hpp
 * @brief Service for orchestrating AI-powered cognitive pipelines.
 */

#pragma once

#include "application/KnowledgeService.hpp"
#include "application/AsyncTaskManager.hpp"
#include "domain/AIService.hpp"
#include "domain/TranscriptionService.hpp"
#include <memory>
#include <string>
#include <functional>

namespace ideawalker::application {

/**
 * @class AIProcessingService
 * @brief Orchestrates AI tasks and managed background execution via AsyncTaskManager.
 */
class AIProcessingService {
public:
    AIProcessingService(KnowledgeService& knowledge,
                        std::shared_ptr<domain::AIService> ai,
                        std::shared_ptr<AsyncTaskManager> taskManager,
                        std::unique_ptr<domain::TranscriptionService> transcriber);

    /** @brief Triggers background processing of the entire inbox. */
    void ProcessInboxAsync(bool force = false, bool fastMode = false);

    /** @brief Triggers background processing of a specific inbox item. */
    void ProcessItemAsync(const std::string& filename, bool force = false, bool fastMode = false);

    /** @brief Triggers background task consolidation. */
    void ConsolidateTasksAsync();

    /** @brief Triggers background audio transcription. */
    void TranscribeAudioAsync(const std::string& audioPath);

    /** @brief Returns access to the underlying AI service. */
    domain::AIService* GetAI() const { return m_ai.get(); }

private:
    KnowledgeService& m_knowledge;
    std::shared_ptr<domain::AIService> m_ai;
    std::shared_ptr<AsyncTaskManager> m_taskManager;
    std::unique_ptr<domain::TranscriptionService> m_transcriber;

    // Internal helpers
    static std::string NormalizeToId(const std::string& filename);
    static std::string FilterTaskLines(const std::string& text);
};

} // namespace ideawalker::application
