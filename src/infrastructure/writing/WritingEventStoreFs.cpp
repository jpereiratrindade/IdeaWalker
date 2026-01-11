/**
 * @file WritingEventStoreFs.cpp
 * @brief Implementation of WritingEventStoreFs.
 */

#include "WritingEventStoreFs.hpp"
#include <fstream>
#include <sstream>
#include <filesystem>
#include <nlohmann/json.hpp>

namespace ideawalker::infrastructure::writing {

using json = nlohmann::json;
namespace fs = std::filesystem;

WritingEventStoreFs::WritingEventStoreFs(std::string projectRoot, std::shared_ptr<PersistenceService> persistence)
    : m_projectRoot(std::move(projectRoot)), m_persistence(std::move(persistence)) {}

std::string WritingEventStoreFs::getEventsFilePath(const std::string& trajectoryId) {
    // Structure: <root>/writing/trajectories/<id>/events.ndjson
    fs::path path = fs::path(m_projectRoot) / "writing" / "trajectories" / trajectoryId;
    if (!fs::exists(path)) {
        fs::create_directories(path);
    }
    return (path / "events.ndjson").string();
}

void WritingEventStoreFs::append(const std::string& trajectoryId, const std::vector<StoredEvent>& events) {
    if (events.empty()) return;

    std::string filepath = getEventsFilePath(trajectoryId);
    
    // Read existing content first (to ensure we append correctly w/ PersistenceService)
    // NOTE: This is not efficient for huge logs, but implementing true append in PersistenceService
    // is out of scope for now. For text projects, this is acceptable.
    std::string fileContent;
    if (fs::exists(filepath)) {
        std::ifstream inFile(filepath);
        if (inFile) {
            std::stringstream buffer;
            buffer << inFile.rdbuf();
            fileContent = buffer.str();
            // Ensure ending newline
            if (!fileContent.empty() && fileContent.back() != '\n') {
                fileContent += "\n";
            }
        }
    }

    std::stringstream newContent;
    for (const auto& evt : events) {
        json j;
        j["type"] = evt.eventType;
        j["data"] = json::parse(evt.eventDataJson);
        j["ts"] = std::chrono::duration_cast<std::chrono::milliseconds>(
            evt.timestamp.time_since_epoch()).count();
        newContent << j.dump() << "\n";
    }

    m_persistence->saveTextAsync(filepath, fileContent + newContent.str());
}

std::vector<StoredEvent> WritingEventStoreFs::readAll(const std::string& trajectoryId) {
    std::vector<StoredEvent> results;
    std::string filepath = getEventsFilePath(trajectoryId);
    
    if (!fs::exists(filepath)) return results;

    std::ifstream inFile(filepath);
    std::string line;
    while (std::getline(inFile, line)) {
        if (line.empty()) continue;
        try {
            auto j = json::parse(line);
            StoredEvent evt;
            evt.eventType = j["type"].get<std::string>();
            evt.eventDataJson = j["data"].dump(); // Keep as string for later parsing
            
            long long ts = j.value("ts", 0LL);
            evt.timestamp = std::chrono::time_point<std::chrono::system_clock>(
                std::chrono::milliseconds(ts));
            
            results.push_back(evt);
        } catch (...) {
            // Ignore malformed lines
        }
    }
    return results;
}

} // namespace ideawalker::infrastructure::writing
