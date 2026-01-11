/**
 * @file DefensePromptFactory.hpp
 * @brief Service to generate defense prompts based on trajectory state.
 */

#pragma once

#include <vector>
#include <string>
#include "../WritingTrajectory.hpp"
#include "../entities/DefenseCard.hpp"

namespace ideawalker::domain::writing {

class DefensePromptFactory {
public:
    static std::vector<DefenseCard> generatePrompts(const WritingTrajectory& trajectory) {
        std::vector<DefenseCard> cards;
        
        // 1. General Defense of Intent
        std::string intentId = "gen-intent-" + trajectory.getId();
        DefenseCard intentCard(intentId, "global", 
            "How does this work effectively address the core claim: '" + trajectory.getIntent().coreClaim + "'?");
        intentCard.expectedDefensePoints.push_back("Direct evidence links");
        intentCard.expectedDefensePoints.push_back("Logical flow from claim to conclusion");
        cards.push_back(intentCard);

        // 2. Segment-specific defenses based on revisions
        for (const auto& pair : trajectory.getSegments()) {
            const auto& seg = pair.second;
            // Generate a prompt if the segment is short or has many revisions (heuristic)
            if (seg.content.length() < 100) {
                 cards.emplace_back("gen-len-" + seg.segmentId, seg.segmentId, 
                    "Section '" + seg.title + "' appears brief. Can you justify its depth given the audience?");
            }
        }

        return cards;
    }
};

} // namespace ideawalker::domain::writing
