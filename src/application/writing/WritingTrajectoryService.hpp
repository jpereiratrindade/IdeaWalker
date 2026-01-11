/**
 * @file WritingTrajectoryService.hpp
 * @brief Application Service for managing writing trajectories.
 */

#pragma once

#include <memory>
#include <string>
#include <vector>
#include "domain/writing/WritingTrajectory.hpp"
#include "domain/writing/repositories/IWritingTrajectoryRepository.hpp"

namespace ideawalker::application::writing {

using namespace ideawalker::domain::writing;

class WritingTrajectoryService {
public:
    explicit WritingTrajectoryService(std::shared_ptr<IWritingTrajectoryRepository> repository);

    // Creates a new trajectory
    std::string createTrajectory(const std::string& purpose, 
                               const std::string& audience, 
                               const std::string& coreClaim, 
                               const std::string& constraints);

    // Adds a segment (e.g., "Introduction", "Chapter 1")
    void addSegment(const std::string& trajectoryId, 
                  const std::string& title, 
                  const std::string& initialContent, 
                  SourceTag source = SourceTag::Human);

    // Revises a segment with mandatory rationale
    void reviseSegment(const std::string& trajectoryId, 
                     const std::string& segmentId, 
                     const std::string& newContent, 
                     RevisionOperation op, 
                     const std::string& rationale,
                     SourceTag source = SourceTag::Human);

    // Advances the stage of the project
    void advanceStage(const std::string& trajectoryId, TrajectoryStage newStage);

    // Retrieval
    size_t getTrajectoryCount() const;
    std::vector<WritingTrajectory> getAllTrajectories() const;
    std::unique_ptr<WritingTrajectory> getTrajectory(const std::string& id) const;

private:
    std::shared_ptr<IWritingTrajectoryRepository> m_repository;
};

} // namespace ideawalker::application::writing
