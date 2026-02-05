/**
 * @file ContentExtractor.hpp
 * @brief Utility for extracting text from different file formats (PDF, Markdown, LaTeX).
 */

#pragma once
#include <iostream>
#include <string>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <cstdio>
#include <vector>
#include <functional>
#include <cctype>
#include <cstdlib>
#include <chrono>

namespace ideawalker::infrastructure {

class ContentExtractor {
public:
    struct ExtractionResult {
        std::string content;
        bool success = false;
        std::string method; // "pdftotext", "ocrmypdf", "tesseract", "text-read"
        std::vector<std::string> warnings;
    };

    static ExtractionResult Extract(const std::string& path, std::function<void(std::string)> statusCallback = nullptr) {
        std::filesystem::path p(path);
        std::string ext = p.extension().string();
        std::transform(ext.begin(), ext.end(), ext.begin(), [](unsigned char c){ return std::tolower(c); });

        if (ext == ".pdf") {
            return ExtractPdf(path, statusCallback);
        } else {
            return ExtractText(path);
        }
    }

private:
    static std::string RunCommand(const std::string& cmd) {
        std::string output;
        FILE* pipe = popen(cmd.c_str(), "r");
        if (!pipe) return output;
        char buffer[256];
        while (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
            output.append(buffer);
        }
        pclose(pipe);
        return output;
    }

    static bool HasTool(const std::string& tool) {
        std::string cmd = "command -v " + tool + " >/dev/null 2>&1";
        int result = std::system(cmd.c_str());
        return result == 0;
    }

    static std::string GetTempFilePath(const std::string& suffix) {
        auto now = std::chrono::high_resolution_clock::now().time_since_epoch().count();
        std::string name = "ideawalker_" + std::to_string(now) + suffix;
        return (std::filesystem::temp_directory_path() / name).string();
    }

    static bool IsValidContent(const std::string& content) {
        size_t nonWhitespace = 0;
        for (char c : content) {
            if (!std::isspace(static_cast<unsigned char>(c))) {
                ++nonWhitespace;
                if (nonWhitespace >= 10) return true;
            }
        }
        return false;
    }

    static ExtractionResult ExtractPdf(const std::string& path, std::function<void(std::string)> statusCallback) {
        ExtractionResult result;
        
        // Tier 1: Try standard extraction (pdftotext)
        std::string content = RunCommand("pdftotext \"" + path + "\" - 2>/dev/null");

        if (IsValidContent(content)) {
            result.content = content;
            result.success = true;
            result.method = "pdftotext";
            return result;
        }

        // Tier 2: Hybrid Pipeline (ocrmypdf)
        if (HasTool("ocrmypdf")) {
            if (statusCallback) statusCallback("[OCR] Detectado PDF de imagem. Iniciando leitura visual (CPU)...");
            
            // Persistent OCR storage
            std::filesystem::path inputPath(path);
            std::filesystem::path ocrDir = inputPath.parent_path() / ".ocr";
            if (!std::filesystem::exists(ocrDir)) {
                std::filesystem::create_directories(ocrDir);
            }
            std::filesystem::path ocrPath = ocrDir / (inputPath.stem().string() + "_ocr.pdf");
            std::string tempPdf = ocrPath.string();

            // Check if already exists to skip re-processing (simple cache)
            if (std::filesystem::exists(ocrPath)) {
                 if (statusCallback) statusCallback("[OCR] Usando versão em cache (.ocr/)...");
                 std::string ocrContent = RunCommand("pdftotext \"" + tempPdf + "\" - 2>/dev/null");
                 if (IsValidContent(ocrContent)) {
                     result.content = ocrContent;
                     result.success = true;
                     result.method = "ocr-cache";
                     return result;
                 }
            }

            // Minimal flags as per user success
            // --jobs 4: safe parallelism
            // 2>&1: required for progress capture
            std::string cmd = "ocrmypdf --jobs 4 --output-type pdf \"" + path + "\" \"" + tempPdf + "\" 2>&1";
            std::cout << "[DEBUG] Running OCR command: " << cmd << std::endl;
            
            // Run with progress capture
            bool ocrSuccess = RunCommandWithCallback(cmd, [&statusCallback](const std::string& line) {
                if (statusCallback) {
                    // Filter for interesting lines logic
                    if (line.find("Page") != std::string::npos || line.find("Scanning") != std::string::npos || line.find("Optimizing") != std::string::npos) {
                         statusCallback("[OCR] " + line);
                    }
                }
            });
            
            if (ocrSuccess) {
                 std::string ocrContent = RunCommand("pdftotext \"" + tempPdf + "\" - 2>/dev/null");
                 // std::filesystem::remove(tempPdf); // KEEP FILE PERSISTENT
                 
                 if (IsValidContent(ocrContent)) {
                     result.content = ocrContent;
                     result.success = true;
                     result.method = "ocr-hybrid (ocrmypdf)";
                     result.warnings.push_back("Content extracted via OCR hybrid pipeline. Formatting preserved but errors possible.");
                     return result;
                 }
            } else {
                 std::filesystem::remove(tempPdf);
                 result.warnings.push_back("ocrmypdf failed to process the file.");
            }
        }

        // Tier 3: Last Resort (Raw Tesseract)
        if (HasTool("tesseract")) {
             if (statusCallback) statusCallback("[OCR] Tentando fallback para Tesseract (raw)...");
             std::string cmd = "tesseract \"" + path + "\" stdout 2>/dev/null";
             std::string tesseractContent = RunCommand(cmd);
             if (IsValidContent(tesseractContent)) {
                 result.content = tesseractContent;
                 result.success = true;
                 result.method = "ocr-raw (tesseract)";
                 result.warnings.push_back("Content extracted via raw OCR. Layout lost, high error rate possible.");
                 return result;
             }
        }

        // Failure
        result.success = false;
        result.method = "failed";
        return result;
    }

    // Runs command and streams output to callback. Returns true if exit code 0.
    static bool RunCommandWithCallback(const std::string& cmd, std::function<void(std::string)> lineCallback) {
        FILE* pipe = popen(cmd.c_str(), "r");
        if (!pipe) {
            if (lineCallback) lineCallback("[Error] popen failed to start command.");
            return false;
        }
        char buffer[256];
        while (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
            std::string line(buffer);
            // Trim newline
            if (!line.empty() && line.back() == '\n') line.pop_back();
            if (lineCallback) lineCallback(line);
        }
        int returnCode = pclose(pipe);
        if (returnCode != 0) {
             if (lineCallback) lineCallback("[Error] Command exited with code: " + std::to_string(returnCode));
        }
        return (returnCode == 0);
    }

    static ExtractionResult ExtractText(const std::string& path) {
        ExtractionResult result;
        std::ifstream file(path);
        if (!file.is_open()) {
            result.success = false;
            result.warnings.push_back("Could not open file.");
            return result;
        }
        std::stringstream buffer;
        buffer << file.rdbuf();
        result.content = buffer.str();
        result.success = true;
        result.method = "text-read";
        return result;
    }
};


} // namespace ideawalker::infrastructure
