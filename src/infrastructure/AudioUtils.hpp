#pragma once

#include <string>
#include <vector>

namespace ideawalker::infrastructure {

/**
 * @brief Utilities for audio processing.
 */
class AudioUtils {
public:
    /**
     * @brief Executes a system command.
     */
    static int ExecCmd(const std::string& cmd);

    /**
     * @brief Converts an audio file to 16kHz mono WAV using ffmpeg.
     * @param inputPath Path to source file.
     * @param outputPath Output path (populated automatically if empty).
     * @param error Populated on failure.
     * @return True if successful.
     */
    static bool ConvertAudioToWav(const std::string& inputPath, std::string& outputPath, std::string& error);

    /**
     * @brief Loads a WAV file and converts it to 16kHz float32 mono (Whisper format).
     * @param fname Path to WAV file.
     * @param pcmf32 Resulting vector of samples.
     * @param error Populated on failure.
     * @return True if successful.
     */
    static bool LoadAudioSDL(const std::string& fname, std::vector<float>& pcmf32, std::string& error);
};

} // namespace ideawalker::infrastructure
