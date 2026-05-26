/**
 * @file PromptCatalog.hpp
 * @brief Central storage for AI persona system prompts.
 *
 * Prompts are loaded from files if present, with hardcoded strings as fallback.
 * To override a persona prompt without recompiling, create a file in the
 * project's `prompts/` directory:
 *
 *   prompts/AnalistaCognitivo.md
 *   prompts/SecretarioExecutivo.md
 *   prompts/Brainstormer.md
 *   prompts/Orquestrador.md
 *   prompts/ScientificObserver.md
 *   prompts/Tecelao.md
 *   prompts/consolidation.md
 *
 * If the file does not exist or cannot be read, the hardcoded prompt is used.
 */

#pragma once

#include <string>
#include "domain/CognitiveModel.hpp"

namespace ideawalker::infrastructure {

class PromptCatalog {
public:
    /**
     * @brief Returns the system prompt for a specific persona.
     * @param persona The target persona.
     * @param promptsDir Optional directory to search for file overrides.
     *                   Typically `<projectRoot>/prompts/`. Pass empty string
     *                   to always use hardcoded prompts.
     */
    static std::string GetSystemPrompt(domain::AIPersona persona, const std::string& promptsDir = "");

    /**
     * @brief Returns the prompt for task consolidation.
     * @param promptsDir Optional directory to search for `consolidation.md`.
     */
    static std::string GetConsolidationPrompt(const std::string& promptsDir = "");

    /** @brief Returns the filename used for a persona's prompt file. */
    static std::string GetPromptFilename(domain::AIPersona persona);

private:
    /** @brief Attempts to load a prompt from file, returns empty string on failure. */
    static std::string TryLoadFromFile(const std::string& promptsDir, const std::string& filename);
};

} // namespace ideawalker::infrastructure
