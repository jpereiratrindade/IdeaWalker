/**
 * @file WritingEventStoreFs.cpp
 * @brief Implementation of WritingEventStoreFs.
 */

#include "WritingEventStoreFs.hpp"
#include <fstream>
#include <sstream>
#include <filesystem>
#include <nlohmann/json.hpp>
#include <iostream>

namespace ideawalker::infrastructure::writing {

using json = nlohmann::json;
namespace fs = std::filesystem;

WritingEventStoreFs::WritingEventStoreFs(std::string projectRoot, std::shared_ptr<PersistenceService> persistence)
    : m_projectRoot(std::move(projectRoot)), m_persistence(std::move(persistence)) {}

std::string WritingEventStoreFs::getEventsFilePath(const std::string& trajectoryId, bool ensureDirectories) {
    // Structure: <root>/writing/trajectories/<id>/events.ndjson
    fs::path path = fs::path(m_projectRoot) / "writing" / "trajectories" / trajectoryId;
    if (ensureDirectories && !fs::exists(path)) {
        fs::create_directories(path);
    }
    return (path / "events.ndjson").string();
}

void WritingEventStoreFs::append(const std::string& trajectoryId, const std::vector<StoredEvent>& events) {
    if (events.empty()) return;

    std::string filepath = getEventsFilePath(trajectoryId, true);
    

    
    // Use synchronous append for data integrity and immediate consistency.
    // This avoids race conditions between creation and subsequent reads, 
    // and ensures events are not lost if the application closes immediately.
    std::ofstream outFile(filepath, std::ios::app);
    if (!outFile) {
        std::cerr << "[WritingEventStoreFs] Failed to open events file for append: " << filepath << std::endl;
        return;
    }

    for (const auto& evt : events) {
        json j;
        j["schemaVersion"] = 1;
        j["type"] = evt.eventType;
        j["data"] = json::parse(evt.eventDataJson);
        j["ts"] = std::chrono::duration_cast<std::chrono::milliseconds>(
            evt.timestamp.time_since_epoch()).count();
        outFile << j.dump() << "\n";
    }
    // File closes automatically and flushes handles
}

std::vector<StoredEvent> WritingEventStoreFs::readAll(const std::string& trajectoryId) {
    std::vector<StoredEvent> results;
    std::string filepath = getEventsFilePath(trajectoryId, false);
    
    if (!fs::exists(filepath)) return results;

    std::ifstream inFile(filepath);
    if (!inFile) {
        std::cerr << "[WritingEventStoreFs] Failed to open events file for read: " << filepath << std::endl;
        return results;
    }
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
            std::cerr << "[WritingEventStoreFs] Ignoring malformed event line in: " << filepath << std::endl;
        }
    }
    return results;
}

std::vector<std::string> WritingEventStoreFs::getAllTrajectoryIds() {
    std::vector<std::string> ids;
    fs::path rootPath = fs::path(m_projectRoot) / "writing" / "trajectories";
    
    if (!fs::exists(rootPath) || !fs::is_directory(rootPath)) {
        return ids;
    }

    for (const auto& entry : fs::directory_iterator(rootPath)) {
        if (entry.is_directory()) {
            // The directory name is the trajectory ID
            ids.push_back(entry.path().filename().string());
        }
    }
    return ids;
}

} // namespace ideawalker::infrastructure::writing
