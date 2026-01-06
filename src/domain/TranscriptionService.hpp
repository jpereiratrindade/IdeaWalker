#pragma once

#include <functional>
#include <string>

namespace ideawalker::domain {

class TranscriptionService {
public:
    virtual ~TranscriptionService() = default;

    using OnSuccess = std::function<void(const std::string& textFilePath)>;
    using OnError = std::function<void(const std::string& errorMessage)>;

    virtual void transcribeAsync(const std::string& audioPath, OnSuccess onSuccess, OnError onError) = 0;
};

} // namespace ideawalker::domain
