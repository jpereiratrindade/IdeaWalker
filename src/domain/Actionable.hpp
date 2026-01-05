#pragma once
#include <string>

namespace ideawalker::domain {

struct Actionable {
    std::string description;
    bool isCompleted = false;

    Actionable(const std::string& desc) : description(desc) {}
};

} // namespace ideawalker::domain
