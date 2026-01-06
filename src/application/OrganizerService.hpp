#pragma once
#include "domain/ThoughtRepository.hpp"
#include "domain/AIService.hpp"
#include <memory>

namespace ideawalker::application {

class OrganizerService {
public:
    enum class ProcessResult {
        Processed,
        SkippedUpToDate,
        NotFound,
        Failed
    };

    static constexpr const char* kConsolidatedTasksFilename = "_Consolidated_Tasks.md";

    OrganizerService(std::unique_ptr<domain::ThoughtRepository> repo, 
                     std::unique_ptr<domain::AIService> ai);

    void processInbox(bool force = false);
    ProcessResult processInboxItem(const std::string& filename, bool force = false);
    bool updateConsolidatedTasks();
    void updateNote(const std::string& filename, const std::string& content);
    void toggleTask(const std::string& filename, int index);
    void setTaskStatus(const std::string& filename, int index, bool completed, bool inProgress);
    std::vector<std::string> getBacklinks(const std::string& filename);
    std::map<std::string, int> getActivityHistory();
    std::vector<domain::RawThought> getRawThoughts();
    std::vector<domain::Insight> getAllInsights();
    
private:
    std::unique_ptr<domain::ThoughtRepository> m_repo;
    std::unique_ptr<domain::AIService> m_ai;
};

} // namespace ideawalker::application
