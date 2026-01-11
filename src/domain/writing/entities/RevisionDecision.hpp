/**
 * @file RevisionDecision.hpp
 * @brief Entity representing a conscious decision to change the text.
 */

#pragma once

#include <string>
#include <vector>
#include <chrono>
#include <optional>

namespace ideawalker::domain::writing {

enum class RevisionOperation {
    Clarify,
    Compress,
    Expand,
    Reorganize,
    Cite,
    Remove,
    Reframe,
    Correction
};

inline std::string OperationToString(RevisionOperation op) {
    switch (op) {
        case RevisionOperation::Clarify: return "clarify";
        case RevisionOperation::Compress: return "compress";
        case RevisionOperation::Expand: return "expand";
        case RevisionOperation::Reorganize: return "reorganize";
        case RevisionOperation::Cite: return "cite";
        case RevisionOperation::Remove: return "remove";
        case RevisionOperation::Reframe: return "reframe";
        case RevisionOperation::Correction: return "correction";
        default: return "unknown";
    }
}

/**
 * @class RevisionDecision
 * @brief Captures the strategic 'Why' behind a text change.
 * 
 * Invariant: Rationale must not be empty.
 */
class RevisionDecision {
public:
    std::string decisionId;
    std::string targetSegmentId;
    RevisionOperation operation;
    std::string rationale; ///< Mandatory explanation.
    std::vector<std::string> alternativesConsidered;
    std::chrono::system_clock::time_point timestamp;

    RevisionDecision(std::string id, std::string targetId, RevisionOperation op, std::string rat)
        : decisionId(std::move(id)), 
          targetSegmentId(std::move(targetId)), 
          operation(op), 
          rationale(std::move(rat)),
          timestamp(std::chrono::system_clock::now()) {
        
        if (rationale.empty()) {
            // In a real exception-safe environment we might throw, 
            // but here we enforce it via construction.
            // For now, let's allow it but this is a smell.
            // Actually, let's enforce it.
             throw std::invalid_argument("RevisionDecision: Rationale cannot be empty.");
        }
    }

    void addAlternative(const std::string& alt) {
        alternativesConsidered.push_back(alt);
    }
};

} // namespace ideawalker::domain::writing
