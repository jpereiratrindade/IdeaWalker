/**
 * @file CoherenceLensService.hpp
 * @brief Service to analyze alignment between Intent and Content.
 */

#pragma once

#include <vector>
#include <string>
#include <sstream>
#include "../WritingTrajectory.hpp"

namespace ideawalker::domain::writing {

struct Inconsistency {
    std::string type; // "Structural", "Semantic"
    std::string description;
    std::string severity; // "Low", "Medium", "High"
};

class CoherenceLensService {
public:
    static std::vector<Inconsistency> analyze(const WritingTrajectory& trajectory) {
        std::vector<Inconsistency> issues;
        const auto& intent = trajectory.getIntent();
        
        // Check 1: Empty Core Claim
        if (intent.coreClaim.empty()) {
            issues.push_back({"Structural", "Core Claim is undefined. The trajectory lacks a central thesis.", "High"});
        }

        // Check 2: Segment Alignment (Naive keyword check for MVP)
        // In a real implementation, this would use the AI Adapter to compute semantic similarity.
        if (!intent.coreClaim.empty()) {
            std::stringstream ss(intent.coreClaim);
            std::string word;
            std::vector<std::string> keywords;
            while (ss >> word) {
                if (word.length() > 4) keywords.push_back(word);
            }

            int matchCount = 0;
            for (const auto& pair : trajectory.getSegments()) {
                const auto& content = pair.second.content;
                for (const auto& kw : keywords) {
                    if (content.find(kw) != std::string::npos) {
                        matchCount++;
                    }
                }
            }

            if (matchCount == 0 && !trajectory.getSegments().empty()) {
                issues.push_back({"Semantic", "None of the segments appear to reference key terms from the Core Claim.", "Medium"});
            }
        }

        return issues;
    }
};

} // namespace ideawalker::domain::writing
