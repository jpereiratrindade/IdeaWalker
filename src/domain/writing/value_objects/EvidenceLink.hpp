/**
 * @file EvidenceLink.hpp
 * @brief Value Object representing an evidentiary support link.
 */

#pragma once

#include <string>

namespace ideawalker::domain::writing {

enum class RefType {
    Note,       ///< Internal IdeaWalker note.
    Pdf,        ///< External PDF file.
    Doi,        ///< Digital Object Identifier.
    Url,        ///< Web link.
    Interview,  ///< Transcript or record.
    Dataset,    ///< Raw data.
    Other
};

/**
 * @struct EvidenceLink
 * @brief Connects a writing segment to a source of truth.
 */
struct EvidenceLink {
    RefType type;
    std::string refId;      ///< Path, DOI, or ID.
    std::string claimAnchor;///< Specific excerpt/range in the source supporting the claim.
    float confidence;       ///< 0.0 to 1.0 author's confidence in this evidence.

    EvidenceLink(RefType t, std::string r, std::string anchor = "", float conf = 1.0f)
        : type(t), refId(std::move(r)), claimAnchor(std::move(anchor)), confidence(conf) {}
};

} // namespace ideawalker::domain::writing
