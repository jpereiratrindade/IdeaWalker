/**
 * @file WritingTrajectoryService.cpp
 * @brief Implementation of WritingTrajectoryService.
 */

#include "WritingTrajectoryService.hpp"
#include <iostream>
#include <algorithm>
// #include <uuid/uuid.h> // Dependency removed for portability

namespace ideawalker::application::writing {

// Helper to generate UUIDs (simplistic for MVP if libuuid not linked, fallback to rand)
// But since we are in C++, we might not have uuid linked. 
// Step 52 showed CMakeLists.txt: logic uses `nlohmann_json`, `httplib`, `SDL2`... 
// It does NOT link libuuid. I should use a simple random string for now or add uuid to cmake.
// For now, simple random string generator to avoid build issues.
static std::string generateUUID() {
    static const char alphanum[] = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";
    std::string s;
    s.reserve(16);
    for (int i = 0; i < 16; ++i) {
        s += alphanum[rand() % (sizeof(alphanum) - 1)];
    }
    return s;
}

WritingTrajectoryService::WritingTrajectoryService(std::shared_ptr<IWritingTrajectoryRepository> repository)
    : m_repository(std::move(repository)) {}

std::string WritingTrajectoryService::createTrajectory(const std::string& purpose, 
                                                     const std::string& audience, 
                                                     const std::string& coreClaim, 
                                                     const std::string& constraints) {
    std::string id = generateUUID();
    WritingIntent intent(purpose, audience, coreClaim, constraints);
    
    WritingTrajectory traj(id, intent);
    m_repository->save(traj);
    
    // Eagerly update cache
    if (m_cacheValid) {
        m_trajectoryCache.push_back(traj);
    } // If cache invalid, next read will load everything including this new one
    
    return id;
}

void WritingTrajectoryService::addSegment(const std::string& trajectoryId, 
                                        const std::string& title, 
                                        const std::string& initialContent, 
                                        SourceTag source) {
    auto trajOpt = m_repository->findById(trajectoryId);
    if (!trajOpt) {
        throw std::runtime_error("Trajectory not found: " + trajectoryId);
    }

    // Since findById returns optional<WritingTrajectory> (by value due to optional), 
    // we have a copy. We modify it and update.
    WritingTrajectory traj = std::move(*trajOpt);
    traj.addSegment(title, initialContent, source);
    m_repository->update(traj);
    
    // Update cache if valid
    if (m_cacheValid) {
        auto it = std::find_if(m_trajectoryCache.begin(), m_trajectoryCache.end(), 
            [&](const WritingTrajectory& t) { return t.getId() == trajectoryId; });
        if (it != m_trajectoryCache.end()) {
            *it = traj;
        } else {
             // Should not happen if cache was valid and contained it, but just in case
             m_trajectoryCache.push_back(traj);
        }
    }
}

void WritingTrajectoryService::reviseSegment(const std::string& trajectoryId, 
                                           const std::string& segmentId, 
                                           const std::string& newContent, 
                                           RevisionOperation op, 
                                           const std::string& rationale,
                                           SourceTag source) {
    auto trajOpt = m_repository->findById(trajectoryId);
    if (!trajOpt) {
        throw std::runtime_error("Trajectory not found: " + trajectoryId);
    }

    WritingTrajectory traj = std::move(*trajOpt);
    traj.reviseSegment(segmentId, newContent, op, rationale, source);
    m_repository->update(traj);
    
    if (m_cacheValid) {
        auto it = std::find_if(m_trajectoryCache.begin(), m_trajectoryCache.end(), 
            [&](const WritingTrajectory& t) { return t.getId() == trajectoryId; });
        if (it != m_trajectoryCache.end()) *it = traj;
    }
}

void WritingTrajectoryService::advanceStage(const std::string& trajectoryId, TrajectoryStage newStage) {
    auto trajOpt = m_repository->findById(trajectoryId);
    if (!trajOpt) {
        throw std::runtime_error("Trajectory not found: " + trajectoryId);
    }

    WritingTrajectory traj = std::move(*trajOpt);
    traj.advanceStage(newStage);
    m_repository->update(traj);
    
    if (m_cacheValid) {
        auto it = std::find_if(m_trajectoryCache.begin(), m_trajectoryCache.end(), 
            [&](const WritingTrajectory& t) { return t.getId() == trajectoryId; });
        if (it != m_trajectoryCache.end()) *it = traj;
    }
}

void WritingTrajectoryService::addDefenseCard(const std::string& trajectoryId, 
                                            const std::string& cardId,
                                            const std::string& segmentId, 
                                            const std::string& prompt, 
                                            const std::vector<std::string>& points) {
    auto trajOpt = m_repository->findById(trajectoryId);
    if (!trajOpt) throw std::runtime_error("Trajectory not found");
    
    WritingTrajectory traj = std::move(*trajOpt);
    traj.addDefenseCard(cardId, segmentId, prompt, points);
    m_repository->update(traj);
    
    if (m_cacheValid) {
        auto it = std::find_if(m_trajectoryCache.begin(), m_trajectoryCache.end(), 
            [&](const WritingTrajectory& t) { return t.getId() == trajectoryId; });
        if (it != m_trajectoryCache.end()) *it = traj;
    }
}

void WritingTrajectoryService::updateDefenseStatus(const std::string& trajectoryId, 
                                                 const std::string& cardId, 
                                                 DefenseStatus newStatus, 
                                                 const std::string& response) {
    auto trajOpt = m_repository->findById(trajectoryId);
    if (!trajOpt) throw std::runtime_error("Trajectory not found");

    WritingTrajectory traj = std::move(*trajOpt);
    traj.updateDefenseStatus(cardId, newStatus, response);
    m_repository->update(traj);
    
    if (m_cacheValid) {
        auto it = std::find_if(m_trajectoryCache.begin(), m_trajectoryCache.end(), 
            [&](const WritingTrajectory& t) { return t.getId() == trajectoryId; });
        if (it != m_trajectoryCache.end()) *it = traj;
    }
}

size_t WritingTrajectoryService::getTrajectoryCount() const {
    if (!m_cacheValid) updateCache();
    return m_trajectoryCache.size();
}

std::vector<WritingTrajectory> WritingTrajectoryService::getAllTrajectories() const {
    if (!m_cacheValid) updateCache();
    return m_trajectoryCache;
}

void WritingTrajectoryService::refreshCache() {
    m_cacheValid = false;
    updateCache();
}

void WritingTrajectoryService::updateCache() const {
    m_trajectoryCache = m_repository->findAll();
    m_cacheValid = true;
}

std::unique_ptr<WritingTrajectory> WritingTrajectoryService::getTrajectory(const std::string& id) const {
    auto trajOpt = m_repository->findById(id);
    if (trajOpt) {
        return std::make_unique<WritingTrajectory>(std::move(*trajOpt));
    }
    return nullptr;
}

} // namespace ideawalker::application::writing
