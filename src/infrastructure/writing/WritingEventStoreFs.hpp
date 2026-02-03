/**
 * @file WritingEventStoreFs.hpp
 * @brief File-system based Event Store for Writing Trajectories.
 */

#pragma once

#include <string>
#include <vector>
#include <memory>
#include "domain/writing/events/WritingEvents.hpp"
#include "infrastructure/PersistenceService.hpp"

namespace ideawalker::infrastructure::writing {

using namespace ideawalker::domain::writing;

struct StoredEvent {
    std::string eventType;
    std::string eventDataJson; // The payload
    std::chrono::system_clock::time_point timestamp;
};

class WritingEventStoreFs {
public:
    explicit WritingEventStoreFs(std::string projectRoot, std::shared_ptr<PersistenceService> persistence);

    // Appends new events to the log
    void append(const std::string& trajectoryId, const std::vector<StoredEvent>& events);

    // Reads all events for a trajectory
    std::vector<StoredEvent> readAll(const std::string& trajectoryId);

    // List all trajectory IDs found in storage
    std::vector<std::string> getAllTrajectoryIds();

private:
    std::string m_projectRoot;
    std::shared_ptr<PersistenceService> m_persistence;
    
    std::string getEventsFilePath(const std::string& trajectoryId, bool ensureDirectories);
};

} // namespace ideawalker::infrastructure::writing
