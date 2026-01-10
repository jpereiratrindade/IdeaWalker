/**
 * @file ConversationService.hpp
 * @brief Service to manage cognitive dialogue sessions.
 */

#pragma once
#include <vector>
#include <string>
#include <memory>
#include <mutex>
#include <atomic>
#include "domain/AIService.hpp"
#include "application/ContextAssembler.hpp"
#include "infrastructure/PersistenceService.hpp"

namespace ideawalker::application {

/**
 * @class ConversationService
 * @brief Manages the lifecycle of a cognitive dialogue session, context injection, and persistence.
 */
class ConversationService {
public:
    ConversationService(std::shared_ptr<domain::AIService> aiService, 
                        std::shared_ptr<infrastructure::PersistenceService> persistence,
                        const std::string& projectRoot);
    
    /**
     * @brief Starts or restarts a session focused on a specific note.
     * @param bundle The unified context bundle.
     */
    void startSession(const ContextBundle& bundle);
    
    /**
     * @brief Sends a user message to the conversation asynchronously.
     * @param userMessage The user's input.
     */
    void sendMessage(const std::string& userMessage);
    
    /**
     * @brief Gets the current conversation history (Thread-Safe Copy).
     */
    std::vector<domain::AIService::ChatMessage> getHistory() const;
    
    /**
     * @brief Checks if a session is currently active.
     */
    bool isSessionActive() const;

    /**
     * @brief Checks if the AI is currently processing a response.
     */
    bool isThinking() const;

    /**
     * @brief Gets the ID of the note currently in context.
     */
    std::string getCurrentNoteId() const;
    
    /**
     * @brief List all saved dialogue sessions in the project.
     */
    std::vector<std::string> listDialogues() const;

    /**
     * @brief Loads a saved dialogue session.
     * @param filename The name of the file in the dialogues/ directory.
     * @return True if loaded successfully.
     */
    bool loadSession(const std::string& filename);

private:
    void saveSession(const std::vector<domain::AIService::ChatMessage>& historySnapshot);
    std::string generateSystemPrompt(const ContextBundle& bundle);

    std::shared_ptr<domain::AIService> m_aiService;
    std::shared_ptr<infrastructure::PersistenceService> m_persistence;
    std::string m_projectRoot;
    
    std::string m_currentNoteId;
    std::vector<domain::AIService::ChatMessage> m_history;
    std::string m_sessionStartTime;
    
    mutable std::mutex m_mutex;
    std::atomic<bool> m_isThinking{false};
};

} // namespace ideawalker::application
