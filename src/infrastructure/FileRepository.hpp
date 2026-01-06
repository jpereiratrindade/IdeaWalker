#pragma once
#include "domain/ThoughtRepository.hpp"
#include <string>

namespace ideawalker::infrastructure {

class FileRepository : public domain::ThoughtRepository {
public:
    FileRepository(const std::string& inboxPath, const std::string& notesPath);

    std::vector<domain::RawThought> fetchInbox() override;
    bool shouldProcess(const domain::RawThought& thought, const std::string& insightId) override;
    void saveInsight(const domain::Insight& insight) override;
    void updateNote(const std::string& filename, const std::string& content) override;
    std::vector<domain::Insight> fetchHistory() override;
    std::vector<std::string> getBacklinks(const std::string& filename) override;
    std::map<std::string, int> getActivityHistory() override;

private:
    std::string m_inboxPath;
    std::string m_notesPath;
};

} // namespace ideawalker::infrastructure
