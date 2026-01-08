/**
 * @file DocumentIngestionService.cpp
 * @brief Implementation of DocumentIngestionService.
 */

#include "application/DocumentIngestionService.hpp"
#include <filesystem>
#include <fstream>
#include <sstream>
#include <chrono>
#include <iomanip>
#include <algorithm>

namespace fs = std::filesystem;

namespace ideawalker::application {

DocumentIngestionService::DocumentIngestionService(std::unique_ptr<infrastructure::FileSystemArtifactScanner> scanner,
                                                 std::shared_ptr<domain::AIService> aiService,
                                                 const std::string& observationsPath)
    : m_scanner(std::move(scanner)), m_aiService(std::move(aiService)), m_observationsPath(observationsPath) {
    if (!fs::exists(m_observationsPath)) {
        fs::create_directories(m_observationsPath);
    }
}

DocumentIngestionService::IngestionResult DocumentIngestionService::ingestPending(std::function<void(std::string)> statusCallback) {
    IngestionResult result = {0, 0, {}};
    
    if (statusCallback) statusCallback("Varrendo a inbox...");
    auto artifacts = m_scanner->scan();
    result.artifactsDetected = static_cast<int>(artifacts.size());

    for (const auto& artifact : artifacts) {
        // For Phase 1, we process everything. Later we can add "already processed" check via hash database.
        
        if (statusCallback) statusCallback("Processando: " + artifact.filename);

        std::string content;
        std::string ext = fs::path(artifact.path).extension().string();
        std::transform(ext.begin(), ext.end(), ext.begin(), [](unsigned char c){ return std::tolower(c); });

        if (ext == ".pdf") {
            // Basic fallback: Try to use pdftotext (Poppler) via shell
            std::string cmd = "pdftotext \"" + artifact.path + "\" - 2>/dev/null";
            FILE* pipe = popen(cmd.c_str(), "r");
            if (pipe) {
                char buffer[128];
                std::stringstream ss;
                while (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
                    ss << buffer;
                }
                int status = pclose(pipe);
                if (status == 0) {
                    content = ss.str();
                }
            }

            if (content.empty()) {
                content = "[CONTEÚDO BINÁRIO: Extração de texto para PDF falhou ou pdftotext não encontrado. Processe metadados do arquivo se possível.]";
            }
        } else {
            std::ifstream file(artifact.path);
            if (!file.is_open()) {
                result.errors.push_back("Não foi possível abrir: " + artifact.filename);
                continue;
            }
            std::stringstream buffer;
            buffer << file.rdbuf();
            content = buffer.str();
        }

        std::vector<domain::AIService::ChatMessage> history;
        domain::AIService::ChatMessage systemMsg;
        systemMsg.role = domain::AIService::ChatMessage::Role::System;
        systemMsg.content = generateObservationPrompt(artifact, content);
        history.push_back(systemMsg);

        auto response = m_aiService->chat(history);
        if (response) {
            domain::ObservationRecord record;
            record.sourcePath = artifact.path;
            record.sourceHash = artifact.contentHash;
            record.content = *response;
            record.createdAt = std::chrono::system_clock::now();
            
            // ID generation: YYYYMMDD_HHMMSS_Filename
            auto now = std::chrono::system_clock::to_time_t(record.createdAt);
            std::stringstream idss;
            idss << std::put_time(std::localtime(&now), "%Y%m%d_%H%M%S") << "_" << artifact.filename;
            record.id = idss.str();

            saveObservation(record);
            result.observationsGenerated++;
        } else {
            result.errors.push_back("Falha na IA para: " + artifact.filename);
        }
    }

    return result;
}

std::vector<domain::ObservationRecord> DocumentIngestionService::getObservations() const {
    std::vector<domain::ObservationRecord> observations;
    if (!fs::exists(m_observationsPath)) return observations;

    for (const auto& entry : fs::directory_iterator(m_observationsPath)) {
        if (entry.is_regular_file() && entry.path().extension() == ".md") {
            std::ifstream file(entry.path());
            std::stringstream buffer;
            buffer << file.rdbuf();
            
            domain::ObservationRecord obs;
            obs.id = entry.path().filename().string();
            obs.content = buffer.str();
            // Traceability metadata would be parsed from file content header in a real implementation
            observations.push_back(obs);
        }
    }
    return observations;
}

std::string DocumentIngestionService::generateObservationPrompt(const domain::SourceArtifact& artifact, const std::string& content) {
    std::stringstream ss;
    ss << "Você é um Analista de Documentos do IdeaWalker.\n"
       << "Seu objetivo é extrair uma OBSERVAÇÃO NARRATIVA do seguinte artefato.\n\n"
       << "ARQUIVO: " << artifact.filename << "\n"
       << "TIPO: " << (artifact.type == domain::SourceType::Markdown ? "Markdown" : "Texto") << "\n\n"
       << "CONTEÚDO DO ARTEFATO:\n"
       << "------------------------\n"
       << content << "\n"
       << "------------------------\n\n"
       << "REGRAS:\n"
       << "1. Não reescreva o documento.\n"
       << "2. Forneça uma síntese crítica/reflexiva.\n"
       << "3. Identifique potenciais conexões com outros rascunhos.\n"
       << "4. Formate a saída como uma nota Markdown limpa.\n";
    return ss.str();
}

void DocumentIngestionService::saveObservation(const domain::ObservationRecord& record) {
    fs::path outPath = fs::path(m_observationsPath) / (record.id + ".md");
    std::ofstream file(outPath);
    
    file << "# Observação: " << record.id << "\n"
         << "> Fonte: " << record.sourcePath << "\n"
         << "> Hash: " << record.sourceHash << "\n\n"
         << record.content;
}

} // namespace ideawalker::application
