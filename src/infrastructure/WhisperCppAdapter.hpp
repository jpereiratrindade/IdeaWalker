/**
 * @file WhisperCppAdapter.hpp
 * @brief Adapter for on-device transcription using whisper.cpp.
 */

#pragma once

#include "domain/TranscriptionService.hpp"
#include <string>
#include <mutex>

struct whisper_context;

namespace ideawalker::infrastructure {

/**
 * @class WhisperCppAdapter
 * @brief Implements TranscriptionService using the whisper.cpp library for local inference.
 */
class WhisperCppAdapter : public domain::TranscriptionService {
public:
    /**
     * @brief Constructor for WhisperCppAdapter.
     * @param modelPath Path to the GGML model file.
     * @param inboxPath Directory where transcriptions will be saved.
     */
    WhisperCppAdapter(const std::string& modelPath, const std::string& inboxPath);
    
    /** @brief Destructor. Frees the whisper context. */
    ~WhisperCppAdapter() override;

    /** @brief Performs transcription in a background thread. @see domain::TranscriptionService::transcribeAsync */
    void transcribeAsync(const std::string& audioPath, OnSuccess onSuccess, OnError onError) override;

private:
    /**
     * @brief Internal helper to load the model file.
     * @param errorMsg Populated on failure.
     * @return True if model is ready.
     */
    bool loadModel(std::string& errorMsg);

    std::string m_modelPath; ///< Path to .bin model.
    std::string m_inboxPath; ///< Target directory for results.
    
    whisper_context* m_ctx = nullptr; ///< Whisper.cpp execution context.
    std::mutex m_mutex; ///< Protects the shared context during inference.
    bool m_modelLoaded = false; ///< Loading status flag.
};

} // namespace ideawalker::infrastructure
