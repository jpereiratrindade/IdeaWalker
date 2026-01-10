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



} // namespace ideawalker::application
