/**
 * @file ContextAssembler.hpp
 * @brief Application service to assemble structured context for the LLM.
 */

#pragma once
#include <string>
#include <vector>
#include <memory>
#include "application/OrganizerService.hpp"
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
    std::string render() const;
    
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
    ContextAssembler(OrganizerService& organizer, DocumentIngestionService& ingestion);

    /**
     * @brief Assembles a context bundle for a specific note.
     * @param noteId The ID of the note.
     * @param noteContent The current content of the note.
     * @return A ContextBundle ready for the LLM.
     */
    ContextBundle assemble(const std::string& noteId, const std::string& noteContent);

private:
    OrganizerService& m_organizer;
    DocumentIngestionService& m_ingestion;
};

} // namespace ideawalker::application
