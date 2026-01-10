/**
 * @file ConversationService.cpp
 * @brief Implementation of ConversationService.
 */

#include "application/ConversationService.hpp"
#include <filesystem>
#include <algorithm>
#include <sstream>
#include <chrono>
#include <iomanip>
#include <iostream>
#include <thread>
#include <fstream> 

namespace ideawalker::application {

namespace fs = std::filesystem;

ConversationService::ConversationService(std::shared_ptr<domain::AIService> aiService, 
                                         std::shared_ptr<infrastructure::PersistenceService> persistence,
                                         const std::string& projectRoot)
    : m_aiService(std::move(aiService)), m_persistence(std::move(persistence)), m_projectRoot(projectRoot) {}

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

    // Initial System Message
    std::string systemPrompt = generateSystemPrompt(bundle);
    m_history.push_back({domain::AIService::ChatMessage::Role::System, systemPrompt});

    // Save initial state
    saveSession(m_history);
}

void ConversationService::sendMessage(const std::string& userMessage) {
    if (m_projectRoot.empty()) return;

    std::vector<domain::AIService::ChatMessage> historyCopy;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_history.push_back({domain::AIService::ChatMessage::Role::User, userMessage});
        m_isThinking.store(true);
        historyCopy = m_history; // Snapshot for saving
    }
    
    // Save immediately after user message
    saveSession(historyCopy);

    // AI Processing in background
    std::thread([this, historyCopy]() {
        auto responseOpt = m_aiService->chat(historyCopy, true);
        
        std::vector<domain::AIService::ChatMessage> updatedHistorySnapshot;
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            m_isThinking.store(false);
            if (responseOpt) {
                m_history.push_back({domain::AIService::ChatMessage::Role::Assistant, *responseOpt});
            } else {
                m_history.push_back({domain::AIService::ChatMessage::Role::Assistant, "[Erro: Sem resposta do AI]"});
            }
            updatedHistorySnapshot = m_history;
        }
        
        // Save after AI response using the NEW snapshot
        saveSession(updatedHistorySnapshot);

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
    if (m_projectRoot.empty() || m_sessionStartTime.empty() || !m_persistence) return;

    fs::path dialoguesDir = fs::path(m_projectRoot) / "dialogues";
    // Directory creation is handled by PersistenceService or pre-existing logic, 
    // but we can ensure it exists here or rely on PersistenceService's parent_path check relative to filename.
    
    // Generate filename
    std::string safeNoteId = m_currentNoteId;
    std::replace(safeNoteId.begin(), safeNoteId.end(), '/', '_');
    std::replace(safeNoteId.begin(), safeNoteId.end(), '\\', '_');
    
    std::string filename = safeNoteId + "_" + m_sessionStartTime + ".md";
    fs::path finalPath = dialoguesDir / filename;

    std::stringstream ss;
    ss << "# Conversa do Projeto\n\n";
    ss << "Data: " << m_sessionStartTime << "\n";
    ss << "Nota Foco: " << m_currentNoteId << "\n\n";
    ss << "---\n\n";

    for (const auto& msg : historySnapshot) {
        if (msg.role == domain::AIService::ChatMessage::Role::System) continue; 
        
        ss << "### " << (msg.role == domain::AIService::ChatMessage::Role::User ? "Usuário" : "IdeaWalker") << "\n";
        ss << msg.content << "\n\n"; 
    }

    // Delegate IO to PersistenceService
    m_persistence->saveTextAsync(finalPath.string(), ss.str());
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

    // Reset current state
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_history.clear();
        m_currentNoteId = "";
        m_sessionStartTime = "";
        
        // Parse basic metadata from filename or content (Simplified)
        // Filename format: NoteID_Date_Time.md
        // We can recover NoteID roughly
        size_t underscore = filename.find_last_of('_'); 
        // This is tricky without strict parsing, but for now we just load content.
        // Recovering m_sessionStartTime from filename roughly is possible.
        
        // Very basic markdown parsing
        std::string line;
        domain::AIService::ChatMessage currentMsg;
        bool hasMsg = false;
        
        while (std::getline(ifs, line)) {
            if (line.rfind("Data: ", 0) == 0) {
                m_sessionStartTime = line.substr(6);
            } else if (line.rfind("Nota Foco: ", 0) == 0) {
                m_currentNoteId = line.substr(11);
            } else if (line.rfind("### Usuário", 0) == 0) {
                if (hasMsg) m_history.push_back(currentMsg);
                currentMsg.role = domain::AIService::ChatMessage::Role::User;
                currentMsg.content = "";
                hasMsg = true;
            } else if (line.rfind("### IdeaWalker", 0) == 0) {
                if (hasMsg) m_history.push_back(currentMsg);
                currentMsg.role = domain::AIService::ChatMessage::Role::Assistant;
                currentMsg.content = "";
                hasMsg = true;
            } else {
                if (hasMsg && currentMsg.role != domain::AIService::ChatMessage::Role::System) { // Skip preamble
                     if (!line.empty()) currentMsg.content += line + "\n";
                }
            }
        }
        if (hasMsg) m_history.push_back(currentMsg);
    }
    
    return true;
}

} // namespace ideawalker::application
