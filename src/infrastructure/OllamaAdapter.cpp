#include "infrastructure/OllamaAdapter.hpp"
#include "infrastructure/PromptCatalog.hpp"
#include "infrastructure/ModelSelector.hpp"
#include <iostream>

namespace ideawalker::infrastructure {

using json = nlohmann::json;

OllamaAdapter::OllamaAdapter(const std::string& host, int port)
    : m_client(host, port), m_orchestrator(m_client) {
    // Constructor no longer performs I/O
}

void OllamaAdapter::initialize() {
    auto availableModels = getAvailableModels();
    
    if (availableModels.empty()) {
        std::cerr << "[OllamaAdapter] Failed to list models. Is Ollama running? Keeping default: " << m_model << std::endl;
        return;
    }

    m_model = ModelSelector::SelectBest(availableModels, m_model);
    std::cout << "[OllamaAdapter] Auto-selected model: " << m_model << std::endl;
    // START PATCH: Check if the selected model matches preference (if we had one)
    // Actually, SelectBest already prioritizes m_model.
    // If it returned something else, it means m_model wasn't in list.
    // We should log this clearly.
    // END PATCH
}

std::optional<domain::Insight> OllamaAdapter::processRawThought(const std::string& rawContent, bool fastMode, std::function<void(std::string)> statusCallback) {
    return m_orchestrator.Orchestrate(m_model, rawContent, fastMode, statusCallback);
}

std::optional<std::string> OllamaAdapter::chat(const std::vector<domain::AIService::ChatMessage>& history, bool stream) {
    json messagesJson = json::array();
    for (const auto& msg : history) {
        messagesJson.push_back({
            {"role", domain::AIService::ChatMessage::RoleToString(msg.role)},
            {"content", msg.content}
        });
    }
    return m_client.chat(m_model, messagesJson, stream);
}

std::optional<std::string> OllamaAdapter::generateJson(const std::string& systemPrompt, const std::string& userPrompt) {
    // Upgraded to use Chat API for better compatibility with Instruct models (like Qwen2.5)
    json messages = json::array({
        {{"role", "system"}, {"content", systemPrompt}},
        {{"role", "user"}, {"content", userPrompt}}
    });
    return m_client.chat(m_model, messages, false, true);
}

std::optional<std::string> OllamaAdapter::consolidateTasks(const std::string& tasksMarkdown) {
    return m_client.generate(m_model, PromptCatalog::GetConsolidationPrompt(), tasksMarkdown, false);
}

std::vector<float> OllamaAdapter::getEmbedding(const std::string& text) {
    return m_client.getEmbedding(m_model, text);
}

std::vector<std::string> OllamaAdapter::getAvailableModels() {
    return m_client.getAvailableModels();
}

void OllamaAdapter::setModel(const std::string& modelName) {
    if (m_model != modelName) {
        std::cout << "[OllamaAdapter] Model changed manually to: " << modelName << std::endl;
        m_model = modelName;
    }
}

std::string OllamaAdapter::getCurrentModel() const {
    return m_model;
}

} // namespace ideawalker::infrastructure
