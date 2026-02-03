/**
 * @file PromptCatalog.hpp
 * @brief Central storage for AI persona system prompts.
 */

#pragma once

#include <string>
#include "domain/CognitiveModel.hpp"

namespace ideawalker::infrastructure {

class PromptCatalog {
public:
    /** @brief Returns the system prompt for a specific persona. */
    static std::string GetSystemPrompt(domain::AIPersona persona);

    /** @brief Returns the prompt for task consolidation. */
    static std::string GetConsolidationPrompt();
};

} // namespace ideawalker::infrastructure
