#pragma once
#include <string>

namespace ideawalker::domain {

struct Actionable {
    std::string description;
    bool isCompleted = false;
    bool isInProgress = false;

    Actionable(const std::string& desc, bool completed = false, bool inProgress = false) 
        : description(desc), isCompleted(completed), isInProgress(inProgress) {}
};

} // namespace ideawalker::domain
