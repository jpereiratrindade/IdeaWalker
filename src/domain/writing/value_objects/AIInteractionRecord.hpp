/**
 * @file AIInteractionRecord.hpp
 * @brief Value Object tracking AI assistance for transparency.
 */

#pragma once

#include <string>
#include <chrono>

namespace ideawalker::domain::writing {

enum class AIIntent {
    Brainstorm,
    Critique,
    Rewrite,
    Outline,
    CitationIdeas,
    Other
};

/**
 * @struct AIInteractionRecord
 * @brief Metacognitive record of AI usage.
 */
struct AIInteractionRecord {
    std::string modelName;      ///< E.g., "llama3", "gpt-4".
    AIIntent intent;
    std::string inputHash;      ///< Hash of the prompt for verification.
    std::string outputSummary;  ///< Brief summary of what was generated.
    std::chrono::system_clock::time_point timestamp;

    AIInteractionRecord(std::string model, AIIntent i, std::string hash, std::string summary)
        : modelName(std::move(model)), 
          intent(i), 
          inputHash(std::move(hash)), 
          outputSummary(std::move(summary)),
          timestamp(std::chrono::system_clock::now()) {}
};

} // namespace ideawalker::domain::writing
