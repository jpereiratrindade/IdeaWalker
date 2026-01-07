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

namespace ideawalker::infrastructure {

namespace {

// Helper to execute system command
int ExecCmd(const std::string& cmd) {
    return std::system(cmd.c_str());
}

// Convert audio to 16kHz mono WAV using ffmpeg
bool ConvertAudioToWav(const std::string& inputPath, std::string& outputPath, std::string& error) {
    namespace fs = std::filesystem;
    fs::path tempPath = fs::temp_directory_path() / (fs::path(inputPath).stem().string() + "_temp.wav");
    outputPath = tempPath.string();

    // Remove if exists
    if (fs::exists(tempPath)) {
        fs::remove(tempPath);
    }

    // ffmpeg -i input -ar 16000 -ac 1 -c:a pcm_s16le output
    // -y to overwrite, -loglevel error to reduce noise
    std::string cmd = "ffmpeg -y -loglevel error -i \"" + inputPath + "\" -ar 16000 -ac 1 -c:a pcm_s16le \"" + outputPath + "\"";
    
    int ret = ExecCmd(cmd);
    if (ret != 0) {
        error = "Falha ao converter áudio com ffmpeg. Verifique se o ffmpeg está instalado.";
        return false;
    }
    
    if (!fs::exists(outputPath)) {
        error = "Arquivo convertido não encontrado: " + outputPath;
        return false;
    }

    return true;
}

// Helper to load WAV using SDL2 and convert to 16kHz float32 mono
bool LoadAudioSDL(const std::string& fname, std::vector<float>& pcmf32, std::string& error) {
    SDL_AudioSpec wavSpec;
    Uint32 wavLength;
    Uint8 *wavBuffer;

    if (SDL_LoadWAV(fname.c_str(), &wavSpec, &wavBuffer, &wavLength) == NULL) {
        error = "Falha em SDL_LoadWAV: " + std::string(SDL_GetError());
        return false;
    }

    // Target format: 16000 Hz, Float32, Mono
    SDL_AudioSpec targetSpec;
    SDL_zero(targetSpec);
    targetSpec.freq = 16000;
    targetSpec.format = AUDIO_F32SYS;
    targetSpec.channels = 1;

    SDL_AudioCVT cvt;
    if (SDL_BuildAudioCVT(&cvt, wavSpec.format, wavSpec.channels, wavSpec.freq,
                          targetSpec.format, targetSpec.channels, targetSpec.freq) < 0) {
        error = "Falha em SDL_BuildAudioCVT: " + std::string(SDL_GetError());
        SDL_FreeWAV(wavBuffer);
        return false;
    }

    cvt.len = wavLength;
    // cvt.len_mult is the multiplier for buffer size
    // We need to allocate enough space given the conversion
    // For safety, let's use a large buffer allocation or let SDL calculate
    // SDL requires cvt.buf to be allocated by caller with length len * len_mult
    cvt.buf = (Uint8 *)SDL_malloc(cvt.len * cvt.len_mult);
    SDL_memcpy(cvt.buf, wavBuffer, wavLength);
    
    if (SDL_ConvertAudio(&cvt) < 0) {
        error = "Falha em SDL_ConvertAudio: " + std::string(SDL_GetError());
        SDL_free(cvt.buf);
        SDL_FreeWAV(wavBuffer);
        return false;
    }

    // Now cvt.buf has the data in float32 mono 16kHz
    // The length in bytes is cvt.len_cvt
    int sampleCount = cvt.len_cvt / sizeof(float);
    pcmf32.resize(sampleCount);
    SDL_memcpy(pcmf32.data(), cvt.buf, cvt.len_cvt);

    SDL_free(cvt.buf);
    SDL_FreeWAV(wavBuffer);
    
    return true;
}

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

bool WhisperCppAdapter::loadModel(std::string& errorMsg) {
    if (m_modelLoaded) return true;
    
    if (!std::filesystem::exists(m_modelPath)) {
        errorMsg = "Arquivo de modelo não encontrado em: " + m_modelPath + ". Por favor baixe um modelo ggml (ex: ggml-base.bin).";
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
             if (!ConvertAudioToWav(audioPath, processedPath, convError)) {
                 if (onError) onError(convError);
                 return;
             }
             usingTempFile = true;
        }

        // Load Audio
        std::vector<float> pcmf32;
        if (!LoadAudioSDL(processedPath, pcmf32, error)) {
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
