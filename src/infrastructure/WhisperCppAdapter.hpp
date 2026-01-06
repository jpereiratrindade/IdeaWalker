#pragma once

#include "domain/TranscriptionService.hpp"
#include <string>
#include <mutex>

// Forward declaration to avoid including whisper.h in header if possible, 
// but whisper_context* is needed.
struct whisper_context;

namespace ideawalker::infrastructure {

class WhisperCppAdapter : public domain::TranscriptionService {
public:
    WhisperCppAdapter(const std::string& modelPath, const std::string& inboxPath);
    ~WhisperCppAdapter() override;

    void transcribeAsync(const std::string& audioPath, OnSuccess onSuccess, OnError onError) override;

private:
    std::string m_modelPath;
    std::string m_inboxPath;
    
    // Whisper context is not thread-safe for parallel inference, 
    // but we will create/destroy or lock it. For simplicity, let's keep one context per app for now.
    // Or better: Load model once, run inference guarded by mutex.
    whisper_context* m_ctx = nullptr;
    std::mutex m_mutex;
    bool m_modelLoaded = false;

    bool loadModel(std::string& errorMsg);
};

} // namespace ideawalker::infrastructure
