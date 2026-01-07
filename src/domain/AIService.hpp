/**
 * @file AIService.hpp
 * @brief Interface for AI-powered thought processing.
 */

#pragma once
#include <string>
#include <optional>
#include <functional>
#include "Insight.hpp"

namespace ideawalker::domain {

/**
 * @enum AIPersona
 * @brief Defines the personality/role of the AI.
 */
enum class AIPersona {
    AnalistaCognitivo,   ///< Deep, strategic, tension-focused.
    SecretarioExecutivo, ///< Concise, task-focused, summary.
    Brainstormer,        ///< Expansive, creative, divergent.
    Orquestrador         ///< Meta-persona: diagnoses and sequences other personas.
};

/**
 * @class AIService
 * @brief Abstract interface for services that process raw text into structured insights using AI.
 */
class AIService {
public:
    virtual ~AIService() = default;

    /**
     * @brief Sets the persona for the AI service.
     * @param persona The desired persona.
     */
    virtual void setPersona(AIPersona persona) = 0;

    /**
     * @brief Transforms a raw thought into a structured Insight.
     * @param rawContent Content from a note or transcription.
     * @param statusCallback Optional callback for status.
     * @return Optional Insight object if processing succeeded.
     */
    virtual std::optional<Insight> processRawThought(const std::string& rawContent, std::function<void(std::string)> statusCallback = nullptr) = 0;

    /**
     * @brief Consolidates multiple tasks from different sources into a single markdown list.
     * @param tasksMarkdown Markdown list of tasks to consolidate.
     * @return Optional consolidated markdown string.
     */
    virtual std::optional<std::string> consolidateTasks(const std::string& tasksMarkdown) = 0;
};

} // namespace ideawalker::domain
