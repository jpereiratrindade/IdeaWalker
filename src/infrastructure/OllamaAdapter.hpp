#pragma once
#include "domain/AIService.hpp"
#include <string>

namespace ideawalker::infrastructure {

class OllamaAdapter : public domain::AIService {
public:
    OllamaAdapter(const std::string& host = "localhost", int port = 11434);

    std::optional<domain::Insight> processRawThought(const std::string& rawContent) override;

private:
    std::string m_host;
    int m_port;
    std::string m_model = "qwen2.5:7b";
};

} // namespace ideawalker::infrastructure
