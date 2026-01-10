/**
 * @file OllamaAdapter.hpp
 * @brief Adapter for communication with a local Ollama server.
 */

#pragma once
#include "domain/AIService.hpp"
#include <string>

namespace ideawalker::infrastructure {

/**
 * @class OllamaAdapter
 * @brief Implements AIService using the Ollama REST API.
 */
class OllamaAdapter : public domain::AIService {
public:
    /**
     * @brief Constructor for OllamaAdapter.
     * @param host Server hostname or IP.
     * @param port Server port.
     */
    OllamaAdapter(const std::string& host = "localhost", int port = 11434);
    
    /** @brief Processes text using a structured prompt. @see domain::AIService::processRawThought */
    std::optional<domain::Insight> processRawThought(const std::string& rawContent, bool fastMode = false, std::function<void(std::string)> statusCallback = nullptr) override;

    /** @brief Sends a chat history to the AI. @see domain::AIService::chat */
    std::optional<std::string> chat(const std::vector<domain::AIService::ChatMessage>& history, bool stream = false) override;

    /** @brief Unifies and rewrites tasks. @see domain::AIService::consolidateTasks */
    std::optional<std::string> consolidateTasks(const std::string& tasksMarkdown) override;

    /** @brief Generates a semantic embedding vector. @see domain::AIService::getEmbedding */
    std::vector<float> getEmbedding(const std::string& text) override;

private:
    void detectBestModel();
    std::string getSystemPrompt(domain::AIPersona persona);
    std::optional<std::string> generateRawResponse(const std::string& systemPrompt,
                                                   const std::string& userContent,
                                                   bool forceJson);
    std::string m_host; ///< Ollama host.
    int m_port; ///< Ollama port.
    std::string m_model = "qwen2.5:7b"; ///< Target model name.
    domain::AIPersona m_currentPersona = domain::AIPersona::AnalistaCognitivo; ///< Current persona.
};

} // namespace ideawalker::infrastructure
