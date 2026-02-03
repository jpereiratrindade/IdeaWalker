/**
 * @file OllamaAdapter.hpp
 * @brief Adapter for communication with a local Ollama server.
 */

#pragma once
#include "domain/AIService.hpp"
#include <string>

#include "infrastructure/OllamaClient.hpp"
#include "infrastructure/PersonaOrchestrator.hpp"

namespace ideawalker::infrastructure {

/**
 * @class OllamaAdapter
 * @brief Implements AIService using the Ollama REST API.
 */
class OllamaAdapter : public domain::AIService {
public:
    OllamaAdapter(const std::string& host = "localhost", int port = 11434);
    
    /** @brief Checks connection and auto-detects best available model. */
    void initialize() override;

    std::optional<domain::Insight> processRawThought(const std::string& rawContent, bool fastMode = false, std::function<void(std::string)> statusCallback = nullptr) override;
    std::optional<std::string> chat(const std::vector<domain::AIService::ChatMessage>& history, bool stream = false) override;
    std::optional<std::string> consolidateTasks(const std::string& tasksMarkdown) override;
    std::vector<float> getEmbedding(const std::string& text) override;

    std::vector<std::string> getAvailableModels() override;
    void setModel(const std::string& modelName) override;
    std::string getCurrentModel() const override;

private:
    OllamaClient m_client;
    PersonaOrchestrator m_orchestrator;
    std::string m_model = "qwen2.5:7b";
};

} // namespace ideawalker::infrastructure
