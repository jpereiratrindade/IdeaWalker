/**
 * @file WhisperCppAdapter.cpp
 * @brief Implementation of the WhisperCppAdapter class.
 */
#include "infrastructure/WhisperCppAdapter.hpp"
#include "whisper.h" 

#include <SDL.h>
#include <filesystem>
#include <iostream>
#include <vector>
#include <thread>
#include <fstream>
#include <cmath>
#include <algorithm>

#include "infrastructure/AudioUtils.hpp"

namespace ideawalker::infrastructure {

namespace {
    // Local internal helpers (if any remain)
} // namespace

WhisperCppAdapter::WhisperCppAdapter(const std::string& modelPath, const std::string& inboxPath)
    : m_modelPath(modelPath)
    , m_inboxPath(inboxPath)
{
    // Don't load in constructor to keep it fast/safe. Load on first use or explicit init.
}

WhisperCppAdapter::~WhisperCppAdapter() {
    if (m_ctx) {
        whisper_free(m_ctx);
    }
}

// Helper to ensure model exists or downloads it
bool EnsureModelDownloaded(const std::string& modelPath, std::string& errorMsg) {
    namespace fs = std::filesystem;
    if (fs::exists(modelPath)) return true;

    // Log attempt
    std::cout << "[WhisperCppAdapter] Model not found at: " << modelPath << std::endl;
    std::cout << "[WhisperCppAdapter] Attempting auto-download of ggml-base.bin..." << std::endl;

    // Ensure directory exists
    fs::path modelDir = fs::path(modelPath).parent_path();
    if (!fs::exists(modelDir)) {
        fs::create_directories(modelDir);
    }

    // Use curl to download
    // URL for ggml-base.bin (compatible with whisper.cpp)
    std::string url = "https://huggingface.co/ggerganov/whisper.cpp/resolve/main/ggml-base.bin";
    std::string cmd = "curl -L -o \"" + modelPath + "\" \"" + url + "\"";
    
    int ret = AudioUtils::ExecCmd(cmd);
    if (ret != 0 || !fs::exists(modelPath)) {
        errorMsg = "Falha ao baixar modelo Whisper automaticamente. Verifique conexão ou instale manualmente em: " + modelPath;
        return false;
    }
    std::cout << "[WhisperCppAdapter] Download completed successfully." << std::endl;
    return true;
}



bool WhisperCppAdapter::loadModel(std::string& errorMsg) {
    if (m_modelLoaded) return true;
    
    // Auto-download if needed
    if (!EnsureModelDownloaded(m_modelPath, errorMsg)) {
        return false;
    }

    struct whisper_context_params cparams = whisper_context_default_params();
    m_ctx = whisper_init_from_file_with_params(m_modelPath.c_str(), cparams);

    if (!m_ctx) {
        errorMsg = "Falha ao inicializar contexto whisper do arquivo.";
        return false;
    }

    m_modelLoaded = true;
    return true;
}

void WhisperCppAdapter::transcribeAsync(const std::string& audioPath, OnSuccess onSuccess, OnError onError) {
    if (!std::filesystem::exists(audioPath)) {
        if (onError) onError("Arquivo de áudio não encontrado.");
        return;
    }

    std::thread([this, audioPath, onSuccess, onError]() {
        std::lock_guard<std::mutex> lock(m_mutex);
        
        std::string error;
        if (!loadModel(error)) {
            if (onError) onError(error);
            return;
        }

        namespace fs = std::filesystem;
        
        // Pre-process if needed
        std::string processedPath = audioPath;
        bool usingTempFile = false;
        
        fs::path p(audioPath);
        std::string ext = p.extension().string();
        // Simple lowercasing
        std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);

        if (ext != ".wav") {
             std::string convError;
             if (!AudioUtils::ConvertAudioToWav(audioPath, processedPath, convError)) {
                 if (onError) onError(convError);
                 return;
             }
             usingTempFile = true;
        }

        // Load Audio
        std::vector<float> pcmf32;
        if (!AudioUtils::LoadAudioSDL(processedPath, pcmf32, error)) {
            if (usingTempFile) fs::remove(processedPath);
            if (onError) onError("Erro ao Carregar Áudio: " + error);
            return;
        }

        // Cleanup temp file immediately after loading
        if (usingTempFile) {
            fs::remove(processedPath);
        }

        // Run Inference
        whisper_full_params wparams = whisper_full_default_params(WHISPER_SAMPLING_GREEDY);
        wparams.print_progress = false;
        wparams.print_special = false;
        wparams.print_realtime = false;
        wparams.print_timestamps = false;
        wparams.language = "pt"; // Force Portuguese or keep "auto"
        wparams.n_threads = std::thread::hardware_concurrency(); // Use all available cores

        if (whisper_full(m_ctx, wparams, pcmf32.data(), pcmf32.size()) != 0) {
            if (onError) onError("Inferência Whisper falhou.");
            return;
        }

        // Get Text
        std::string result;
        const int n_segments = whisper_full_n_segments(m_ctx);
        for (int i = 0; i < n_segments; ++i) {
            const char* text = whisper_full_get_segment_text(m_ctx, i);
            result += text;
            result += " "; // Space separator ? or newline?
        }

        // Save to File
        fs::path audioP(audioPath);
        std::string txtName = audioP.stem().string() + "_transcricao.txt";
        fs::path destTxt = fs::path(m_inboxPath) / txtName;

        std::ofstream out(destTxt);
        if (out.is_open()) {
            out << result;
            out.close();
            if (onSuccess) onSuccess(destTxt.string());
        } else {
            if (onError) onError("Falha ao salvar transcrição em: " + destTxt.string());
        }

    }).detach();

}

} // namespace ideawalker::infrastructure
