/**
 * @file SuggestionService.cpp
 * @brief Implementation of SuggestionService.
 */

#include "application/SuggestionService.hpp"
#include <cmath>
#include <numeric>
#include <filesystem>
#include <fstream>
#include <algorithm>
#include <functional>

namespace fs = std::filesystem;

namespace ideawalker::application {

SuggestionService::SuggestionService(std::shared_ptr<domain::AIService> ai, const std::string& projectRoot)
    : m_ai(ai), m_projectRoot(projectRoot) {
    m_cache = std::make_unique<infrastructure::EmbeddingCache>(projectRoot);
    m_cache->load();
}

std::vector<domain::Suggestion> SuggestionService::generateSemanticSuggestions(const std::string& activeNoteId, const std::string& content) {
    std::vector<domain::Suggestion> suggestions;
    if (content.empty()) return suggestions;

    std::string activeHash = computeHash(content);
    auto activeVecOpt = m_cache->get(activeNoteId, activeHash);
    
    if (!activeVecOpt) {
        auto vec = m_ai->getEmbedding(content);
        if (vec.empty()) return suggestions;
        m_cache->update(activeNoteId, activeHash, vec);
        activeVecOpt = vec;
        m_cache->persist();
    }

    const auto& activeVec = *activeVecOpt;
    auto allEmbeddings = m_cache->getAllValid();

    for (const auto& [id, vec] : allEmbeddings) {
        if (id == activeNoteId) continue;

        float sim = cosineSimilarity(activeVec, vec);
        if (sim > 0.80f) { // Threshold
            domain::Suggestion sug;
            sug.id = activeNoteId + "_" + id;
            sug.sourceId = activeNoteId;
            sug.targetId = id;
            sug.score = sim;
            sug.type = domain::SuggestionType::Semantic;
            
            domain::SuggestionReason reason;
            reason.kind = "Similaridade Semântica";
            int pct = (int)(sim * 100);
            reason.evidence = std::to_string(pct) + "%";
            sug.reasons.push_back(reason);
            
            suggestions.push_back(sug);
        }
    }

    // Sort by score descending
    std::sort(suggestions.begin(), suggestions.end(), [](const auto& a, const auto& b) {
        return a.score > b.score;
    });

    if (suggestions.size() > 5) suggestions.resize(5);

    return suggestions;
}

std::vector<domain::Suggestion> SuggestionService::generateNarrativeSuggestions(const std::string& activeNoteId, const std::string& content) {
    // Level 2 implementation will go here
    return {};
}

void SuggestionService::indexProject(const std::vector<domain::Insight>& notes) {
    bool changed = false;
    for (const auto& note : notes) {
        std::string content = note.getContent();
        std::string id = note.getMetadata().id;
        if (content.empty()) continue;
        
        std::string hash = computeHash(content);
        if (!m_cache->get(id, hash)) {
            auto vec = m_ai->getEmbedding(content);
            if (!vec.empty()) {
                m_cache->update(id, hash, vec);
                changed = true;
            }
        }
    }
    if (changed) {
        m_cache->persist();
    }
}

void SuggestionService::shutdown() {
    m_cache->persist();
}

float SuggestionService::cosineSimilarity(const std::vector<float>& v1, const std::vector<float>& v2) const {
    if (v1.size() != v2.size() || v1.empty()) return 0.0f;
    float dot = 0, n1 = 0, n2 = 0;
    for (size_t i = 0; i < v1.size(); ++i) {
        dot += v1[i] * v2[i];
        n1 += v1[i] * v1[i];
        n2 += v2[i] * v2[i];
    }
    float norm = std::sqrt(n1) * std::sqrt(n2);
    return (norm > 0) ? (dot / norm) : 0.0f;
}

std::string SuggestionService::computeHash(const std::string& text) const {
    return std::to_string(std::hash<std::string>{}(text));
}

} // namespace ideawalker::application
