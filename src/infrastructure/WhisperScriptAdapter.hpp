#pragma once

#include "domain/TranscriptionService.hpp"
#include <string>
#include <thread>

namespace ideawalker::infrastructure {

class WhisperScriptAdapter : public domain::TranscriptionService {
public:
    WhisperScriptAdapter(const std::string& pythonPath, 
                         const std::string& scriptPath, 
                         const std::string& inboxPath);
    ~WhisperScriptAdapter() override = default;

    void transcribeAsync(const std::string& audioPath, OnSuccess onSuccess, OnError onError) override;

private:
    std::string m_pythonPath;
    std::string m_scriptPath;
    std::string m_inboxPath;
};

} // namespace ideawalker::infrastructure
