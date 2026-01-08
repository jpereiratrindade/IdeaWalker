/**
 * @file ContextAssembler.cpp
 * @brief Implementation of the ContextAssembler service.
 */

#include "application/ContextAssembler.hpp"
#include <sstream>

namespace ideawalker::application {

ContextAssembler::ContextAssembler(OrganizerService& organizer, DocumentIngestionService& ingestion)
    : m_organizer(organizer), m_ingestion(ingestion) {}

ContextBundle ContextAssembler::assemble(const std::string& noteId, const std::string& noteContent) {
    ContextBundle bundle;
    bundle.activeNoteId = noteId;
    bundle.activeNoteContent = noteContent;

    // Fetch Backlinks
    auto backlinkIds = m_organizer.getBacklinks(noteId);
    for (const auto& blId : backlinkIds) {
        std::string content = m_organizer.getNoteContent(blId);
        if (!content.empty()) {
            bundle.backlinks.emplace_back(blId, content);
        }
    }

    // Fetch Observations from Ingestion Service
    auto obsRecords = m_ingestion.getObservations();
    for (const auto& obs : obsRecords) {
        bundle.observations.emplace_back(obs.id, obs.content);
    }

    return bundle;
}

std::string ContextBundle::render() const {
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

} // namespace ideawalker::application
