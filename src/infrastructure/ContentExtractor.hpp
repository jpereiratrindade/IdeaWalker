/**
 * @file ContentExtractor.hpp
 * @brief Utility for extracting text from different file formats (PDF, Markdown, LaTeX).
 */

#pragma once
#include <string>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <cstdio>

namespace ideawalker::infrastructure {

class ContentExtractor {
public:
    static std::string Extract(const std::string& path) {
        std::filesystem::path p(path);
        std::string ext = p.extension().string();
        std::transform(ext.begin(), ext.end(), ext.begin(), [](unsigned char c){ return std::tolower(c); });

        if (ext == ".pdf") {
            return ExtractPdf(path);
        } else {
            return ExtractText(path);
        }
    }

private:
    static std::string ExtractPdf(const std::string& path) {
        // Try to use pdftotext (Poppler) via shell
        std::string cmd = "pdftotext \"" + path + "\" - 2>/dev/null";
        FILE* pipe = popen(cmd.c_str(), "r");
        if (!pipe) return "[ERRO: Falha ao abrir pipe para pdftotext]";

        char buffer[128];
        std::stringstream ss;
        while (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
            ss << buffer;
        }
        int status = pclose(pipe);
        if (status == 0) {
            std::string content = ss.str();
            if (content.empty()) return "[CONTEÚDO VAZIO: pdftotext não retornou conteúdo. O PDF pode ser baseado em imagem.]";
            return content;
        }
        return "[CONTEÚDO BINÁRIO: pdftotext falhou ou não está instalado. Por favor, instale o poppler-utils.]";
    }

    static std::string ExtractText(const std::string& path) {
        std::ifstream file(path);
        if (!file.is_open()) return "[ERRO: Não foi possível abrir o arquivo]";
        std::stringstream buffer;
        buffer << file.rdbuf();
        return buffer.str();
    }
};

} // namespace ideawalker::infrastructure
