/**
 * @file PersonaOrchestrator.hpp
 * @brief Orchestrates multiple AI personas for complex thought processing.
 */

#pragma once

#include <string>
#include <vector>
#include <optional>
#include <functional>
#include "domain/Insight.hpp"
#include "domain/CognitiveModel.hpp"
#include "infrastructure/OllamaClient.hpp"
#include "infrastructure/PromptCatalog.hpp"

namespace ideawalker::infrastructure {

class PersonaOrchestrator {
public:
    PersonaOrchestrator(OllamaClient& client);

    /** @brief Executes the cognitive pipeline on raw text. */
    std::optional<domain::Insight> Orchestrate(const std::string& model,
                                               const std::string& rawContent, 
                                               bool fastMode, 
                                               std::function<void(std::string)> statusCallback);

private:
    OllamaClient& m_client;

    /** @brief Helper to create a cognitive snapshot. */
    domain::CognitiveSnapshot CreateSnapshot(domain::AIPersona persona,
                                             const std::string& input,
                                             const std::string& output);

    /** @brief Parses the orchestration plan from the AI response. */
    void ParsePlan(const std::string& planJson, 
                   std::vector<domain::AIPersona>& outSequence, 
                   std::vector<std::string>& outTags);
};

} // namespace ideawalker::infrastructure
