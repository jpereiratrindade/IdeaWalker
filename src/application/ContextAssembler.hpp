/**
 * @file ContextAssembler.hpp
 * @brief Application service to assemble structured context for the LLM.
 */

#pragma once
#include <string>
#include <vector>
#include <memory>
#include <sstream>
#include "application/KnowledgeService.hpp"
#include "application/DocumentIngestionService.hpp"

namespace ideawalker::application {

/**
 * @struct ContextBundle
 * @brief A Value Object containing labeled context segments for the LLM.
 */
struct ContextBundle {
    std::string activeNoteId;
    std::string activeNoteContent;
    std::vector<std::pair<std::string, std::string>> backlinks;
    std::vector<std::pair<std::string, std::string>> observations;
    
    /** @brief Renders the bundle into a single formatted string for the system prompt. */
    /** @brief Renders the bundle into a single formatted string for the system prompt. */
    std::string render() const {
        std::stringstream ss;

        ss << "Você está operando em um Contexto de Diálogo Cognitivo.\n"
           << "Abaixo estão os blocos de contexto estruturados. Use-os para apoiar o usuário.\n\n";

        if (!activeNoteContent.empty()) {
            ss << "=== ACTIVE_NOTE (" << activeNoteId << ") ===\n"
               << activeNoteContent << "\n"
               << "========================================\n\n";
        }

        if (!backlinks.empty()) {
            ss << "=== BACKLINKS (Contexto Adicional) ===\n";
            for (const auto& bl : backlinks) {
                ss << "--- Fonte: " << bl.first << " ---\n"
                   << bl.second << "\n";
            }
            ss << "========================================\n\n";
        }

        if (!observations.empty()) {
            ss << "=== NARRATIVE_OBSERVATIONS (Bases de Dados / Ingestão) ===\n";
            for (const auto& obs : observations) {
                ss << "--- Observação: " << obs.first << " ---\n"
                   << obs.second << "\n";
            }
            ss << "========================================\n\n";
        }

        ss << "Instrução: Responda focando na Nota Ativa, usando os Backlinks e Observações apenas como suporte lateral.\n";

        return ss.str();
    }
    
    bool isEmpty() const { 
        return activeNoteContent.empty() && backlinks.empty() && observations.empty(); 
    }
};

/**
 * @class ContextAssembler
 * @brief Orchestrates the gathering of context from different project areas.
 */
class ContextAssembler {
public:
    ContextAssembler(KnowledgeService& knowledge, DocumentIngestionService& ingestion);

    /**
     * @brief Assembles a context bundle for a specific note.
     * @param noteId The ID of the note.
     * @param noteContent The current content of the note.
     * @return A ContextBundle ready for the LLM.
     */
    ContextBundle assemble(const std::string& noteId, const std::string& noteContent);
    
private:
    KnowledgeService& m_knowledge;
    DocumentIngestionService& m_ingestion;
};

} // namespace ideawalker::application
