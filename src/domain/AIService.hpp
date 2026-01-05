#pragma once
#include <string>
#include <optional>
#include "Insight.hpp"

namespace ideawalker::domain {

class AIService {
public:
    virtual ~AIService() = default;

    virtual std::optional<Insight> processRawThought(const std::string& rawContent) = 0;
};

} // namespace ideawalker::domain
