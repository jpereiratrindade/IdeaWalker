#pragma once
#include <vector>
#include <string>
#include <memory>
#include <map>
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
    virtual bool shouldProcess(const RawThought& thought, const std::string& insightId) = 0;
    virtual void saveInsight(const Insight& insight) = 0;
    virtual void updateNote(const std::string& filename, const std::string& content) = 0;
    virtual std::vector<Insight> fetchHistory() = 0;
    virtual std::vector<std::string> getBacklinks(const std::string& filename) = 0;
    virtual std::map<std::string, int> getActivityHistory() = 0;
};

} // namespace ideawalker::domain
