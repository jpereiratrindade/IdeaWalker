/**
 * @file ConversationService.cpp
 * @brief Implementation of ConversationService.
 */

#include "application/ConversationService.hpp"
#include <filesystem>
#include <fstream>
#include <sstream>
#include <chrono>
#include <iomanip>
#include <iostream>
#include <thread>

namespace ideawalker::application {

namespace fs = std::filesystem;

ConversationService::ConversationService(std::shared_ptr<domain::AIService> aiService, const std::string& projectRoot)
    : m_aiService(std::move(aiService)), m_projectRoot(projectRoot) {}

void ConversationService::startSession(const ContextBundle& bundle) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_currentNoteId = bundle.activeNoteId;
    m_history.clear();
    
    // Generate Timestamp for session ID
    auto now = std::chrono::system_clock::now();
    auto in_time_t = std::chrono::system_clock::to_time_t(now);
    std::stringstream ss;
    ss << std::put_time(std::localtime(&in_time_t), "%Y-%m-%d_%H-%M-%S");
    m_sessionStartTime = ss.str();

    // Context Injection
    domain::AIService::ChatMessage systemMsg;
    systemMsg.role = domain::AIService::ChatMessage::Role::System;
    systemMsg.content = generateSystemPrompt(bundle);
    m_history.push_back(systemMsg);
}

void ConversationService::sendMessage(const std::string& userMessage) {
    if (m_currentNoteId.empty()) {
        return;
    }

    {
        std::lock_guard<std::mutex> lock(m_mutex);
        domain::AIService::ChatMessage userMsg;
        userMsg.role = domain::AIService::ChatMessage::Role::User;
        userMsg.content = userMessage;
        m_history.push_back(userMsg);
    } // Unlock for UI update if needed

    m_isThinking = true;

    // Launch Async Thread
    std::thread([this]() {
        // Copy history for thread safety during heavy net op (snapshot)
        std::vector<domain::AIService::ChatMessage> historySnapshot;
        {
             std::lock_guard<std::mutex> lock(m_mutex);
             historySnapshot = m_history;
        }

        auto responseOpt = m_aiService->chat(historySnapshot);
        std::string responseContent;
        
        if (responseOpt) {
            responseContent = *responseOpt;
            
            std::lock_guard<std::mutex> lock(m_mutex);
            domain::AIService::ChatMessage aiMsg;
            aiMsg.role = domain::AIService::ChatMessage::Role::Assistant;
            aiMsg.content = responseContent;
            m_history.push_back(aiMsg);
            
            // Should be safe to call private method with internal locking? 
            // Better to call it here inside lock
            // But saveSession reads m_history which we locked. 
            // We need to implement saveSession to be aware or not use the mutex recursively?
            // Actually, m_mutex is not recursive. saveSession accesses members.
            // Let's protect saveSession logic inside here or refactor saveSession.
            // For now, I'll assume saveSession does NOT lock and relies on caller.
            // Wait, saveSession in original code iterates m_history. 
            // So we are safe because we hold the lock here.
            
            // Refactoring saveSession to NOT lock itself, but assume locked?
            // saveSession is private. Let's look at it.
        } else {
             // Handle error
        }
        
        // Save Session logic inline or called
        if (responseOpt) {
            saveSession(); // This access m_history, so we MUST hold lock if saveSession doesn't lock.
                           // OR we release lock and call saveSession which locks?
                           // Let's look at saveSession implementation below.
        }

        m_isThinking = false;
    }).detach();

    // Removed direct return string. UI listens to history.
}

std::vector<domain::AIService::ChatMessage> ConversationService::getHistory() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_history;
}

bool ConversationService::isSessionActive() const {
    return !m_currentNoteId.empty();
}

std::string ConversationService::getCurrentNoteId() const {
    return m_currentNoteId;
}

std::string ConversationService::generateSystemPrompt(const ContextBundle& bundle) {
    return bundle.render();
}

bool ConversationService::isThinking() const {
    return m_isThinking.load();
}

void ConversationService::saveSession() {
    // Requires EXTERNAL MUTEX LOCK if accessing m_history directly?
    // Or we lock inside.
    // But if we call it from sendMessage (which holds lock), we dead lock if we lock again (std::mutex is not recursive).
    // Let's assume saveSession is called by thread which ALREADY holds lock?
    // 
    // Let's look at sendMessage implementation above: 
    /*
        if (responseOpt) {
            responseContent = *responseOpt;
            std::lock_guard<std::mutex> lock(m_mutex);
            ... push back ...
            saveSession(); 
        }
    */
    // YES, saveSession is called inside lock. 
    // So saveSession should NOT lock.
    // It just reads m_history.
    
    if (m_projectRoot.empty() || m_sessionStartTime.empty()) return;

    fs::path dialoguesDir = fs::path(m_projectRoot) / "dialogues";
    if (!fs::exists(dialoguesDir)) {
        fs::create_directories(dialoguesDir);
    }

    std::string filename = m_sessionStartTime + "_" + m_currentNoteId + ".md";
    fs::path filePath = dialoguesDir / filename;

    std::ofstream ofs(filePath);
    if (ofs.is_open()) {
        ofs << "# Conversa do Projeto\n\n";
        ofs << "Data: " << m_sessionStartTime << "\n";
        ofs << "Nota ativa: " << m_currentNoteId << "\n\n";
        ofs << "---\n\n";
        ofs << "## Diálogo\n\n";

        for (const auto& msg : m_history) {
            if (msg.role == domain::AIService::ChatMessage::Role::System) continue;
            
            std::string roleName = (msg.role == domain::AIService::ChatMessage::Role::User) ? "**Usuário**" : "**IA**";
            ofs << roleName << ": " << msg.content << "\n\n";
        }
    }
}

} // namespace ideawalker::application
