/**
 * @file DraftSegment.hpp
 * @brief Entity representing a versioned unit of writing content.
 */

#pragma once

#include <string>
#include <chrono>
#include "../value_objects/EvidenceLink.hpp"

namespace ideawalker::domain::writing {

enum class SourceTag {
    Human,
    AiAssisted,
    AiGenerated,
    HumanReviewed
};

inline std::string SourceTagToString(SourceTag tag) {
    switch(tag) {
        case SourceTag::Human: return "human";
        case SourceTag::AiAssisted: return "ai_assisted";
        case SourceTag::AiGenerated: return "ai_generated";
        case SourceTag::HumanReviewed: return "human_reviewed";
        default: return "unknown";
    }
}

inline SourceTag SourceTagFromString(const std::string& tag) {
    if (tag == "ai_generated") return SourceTag::AiGenerated;
    if (tag == "ai_assisted") return SourceTag::AiAssisted;
    if (tag == "human_reviewed") return SourceTag::HumanReviewed;
    if (tag == "human") return SourceTag::Human;
    return SourceTag::Human;
}

/**
 * @class DraftSegment
 * @brief A section of the writing (paragraph, subsection, etc).
 */
class DraftSegment {
public:
    std::string segmentId;
    std::string title;
    std::string content;
    SourceTag source;
    int version;
    std::chrono::system_clock::time_point lastModified;
    std::vector<EvidenceLink> evidenceLinks;

    DraftSegment(std::string id, std::string t, std::string c, SourceTag s = SourceTag::Human)
        : segmentId(std::move(id)), 
          title(std::move(t)), 
          content(std::move(c)), 
          source(s), 
          version(1),
          lastModified(std::chrono::system_clock::now()) {}

    void update(const std::string& newContent, SourceTag newSource) {
        content = newContent;
        source = newSource;
        version++;
        lastModified = std::chrono::system_clock::now();
    }

    void addEvidence(const EvidenceLink& link) {
        evidenceLinks.push_back(link);
    }
};

} // namespace ideawalker::domain::writing
