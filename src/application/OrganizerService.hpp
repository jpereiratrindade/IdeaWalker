#pragma once
#include "domain/ThoughtRepository.hpp"
#include "domain/AIService.hpp"
#include <memory>

namespace ideawalker::application {

class OrganizerService {
public:
    OrganizerService(std::unique_ptr<domain::ThoughtRepository> repo, 
                     std::unique_ptr<domain::AIService> ai);

    void processInbox();
    std::vector<domain::RawThought> getRawThoughts();
    
private:
    std::unique_ptr<domain::ThoughtRepository> m_repo;
    std::unique_ptr<domain::AIService> m_ai;
};

} // namespace ideawalker::application
