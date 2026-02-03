/**
 * @file AIService.hpp
 * @brief Interface for AI-powered thought processing.
 */

#pragma once
#include <string>
#include <optional>
#include <functional>
#include <vector>
#include "Insight.hpp"
#include "CognitiveModel.hpp"

namespace ideawalker::domain {

/**
 * @class AIService
 * @brief Abstract interface for services that process raw text into structured insights using AI.
 */
class AIService {
public:
    virtual ~AIService() = default;

    /** @brief Optional initialization (e.g., connection check, model detection). */
    virtual void initialize() {}


    /**
     * @struct ChatMessage
     * @brief Represents a single message in a chat conversation.
     */
    struct ChatMessage {
        enum class Role { System, User, Assistant };
        Role role;
        std::string content;
        
        static std::string RoleToString(Role r) {
            switch(r) {
                case Role::System: return "system";
                case Role::User: return "user";
                case Role::Assistant: return "assistant";
            }
            return "user";
        }
    };

    /**
     * @brief Transforms a raw thought into a structured Insight.
     * @param rawContent Content from a note or transcription.
     * @param statusCallback Optional callback for status.
     * @return Optional Insight object if processing succeeded.
     */
    virtual std::optional<Insight> processRawThought(const std::string& rawContent, bool fastMode = false, std::function<void(std::string)> statusCallback = nullptr) = 0;

    /**
     * @brief Sends a chat history to the AI and gets the next response.
     * @param history The conversation history.
     * @param stream Whether to stream the response (currently unused/false).
     * @return The assistant's response content.
     */
    virtual std::optional<std::string> chat(const std::vector<ChatMessage>& history, bool stream = false) = 0;

    /**
     * @brief Generates a JSON-only response using a system prompt and a user prompt.
     * @param systemPrompt System-level instructions (must enforce JSON-only output).
     * @param userPrompt User content (the document or request).
     * @return Optional JSON string if generation succeeded.
     */
    virtual std::optional<std::string> generateJson(const std::string& systemPrompt, const std::string& userPrompt) {
        (void)systemPrompt;
        (void)userPrompt;
        return std::nullopt;
    }

    /**
     * @brief Consolidates multiple tasks from different sources into a single markdown list.
     * @param tasksMarkdown Markdown list of tasks to consolidate.
     * @return Optional consolidated markdown string.
     */
    virtual std::optional<std::string> consolidateTasks(const std::string& tasksMarkdown) = 0;

    /**
     * @brief Generates a semantic embedding vector for the given text.
     * @param text The text to embed.
     * @return A vector of floats representing the embedding.
     */
    virtual std::vector<float> getEmbedding(const std::string& text) = 0;

    /**
     * @brief Retrieves a list of available AI models from the provider.
     * @return Vector of model names.
     */
    virtual std::vector<std::string> getAvailableModels() = 0;

    /**
     * @brief Sets the specific AI model to use for future requests.
     * @param modelName The name of the model to select (must be available).
     */
    virtual void setModel(const std::string& modelName) = 0;

    /**
     * @brief Gets the name of the currently selected AI model.
     * @return The model name.
     */
    virtual std::string getCurrentModel() const = 0;
};

} // namespace ideawalker::domain
