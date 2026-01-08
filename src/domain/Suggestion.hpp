/**
 * @file Suggestion.hpp
 * @brief Domain entities for the Suggestion Engine.
 */

#pragma once
#include <string>
#include <vector>

namespace ideawalker::domain {

/**
 * @enum SuggestionType
 * @brief Categorizes why a suggestion was made.
 */
enum class SuggestionType {
    Semantic,   ///< Based on vector similarity (Embeddings).
    Narrative,  ///< Based on AI analysis of tensions and axes (The Weaver).
    Intentional ///< Based on user-expressed intent or pattern.
};

/**
 * @enum SuggestionStatus
 * @brief Current state of a suggestion in the UI workflow.
 */
enum class SuggestionStatus {
    Pending,
    Accepted,
    Rejected,
    Snoozed
};

/**
 * @struct SuggestionReason
 * @brief Explains why a suggestion was generated.
 */
struct SuggestionReason {
    std::string kind;    ///< e.g., "Similarity", "Convergência de Tensão"
    std::string evidence; ///< e.g., "92%", "Ambas tratam de resiliência"
};

/**
 * @class Suggestion
 * @brief A potential connection between two pieces of knowledge.
 */
struct Suggestion {
    std::string id;
    std::string sourceId; // Filename of the active note
    std::string targetId; // Filename of the suggested note
    float score;          // 0.0 to 1.0
    SuggestionType type;
    SuggestionStatus status = SuggestionStatus::Pending;
    std::vector<SuggestionReason> reasons;
    std::string createdAt;
};

} // namespace ideawalker::domain
