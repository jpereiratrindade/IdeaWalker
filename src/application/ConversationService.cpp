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
            
            // Re-acquire lock to update state
            std::vector<domain::AIService::ChatMessage> currentHistorySnapshot;
            {
                std::lock_guard<std::mutex> lock(m_mutex);
                domain::AIService::ChatMessage aiMsg;
                aiMsg.role = domain::AIService::ChatMessage::Role::Assistant;
                aiMsg.content = responseContent;
                m_history.push_back(aiMsg);
                
                // CRITICAL: Take a snapshot while locked for saving
                // This removes the race condition where saveSession iterated m_history without lock
                currentHistorySnapshot = m_history;
            }
            
            // Save safely OUTSIDE the lock using the snapshot
            // This prevents I/O from blocking the UI thread waiting for the mutex
            saveSession(currentHistorySnapshot); 
        } else {
             // Handle error
        }
        
        m_isThinking = false;
    }).detach();
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

void ConversationService::saveSession(const std::vector<domain::AIService::ChatMessage>& historySnapshot) {
    if (m_projectRoot.empty() || m_sessionStartTime.empty()) return;

    fs::path dialoguesDir = fs::path(m_projectRoot) / "dialogues";
    if (!fs::exists(dialoguesDir)) {
        try {
            fs::create_directories(dialoguesDir);
        } catch (...) {
            std::cerr << "Failed to create dialogues directory: " << dialoguesDir << std::endl;
            return;
        }
    }

    // Generate unique temp filename to avoid collision if multiple threads save simultaneously
    auto timestamp = std::chrono::steady_clock::now().time_since_epoch().count();
    fs::path finalPath = dialoguesDir / filename;
    fs::path tempPath = dialoguesDir / (filename + "." + std::to_string(timestamp) + ".tmp");

    // ATOMIC WRITE PATTERN: Write to .tmp, then rename
    {
        std::ofstream ofs(tempPath);
        if (ofs.is_open()) {
            ofs << "# Conversa do Projeto\n\n";
            ofs << "Data: " << m_sessionStartTime << "\n";
            ofs << "Nota ativa: " << m_currentNoteId << "\n\n";
            ofs << "---\n\n";
            ofs << "## Diálogo\n\n";

            for (const auto& msg : historySnapshot) {
                if (msg.role == domain::AIService::ChatMessage::Role::System) continue;
                
                std::string roleName = (msg.role == domain::AIService::ChatMessage::Role::User) ? "**Usuário**" : "**IA**";
                ofs << roleName << ": " << msg.content << "\n\n";
            }
            
            // Explicit flush to ensure data is on disk (managed by OS buffers, but helps)
            ofs.flush(); 
        } else {
            std::cerr << "Failed to open temp file for writing: " << tempPath << std::endl;
            return;
        }
    } // ofs closed here

    // Perform atomic rename
    try {
        fs::rename(tempPath, finalPath);
    } catch (const std::filesystem::filesystem_error& e) {
        std::cerr << "Failed to rename temp file to final: " << e.what() << std::endl;
        // Try to delete temp file to avoid clutter, though strictly not necessary for correctness
        try { fs::remove(tempPath); } catch (...) {}
    }
}

std::vector<std::string> ConversationService::listDialogues() const {
    std::vector<std::string> files;
    if (m_projectRoot.empty()) return files;

    fs::path dialoguesDir = fs::path(m_projectRoot) / "dialogues";
    if (fs::exists(dialoguesDir) && fs::is_directory(dialoguesDir)) {
        for (const auto& entry : fs::directory_iterator(dialoguesDir)) {
            if (entry.is_regular_file() && entry.path().extension() == ".md") {
                files.push_back(entry.path().filename().string());
            }
        }
    }
    // Sort by name (which has timestamp) descending
    std::sort(files.rbegin(), files.rend());
    return files;
}

bool ConversationService::loadSession(const std::string& filename) {
    if (m_projectRoot.empty()) return false;

    fs::path filePath = fs::path(m_projectRoot) / "dialogues" / filename;
    if (!fs::exists(filePath)) return false;

    std::ifstream ifs(filePath);
    if (!ifs.is_open()) return false;

    std::lock_guard<std::mutex> lock(m_mutex);
    m_history.clear();
    m_currentNoteId = "";
    m_sessionStartTime = "";

    std::string line;
    bool inDialogue = false;
    
    // Parse timestamp and note ID from headers
    while (std::getline(ifs, line)) {
        if (line.rfind("Data: ", 0) == 0) {
            m_sessionStartTime = line.substr(6);
        } else if (line.rfind("Nota ativa: ", 0) == 0) {
            m_currentNoteId = line.substr(12);
        } else if (line.find("## Diálogo") != std::string::npos) {
            inDialogue = true;
            break;
        }
    }

    if (!inDialogue) return false;

    // Simple parser for **Usuário** and **IA**
    domain::AIService::ChatMessage currentMsg;
    bool hasMsg = false;

    while (std::getline(ifs, line)) {
        if (line.empty()) continue;

        if (line.rfind("**Usuário**: ", 0) == 0) {
            if (hasMsg) m_history.push_back(currentMsg);
            currentMsg.role = domain::AIService::ChatMessage::Role::User;
            currentMsg.content = line.substr(13);
            hasMsg = true;
        } else if (line.rfind("**IA**: ", 0) == 0) {
            if (hasMsg) m_history.push_back(currentMsg);
            currentMsg.role = domain::AIService::ChatMessage::Role::Assistant;
            currentMsg.content = line.substr(8);
            hasMsg = true;
        } else {
            if (hasMsg) {
                currentMsg.content += "\n" + line;
            }
        }
    }
    if (hasMsg) m_history.push_back(currentMsg);

    return true;
}

} // namespace ideawalker::application
