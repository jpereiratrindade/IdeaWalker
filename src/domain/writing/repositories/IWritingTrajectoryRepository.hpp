/**
 * @file IWritingTrajectoryRepository.hpp
 * @brief Interface for persisting writing trajectories.
 */

#pragma once

#include <vector>
#include <optional>
#include <string>
#include "../WritingTrajectory.hpp"

namespace ideawalker::domain::writing {

class IWritingTrajectoryRepository {
public:
    virtual ~IWritingTrajectoryRepository() = default;

    // Save a brand new trajectory (creates the stream/file)
    virtual void save(const WritingTrajectory& trajectory) = 0;

    // Load by ID
    virtual std::optional<WritingTrajectory> findById(const std::string& id) = 0;

    // List all available trajectories (summary)
    virtual std::vector<WritingTrajectory> findAll() = 0;

    // Update existing (append events)
    // Note: In strict ES, we might just append events. Here passing the Aggregate is convenient.
    virtual void update(WritingTrajectory& trajectory) = 0;
};

} // namespace ideawalker::domain::writing
