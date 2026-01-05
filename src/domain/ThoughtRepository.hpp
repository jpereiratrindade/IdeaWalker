#pragma once
#include <vector>
#include <string>
#include <memory>
#include "Insight.hpp"

namespace ideawalker::domain {

struct RawThought {
    std::string filename;
    std::string content;
};

class ThoughtRepository {
public:
    virtual ~ThoughtRepository() = default;

    virtual std::vector<RawThought> fetchInbox() = 0;
    virtual void saveInsight(const Insight& insight) = 0;
    virtual void updateNote(const std::string& filename, const std::string& content) = 0;
    virtual std::vector<Insight> fetchHistory() = 0;
};

} // namespace ideawalker::domain
