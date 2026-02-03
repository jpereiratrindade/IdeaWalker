/**
 * @file OllamaClient.hpp
 * @brief Low-level HTTP client for the Ollama REST API.
 */

#pragma once

#include <string>
#include <vector>
#include <optional>
#include <nlohmann/json.hpp>

namespace ideawalker::infrastructure {

class OllamaClient {
public:
    OllamaClient(const std::string& host = "localhost", int port = 11434);

    /** @brief Sends a POST request to /api/generate. */
    std::optional<std::string> generate(const std::string& model, 
                                        const std::string& system, 
                                        const std::string& prompt, 
                                        bool forceJson = false);

    /** @brief Sends a POST request to /api/chat. */
    std::optional<std::string> chat(const std::string& model, 
                                   const nlohmann::json& messages, 
                                   bool stream = false);

    /** @brief Sends a POST request to /api/embeddings. */
    std::vector<float> getEmbedding(const std::string& model, const std::string& text);

    /** @brief Fetches available models from /api/tags. */
    std::vector<std::string> getAvailableModels();

private:
    std::string m_host;
    int m_port;
};

} // namespace ideawalker::infrastructure
