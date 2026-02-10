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
#include <array>
#include <cstdio>
#include <cstdint>
#include <cstring>
#include <vector>
#include <functional>
#include <cctype>
#include <cstdlib>
#include <chrono>
#include <iomanip>
#include <unordered_map>
#include <sstream>

namespace ideawalker::infrastructure {
    
namespace {
    // Normalizes line for structural comparison (ignore digits, trim, lower)
    std::string NormalizeStructuralLine(const std::string& line) {
        std::string out;
        out.reserve(line.size());
        for (char c : line) {
            if (std::isspace(static_cast<unsigned char>(c))) continue;
            if (std::isdigit(static_cast<unsigned char>(c))) {
                out.push_back('#'); // Mask digits
            } else {
                out.push_back(std::tolower(static_cast<unsigned char>(c)));
            }
        }
        return out;
    }
}

class ContentExtractor {
public:
    struct ExtractionResult {
        std::string content;
        bool success = false;
        std::string method; // "pdftotext", "ocrmypdf", "tesseract", "text-read"
        std::vector<std::string> warnings;
        std::string sourceSha256;
    };

    static ExtractionResult Extract(const std::string& path, std::function<void(std::string)> statusCallback = nullptr) {
        std::filesystem::path p(path);
        std::string ext = p.extension().string();
        std::transform(ext.begin(), ext.end(), ext.begin(), [](unsigned char c){ return std::tolower(c); });
        std::string sha256 = ComputeFileSha256(path);

        if (ext == ".pdf") {
            return ExtractPdf(path, sha256, statusCallback);
        } else {
            return ExtractText(path, sha256);
        }
    }

private:
    class Sha256 {
    public:
        Sha256() { Init(); }

        void Update(const unsigned char* data, size_t len) {
            for (size_t i = 0; i < len; ++i) {
                m_data[m_datalen] = data[i];
                m_datalen++;
                if (m_datalen == 64) {
                    Transform();
                    m_bitlen += 512;
                    m_datalen = 0;
                }
            }
        }

        void Final(unsigned char hash[32]) {
            size_t i = m_datalen;

            if (m_datalen < 56) {
                m_data[i++] = 0x80;
                while (i < 56) m_data[i++] = 0x00;
            } else {
                m_data[i++] = 0x80;
                while (i < 64) m_data[i++] = 0x00;
                Transform();
                std::memset(m_data, 0, 56);
            }

            m_bitlen += m_datalen * 8;
            m_data[63] = static_cast<unsigned char>(m_bitlen);
            m_data[62] = static_cast<unsigned char>(m_bitlen >> 8);
            m_data[61] = static_cast<unsigned char>(m_bitlen >> 16);
            m_data[60] = static_cast<unsigned char>(m_bitlen >> 24);
            m_data[59] = static_cast<unsigned char>(m_bitlen >> 32);
            m_data[58] = static_cast<unsigned char>(m_bitlen >> 40);
            m_data[57] = static_cast<unsigned char>(m_bitlen >> 48);
            m_data[56] = static_cast<unsigned char>(m_bitlen >> 56);
            Transform();

            for (i = 0; i < 4; ++i) {
                hash[i]      = (m_state[0] >> (24 - i * 8)) & 0x000000ff;
                hash[i + 4]  = (m_state[1] >> (24 - i * 8)) & 0x000000ff;
                hash[i + 8]  = (m_state[2] >> (24 - i * 8)) & 0x000000ff;
                hash[i + 12] = (m_state[3] >> (24 - i * 8)) & 0x000000ff;
                hash[i + 16] = (m_state[4] >> (24 - i * 8)) & 0x000000ff;
                hash[i + 20] = (m_state[5] >> (24 - i * 8)) & 0x000000ff;
                hash[i + 24] = (m_state[6] >> (24 - i * 8)) & 0x000000ff;
                hash[i + 28] = (m_state[7] >> (24 - i * 8)) & 0x000000ff;
            }
        }

    private:
        uint32_t m_state[8];
        uint64_t m_bitlen = 0;
        unsigned char m_data[64];
        size_t m_datalen = 0;

        static constexpr std::array<uint32_t, 64> k = {
            0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5,
            0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3, 0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174,
            0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc, 0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
            0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7, 0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967,
            0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13, 0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
            0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3, 0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
            0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
            0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208, 0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2
        };

        static uint32_t RotateRight(uint32_t x, uint32_t n) {
            return (x >> n) | (x << (32 - n));
        }

        static uint32_t Choose(uint32_t e, uint32_t f, uint32_t g) {
            return (e & f) ^ (~e & g);
        }

        static uint32_t Majority(uint32_t a, uint32_t b, uint32_t c) {
            return (a & b) ^ (a & c) ^ (b & c);
        }

        static uint32_t Sig0(uint32_t x) {
            return RotateRight(x, 2) ^ RotateRight(x, 13) ^ RotateRight(x, 22);
        }

        static uint32_t Sig1(uint32_t x) {
            return RotateRight(x, 6) ^ RotateRight(x, 11) ^ RotateRight(x, 25);
        }

        static uint32_t Theta0(uint32_t x) {
            return RotateRight(x, 7) ^ RotateRight(x, 18) ^ (x >> 3);
        }

        static uint32_t Theta1(uint32_t x) {
            return RotateRight(x, 17) ^ RotateRight(x, 19) ^ (x >> 10);
        }

        void Init() {
            m_state[0] = 0x6a09e667;
            m_state[1] = 0xbb67ae85;
            m_state[2] = 0x3c6ef372;
            m_state[3] = 0xa54ff53a;
            m_state[4] = 0x510e527f;
            m_state[5] = 0x9b05688c;
            m_state[6] = 0x1f83d9ab;
            m_state[7] = 0x5be0cd19;
        }

        void Transform() {
            uint32_t m[64];
            for (uint32_t i = 0, j = 0; i < 16; ++i, j += 4) {
                m[i] = (static_cast<uint32_t>(m_data[j]) << 24) |
                       (static_cast<uint32_t>(m_data[j + 1]) << 16) |
                       (static_cast<uint32_t>(m_data[j + 2]) << 8) |
                        static_cast<uint32_t>(m_data[j + 3]);
            }
            for (uint32_t i = 16; i < 64; ++i) {
                m[i] = Theta1(m[i - 2]) + m[i - 7] + Theta0(m[i - 15]) + m[i - 16];
            }

            uint32_t a = m_state[0];
            uint32_t b = m_state[1];
            uint32_t c = m_state[2];
            uint32_t d = m_state[3];
            uint32_t e = m_state[4];
            uint32_t f = m_state[5];
            uint32_t g = m_state[6];
            uint32_t h = m_state[7];

            for (uint32_t i = 0; i < 64; ++i) {
                uint32_t t1 = h + Sig1(e) + Choose(e, f, g) + k[i] + m[i];
                uint32_t t2 = Sig0(a) + Majority(a, b, c);
                h = g;
                g = f;
                f = e;
                e = d + t1;
                d = c;
                c = b;
                b = a;
                a = t1 + t2;
            }

            m_state[0] += a;
            m_state[1] += b;
            m_state[2] += c;
            m_state[3] += d;
            m_state[4] += e;
            m_state[5] += f;
            m_state[6] += g;
            m_state[7] += h;
        }
    };

    static std::string ComputeFileSha256(const std::string& path) {
        std::ifstream file(path, std::ios::binary);
        if (!file.is_open()) return "";
        Sha256 sha;
        std::array<char, 8192> buffer{};
        while (file.good()) {
            file.read(buffer.data(), buffer.size());
            std::streamsize count = file.gcount();
            if (count > 0) {
                sha.Update(reinterpret_cast<const unsigned char*>(buffer.data()), static_cast<size_t>(count));
            }
        }
        unsigned char hash[32];
        sha.Final(hash);
        std::ostringstream oss;
        oss << std::hex << std::setfill('0');
        for (unsigned char c : hash) {
            oss << std::setw(2) << static_cast<int>(c);
        }
        return oss.str();
    }

    static std::string NowIso() {
        auto now = std::chrono::system_clock::now();
        std::time_t tt = std::chrono::system_clock::to_time_t(now);
        std::tm tm = {};
#if defined(_WIN32)
        gmtime_s(&tm, &tt);
#else
        gmtime_r(&tt, &tm);
#endif
        std::ostringstream oss;
        oss << std::put_time(&tm, "%Y-%m-%dT%H:%M:%SZ");
        return oss.str();
    }

    static std::filesystem::path InferProjectRoot(const std::filesystem::path& filePath) {
        std::filesystem::path cur = filePath.parent_path();
        for (int i = 0; i < 6 && !cur.empty(); ++i) {
            if (std::filesystem::exists(cur / "inbox") &&
                std::filesystem::exists(cur / "observations")) {
                return cur;
            }
            cur = cur.parent_path();
        }
        return filePath.parent_path();
    }

    static std::filesystem::path GetTextCacheDir(const std::filesystem::path& filePath) {
        std::filesystem::path root = InferProjectRoot(filePath);
        return root / ".iwcache" / "text";
    }

    static bool TryLoadTextCache(const std::filesystem::path& filePath,
                                 const std::string& sha256,
                                 ExtractionResult& result,
                                 std::function<void(std::string)> statusCallback) {
        if (sha256.empty()) return false;
        std::filesystem::path cacheDir = GetTextCacheDir(filePath);
        std::filesystem::path cacheTxt = cacheDir / (sha256 + ".txt");
        if (!std::filesystem::exists(cacheTxt)) return false;
        std::ifstream file(cacheTxt);
        if (!file.is_open()) return false;
        std::stringstream buffer;
        buffer << file.rdbuf();
        const std::string content = buffer.str();
        if (!IsValidContent(content)) return false;
        result.content = content;
        result.success = true;
        result.method = "text-cache";
        result.sourceSha256 = sha256;
        if (statusCallback) statusCallback("[CACHE] Usando texto extraído previamente (SHA-256).");
        return true;
    }

    static void SaveTextCache(const std::filesystem::path& filePath,
                              const std::string& sha256,
                              const std::string& content,
                              const std::string& method) {
        if (sha256.empty()) return;
        std::filesystem::path cacheDir = GetTextCacheDir(filePath);
        if (!std::filesystem::exists(cacheDir)) {
            std::filesystem::create_directories(cacheDir);
        }
        std::filesystem::path cacheTxt = cacheDir / (sha256 + ".txt");
        if (!std::filesystem::exists(cacheTxt)) {
            std::ofstream out(cacheTxt);
            if (out.is_open()) {
                out << content;
            }
        }
        std::filesystem::path metaPath = cacheDir / (sha256 + ".meta.json");
        if (!std::filesystem::exists(metaPath)) {
            std::ofstream meta(metaPath);
            if (meta.is_open()) {
                meta << "{\n"
                     << "  \"sha256\": \"" << sha256 << "\",\n"
                     << "  \"sourcePath\": \"" << filePath.string() << "\",\n"
                     << "  \"method\": \"" << method << "\",\n"
                     << "  \"extractedAt\": \"" << NowIso() << "\"\n"
                     << "}\n";
            }
        }
    }

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

    static ExtractionResult ExtractPdf(const std::string& path,
                                       const std::string& sha256,
                                       std::function<void(std::string)> statusCallback) {
        ExtractionResult result;
        result.sourceSha256 = sha256;

        if (TryLoadTextCache(path, sha256, result, statusCallback)) {
            return result;
        }
        
        // Tier 1: Try standard extraction (pdftotext)
        std::string rawContent = RunCommand("pdftotext \"" + path + "\" - 2>/dev/null");

        if (IsValidContent(rawContent)) {
             // Apply Structural Exclusion Rule
             std::vector<std::string> pages;
             std::string currentPage;
             for (char c : rawContent) {
                 if (c == '\f') { // Form Feed
                     pages.push_back(currentPage);
                     currentPage.clear();
                 } else {
                     currentPage.push_back(c);
                 }
             }
             if (!currentPage.empty()) pages.push_back(currentPage);

             // Build Frequency Map for Structural Zones (Top 3 / Bottom 3 lines)
             std::unordered_map<std::string, int> structuralFreq;
             int totalPages = pages.size();
             int structuralThreshold = std::max(2, static_cast<int>(totalPages * 0.6)); // 60% recurrence
             
             // First pass: Count
             for (const auto& pageStr : pages) {
                 std::stringstream ss(pageStr);
                 std::string line;
                 std::vector<std::string> lines;
                 while (std::getline(ss, line)) {
                     if (!line.empty()) lines.push_back(line);
                 }
                 if (lines.empty()) continue;

                 // Check Top 3
                 for (size_t i = 0; i < std::min((size_t)3, lines.size()); ++i) {
                     if (lines[i].length() <= 160) {
                        structuralFreq[NormalizeStructuralLine(lines[i])]++;
                     }
                 }
                 // Check Bottom 3
                 if (lines.size() > 3) {
                     for (size_t i = lines.size() - std::min((size_t)3, lines.size()); i < lines.size(); ++i) {
                         if (lines[i].length() <= 160) {
                            structuralFreq[NormalizeStructuralLine(lines[i])]++;
                         }
                     }
                 }
             }

             // Second pass: Filter and Reassemble
             std::ostringstream finalContent;
             for (const auto& pageStr : pages) {
                 std::stringstream ss(pageStr);
                 std::string line;
                 while (std::getline(ss, line)) {
                     std::string norm = NormalizeStructuralLine(line);
                     // If line is short, in freq map, and high frequency -> SKIP
                     if (line.length() <= 160 && structuralFreq.count(norm) && structuralFreq[norm] >= structuralThreshold) {
                         continue; // Structural Exclusion
                     }
                     finalContent << line << "\n";
                 }
                 finalContent << "\n"; // Preserve page separation loosely
             }

            result.content = finalContent.str();
            result.success = true;
            result.method = "pdftotext (filtered)";
            SaveTextCache(path, sha256, result.content, result.method);
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
                     SaveTextCache(path, sha256, result.content, result.method);
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
                     SaveTextCache(path, sha256, result.content, result.method);
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
                 SaveTextCache(path, sha256, result.content, result.method);
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

    static ExtractionResult ExtractText(const std::string& path, const std::string& sha256) {
        ExtractionResult result;
        result.sourceSha256 = sha256;
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
