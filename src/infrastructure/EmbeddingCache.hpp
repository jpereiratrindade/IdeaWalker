/**
 * @file EmbeddingCache.hpp
 * @brief Persistence for semantic embeddings.
 */

#pragma once
#include <string>
#include <vector>
#include <map>
#include <optional>

namespace ideawalker::infrastructure {

/**
 * @class EmbeddingCache
 * @brief Manages a local cache of embeddings to avoid recomputing them for unchanged notes.
 */
class EmbeddingCache {
public:
    explicit EmbeddingCache(const std::string& projectRoot);
    
    /** @brief Updates or adds an embedding to the cache. */
    void update(const std::string& noteId, const std::string& contentHash, const std::vector<float>& embedding);
    
    /** @brief Retrieves an embedding if the hash matches. */
    std::optional<std::vector<float>> get(const std::string& noteId, const std::string& contentHash) const;
    
    /** @brief Gets all valid embeddings currently in cache. */
    std::map<std::string, std::vector<float>> getAllValid() const;

    /** @brief Saves cache to .embeddings.json in project root. */
    void persist();
    
    /** @brief Loads cache from disk. */
    void load();

private:
    std::string m_projectRoot;
    struct CacheEntry {
        std::string hash;
        std::vector<float> vector;
    };
    std::map<std::string, CacheEntry> m_entries;
};

} // namespace ideawalker::infrastructure
