/**
 * @file EmbeddingCache.cpp
 * @brief Implementation of EmbeddingCache.
 */

#include "infrastructure/EmbeddingCache.hpp"
#include <fstream>
#include <filesystem>
#include <nlohmann/json.hpp>

using json = nlohmann::json;
namespace fs = std::filesystem;

namespace ideawalker::infrastructure {

EmbeddingCache::EmbeddingCache(const std::string& projectRoot) : m_projectRoot(projectRoot) {}

void EmbeddingCache::update(const std::string& noteId, const std::string& contentHash, const std::vector<float>& embedding) {
    m_entries[noteId] = {contentHash, embedding};
}

std::optional<std::vector<float>> EmbeddingCache::get(const std::string& noteId, const std::string& contentHash) const {
    auto it = m_entries.find(noteId);
    if (it != m_entries.end() && it->second.hash == contentHash) {
        return it->second.vector;
    }
    return std::nullopt;
}

std::map<std::string, std::vector<float>> EmbeddingCache::getAllValid() const {
    std::map<std::string, std::vector<float>> result;
    for (const auto& [id, entry] : m_entries) {
        result[id] = entry.vector;
    }
    return result;
}

void EmbeddingCache::persist() {
    if (m_projectRoot.empty()) return;
    fs::path p = fs::path(m_projectRoot) / ".embeddings.json";
    
    json j = json::object();
    for (const auto& [id, entry] : m_entries) {
        j[id] = { {"hash", entry.hash}, {"vector", entry.vector} };
    }
    
    std::ofstream ofs(p);
    if (ofs.is_open()) {
        ofs << j.dump(4);
    }
}

void EmbeddingCache::load() {
    if (m_projectRoot.empty()) return;
    fs::path p = fs::path(m_projectRoot) / ".embeddings.json";
    if (!fs::exists(p)) return;
    
    try {
        std::ifstream f(p);
        if (!f.is_open()) return;
        
        json j = json::parse(f);
        m_entries.clear();
        for (auto it = j.begin(); it != j.end(); ++it) {
            std::string id = it.key();
            if (it.value().contains("hash") && it.value().contains("vector")) {
                std::string hash = it.value()["hash"];
                std::vector<float> vec = it.value()["vector"];
                m_entries[id] = {hash, vec};
            }
        }
    } catch (...) {}
}

} // namespace ideawalker::infrastructure
