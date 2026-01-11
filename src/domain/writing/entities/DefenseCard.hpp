/**
 * @file DefenseCard.hpp
 * @brief Entity representing a specific point of argumentation that requires defense.
 */

#pragma once

#include <string>
#include <vector>

namespace ideawalker::domain::writing {

enum class DefenseStatus {
    Pending,
    Rehearsed,
    Passed
};

struct DefenseCard {
    std::string cardId;
    std::string segmentId; // Optional: empty if global
    std::string prompt;    // The challenge/question
    std::vector<std::string> expectedDefensePoints; // Bullet points
    DefenseStatus status = DefenseStatus::Pending;
    std::string userDefenseResponse; // What the user actually answered/thought

    DefenseCard() = default;
    
    DefenseCard(const std::string& id, const std::string& segId, const std::string& p)
        : cardId(id), segmentId(segId), prompt(p), status(DefenseStatus::Pending) {}

    // Methods
    void markRehearsed(const std::string& response) {
        userDefenseResponse = response;
        status = DefenseStatus::Rehearsed;
    }

    void markPassed() {
        status = DefenseStatus::Passed;
    }
};

} // namespace ideawalker::domain::writing
