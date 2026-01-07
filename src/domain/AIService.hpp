/**
 * @file AIService.hpp
 * @brief Interface for AI-powered thought processing.
 */

#pragma once
#include <string>
#include <optional>
#include "Insight.hpp"

namespace ideawalker::domain {

/**
 * @class AIService
 * @brief Abstract interface for services that process raw text into structured insights using AI.
 */
class AIService {
public:
    virtual ~AIService() = default;

    /**
     * @brief Transforms a raw thought into a structured Insight.
     * @param rawContent Content from a note or transcription.
     * @return Optional Insight object if processing succeeded.
     */
    virtual std::optional<Insight> processRawThought(const std::string& rawContent) = 0;

    /**
     * @brief Consolidates multiple tasks from different sources into a single markdown list.
     * @param tasksMarkdown Markdown list of tasks to consolidate.
     * @return Optional consolidated markdown string.
     */
    virtual std::optional<std::string> consolidateTasks(const std::string& tasksMarkdown) = 0;
};

} // namespace ideawalker::domain
