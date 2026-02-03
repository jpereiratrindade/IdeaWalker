#include <cassert>
#include <filesystem>
#include <iostream>

#include "application/writing/WritingTrajectoryService.hpp"
#include "domain/writing/WritingTrajectory.hpp"
#include "infrastructure/PersistenceService.hpp"
#include "infrastructure/writing/WritingEventStoreFs.hpp"
#include "infrastructure/writing/WritingTrajectoryRepositoryFs.hpp"

using namespace ideawalker::domain::writing;
using namespace ideawalker::application::writing;
using namespace ideawalker::infrastructure::writing;

int main() {
    std::cout << "[Test] Starting WritingTrajectory Round-Trip Test..." << std::endl;

    std::string testRoot = "test_project_root_writing";
    std::filesystem::create_directories(testRoot);

    auto persistence = std::make_shared<ideawalker::infrastructure::PersistenceService>();
    auto eventStore = std::make_unique<WritingEventStoreFs>(testRoot, persistence);
    auto repo = std::make_shared<WritingTrajectoryRepositoryFs>(std::move(eventStore));
    WritingTrajectoryService service(repo);

    // Create
    std::string id = service.createTrajectory(
        "Argumentar", 
        "Banca acadêmica", 
        "Separar observação/simulação/recomendação é condição para rigor.", 
        "Formato ABNT"
    );

    // Add segment
    service.addSegment(id, "Introdução", "Texto inicial", SourceTag::Human);

    // Revise segment
    auto trajAfterAdd = service.getTrajectory(id);
    assert(trajAfterAdd && "Trajectory should exist after creation.");

    std::string segmentId = trajAfterAdd->getSegments().begin()->first;
    service.reviseSegment(
        id,
        segmentId,
        "Texto revisado",
        RevisionOperation::Clarify,
        "Clarificar tese",
        SourceTag::AiAssisted
    );

    // Advance stage
    service.advanceStage(id, TrajectoryStage::Outline);

    // Defense
    service.addDefenseCard(id, "card-1", segmentId, "Defenda a tese central.", {"Ponto A", "Ponto B"});
    service.updateDefenseStatus(id, "card-1", DefenseStatus::Rehearsed, "Resposta de ensaio.");

    // Rehydrate
    auto traj = repo->findById(id);
    assert(traj && "Trajectory should be rehydrated.");

    // Validate
    assert(traj->getIntent().purpose == "Argumentar");
    assert(traj->getStage() == TrajectoryStage::Outline);
    assert(traj->getSegments().size() == 1);
    assert(traj->getHistory().size() == 1);

    const auto& seg = traj->getSegments().begin()->second;
    assert(seg.content == "Texto revisado");
    assert(seg.source == SourceTag::AiAssisted);

    const auto& cards = traj->getDefenseCards();
    assert(cards.size() == 1);
    assert(cards[0].status == DefenseStatus::Rehearsed);

    std::filesystem::remove_all(testRoot);
    std::cout << "[PASS] WritingTrajectory Round-Trip Test." << std::endl;
    return 0;
}
