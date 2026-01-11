/**
 * @file WritingTrajectoryRepositoryFs.hpp
 * @brief File system implementation of the WritingTrajectory repository.
 */

#pragma once

#include "domain/writing/repositories/IWritingTrajectoryRepository.hpp"
#include "WritingEventStoreFs.hpp"

namespace ideawalker::infrastructure::writing {

using namespace ideawalker::domain::writing;

class WritingTrajectoryRepositoryFs : public IWritingTrajectoryRepository {
public:
    WritingTrajectoryRepositoryFs(std::unique_ptr<WritingEventStoreFs> eventStore);

    void save(const WritingTrajectory& trajectory) override;
    std::optional<WritingTrajectory> findById(const std::string& id) override;
    std::vector<WritingTrajectory> findAll() override;
    void update(WritingTrajectory& trajectory) override;

private:
    std::unique_ptr<WritingEventStoreFs> m_eventStore;
    
    // Helper to serialize events from the aggregate
    std::vector<StoredEvent> serializeEvents(const std::string& trajectoryId, 
                                           // Ideally we get a list of events from the aggregate
                                           // For MVP we might need to change Aggregate to expose them
                                           const std::vector<std::variant<TrajectoryCreated, SegmentAdded, SegmentRevised, StageAdvanced>>& domainEvents);
};

} // namespace ideawalker::infrastructure::writing
