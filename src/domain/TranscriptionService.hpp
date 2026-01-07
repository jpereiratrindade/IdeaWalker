/**
 * @file TranscriptionService.hpp
 * @brief Interface for audio-to-text transcription.
 */

#pragma once

#include <functional>
#include <string>

namespace ideawalker::domain {

/**
 * @class TranscriptionService
 * @brief Abstract interface for services that convert audio files to text asynchronously.
 */
class TranscriptionService {
public:
    virtual ~TranscriptionService() = default;

    /** @brief Callback for successful transcription. Passed the path to the resulting text file. */
    using OnSuccess = std::function<void(const std::string& textFilePath)>;
    
    /** @brief Callback for transcription failure. Passed an error message. */
    using OnError = std::function<void(const std::string& errorMessage)>;

    /**
     * @brief Initiates an asynchronous transcription process.
     * @param audioPath Path to the input audio file.
     * @param onSuccess Callback invoked on success.
     * @param onError Callback invoked on failure.
     */
    virtual void transcribeAsync(const std::string& audioPath, OnSuccess onSuccess, OnError onError) = 0;
};

} // namespace ideawalker::domain
