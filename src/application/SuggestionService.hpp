/**
 * @file SuggestionService.hpp
 * @brief Service for generating knowledge connection suggestions.
 */

#pragma once
#include <vector>
#include <string>
#include <memory>
#include "domain/Suggestion.hpp"
#include "domain/AIService.hpp"
#include "infrastructure/EmbeddingCache.hpp"

namespace ideawalker::application {

/**
 * @class SuggestionService
 * @brief Responsible for identifying potential connections between notes.
 */
class SuggestionService {
public:
    SuggestionService(std::shared_ptr<domain::AIService> ai, const std::string& projectRoot);
    
    /**
     * @brief Generates semantic suggestions using embeddings.
     * @param activeNoteId The ID of the note currently being viewed.
     * @param content The content of the active note.
     * @return List of suggestions with high similarity.
     */
    std::vector<domain::Suggestion> generateSemanticSuggestions(const std::string& activeNoteId, const std::string& content);
    
    /**
     * @brief Generates narrative suggestions using the "Weaver" persona. (Placeholder for Level 2)
     */
    std::vector<domain::Suggestion> generateNarrativeSuggestions(const std::string& activeNoteId, const std::string& content);

    /**
     * @brief Indexes existing notes to ensure embeddings are available for comparison.
     */
    void indexProject(const std::vector<domain::Insight>& notes);

    /** @brief Saves the embedding cache to disk. */
    void shutdown();

private:
    float cosineSimilarity(const std::vector<float>& v1, const std::vector<float>& v2) const;
    std::string computeHash(const std::string& text) const;

    std::shared_ptr<domain::AIService> m_ai;
    std::string m_projectRoot;
    std::unique_ptr<infrastructure::EmbeddingCache> m_cache;
};

} // namespace ideawalker::application
