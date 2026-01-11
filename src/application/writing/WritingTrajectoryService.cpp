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
}

void WritingTrajectoryService::advanceStage(const std::string& trajectoryId, TrajectoryStage newStage) {
    auto trajOpt = m_repository->findById(trajectoryId);
    if (!trajOpt) {
        throw std::runtime_error("Trajectory not found: " + trajectoryId);
    }

    WritingTrajectory traj = std::move(*trajOpt);
    traj.advanceStage(newStage);
    m_repository->update(traj);
}

size_t WritingTrajectoryService::getTrajectoryCount() const {
    // Inefficient but conforms to interface for now
    return m_repository->findAll().size();
}

std::vector<WritingTrajectory> WritingTrajectoryService::getAllTrajectories() const {
    return m_repository->findAll();
}

std::unique_ptr<WritingTrajectory> WritingTrajectoryService::getTrajectory(const std::string& id) const {
    auto trajOpt = m_repository->findById(id);
    if (trajOpt) {
        return std::make_unique<WritingTrajectory>(std::move(*trajOpt));
    }
    return nullptr;
}

} // namespace ideawalker::application::writing
