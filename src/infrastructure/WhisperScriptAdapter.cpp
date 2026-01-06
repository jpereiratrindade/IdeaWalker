#include "infrastructure/WhisperScriptAdapter.hpp"

#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <sstream>

namespace ideawalker::infrastructure {

WhisperScriptAdapter::WhisperScriptAdapter(const std::string& pythonPath, 
                                           const std::string& scriptPath, 
                                           const std::string& inboxPath)
    : m_pythonPath(pythonPath)
    , m_scriptPath(scriptPath)
    , m_inboxPath(inboxPath)
{}

void WhisperScriptAdapter::transcribeAsync(const std::string& audioPath, OnSuccess onSuccess, OnError onError) {
    // Check if files exist
    if (!std::filesystem::exists(audioPath)) {
        if (onError) onError("Audio file not found: " + audioPath);
        return;
    }

    std::thread([this, audioPath, onSuccess, onError]() {
        namespace fs = std::filesystem;

        // Build command
        // export NO_PROXY=localhost && python script "audio"
        std::stringstream cmd;
#if defined(_WIN32)
        cmd << "set NO_PROXY=localhost && ";
#else
        cmd << "export NO_PROXY=localhost && ";
#endif
        cmd << "\"" << m_pythonPath << "\" \"" << m_scriptPath << "\" \"" << audioPath << "\"";

        std::cout << "[WhisperAdapter] Running: " << cmd.str() << std::endl;

        int result = std::system(cmd.str().c_str());

        if (result != 0) {
            if (onError) onError("Transcription script failed with code: " + std::to_string(result));
            return;
        }

        // Move result file
        // Expected format: audio_stem + "_transcricao.txt" in the SAME directory as audio
        fs::path audioP(audioPath);
        std::string txtName = audioP.stem().string() + "_transcricao.txt";
        fs::path sourceTxt = audioP.parent_path() / txtName;
        fs::path destTxt = fs::path(m_inboxPath) / txtName;

        if (fs::exists(sourceTxt)) {
            try {
                fs::rename(sourceTxt, destTxt);
                if (onSuccess) onSuccess(destTxt.string());
            } catch (const std::exception& e) {
                if (onError) onError("Failed to move transcription to inbox: " + std::string(e.what()));
            }
        } else {
            if (onError) onError("Transcription output not found at: " + sourceTxt.string());
        }

    }).detach();
}

} // namespace ideawalker::infrastructure
