#include "infrastructure/AudioUtils.hpp"
#include <SDL.h>
#include <filesystem>
#include <iostream>
#include <algorithm>

namespace ideawalker::infrastructure {

int AudioUtils::ExecCmd(const std::string& cmd) {
    return std::system(cmd.c_str());
}

bool AudioUtils::ConvertAudioToWav(const std::string& inputPath, std::string& outputPath, std::string& error) {
    namespace fs = std::filesystem;
    fs::path tempPath = fs::temp_directory_path() / (fs::path(inputPath).stem().string() + "_temp.wav");
    outputPath = tempPath.string();

    if (fs::exists(tempPath)) {
        fs::remove(tempPath);
    }

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

bool AudioUtils::LoadAudioSDL(const std::string& fname, std::vector<float>& pcmf32, std::string& error) {
    SDL_AudioSpec wavSpec;
    Uint32 wavLength;
    Uint8 *wavBuffer;

    if (SDL_LoadWAV(fname.c_str(), &wavSpec, &wavBuffer, &wavLength) == NULL) {
        error = "Falha em SDL_LoadWAV: " + std::string(SDL_GetError());
        return false;
    }

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
    cvt.buf = (Uint8 *)SDL_malloc(cvt.len * cvt.len_mult);
    SDL_memcpy(cvt.buf, wavBuffer, wavLength);
    
    if (SDL_ConvertAudio(&cvt) < 0) {
        error = "Falha em SDL_ConvertAudio: " + std::string(SDL_GetError());
        SDL_free(cvt.buf);
        SDL_FreeWAV(wavBuffer);
        return false;
    }

    int sampleCount = cvt.len_cvt / sizeof(float);
    pcmf32.resize(sampleCount);
    SDL_memcpy(pcmf32.data(), cvt.buf, cvt.len_cvt);

    SDL_free(cvt.buf);
    SDL_FreeWAV(wavBuffer);
    
    return true;
}

} // namespace ideawalker::infrastructure
