/**
 * @file FileSystemArtifactScanner.cpp
 * @brief Implementation of the FileSystemArtifactScanner.
 */

#include "infrastructure/FileSystemArtifactScanner.hpp"
#include <algorithm>
#include <filesystem>
#include <sstream>

namespace fs = std::filesystem;

namespace ideawalker::infrastructure {

FileSystemArtifactScanner::FileSystemArtifactScanner(const std::string& inboxPath)
    : m_inboxPath(inboxPath) {}

std::vector<domain::SourceArtifact> FileSystemArtifactScanner::scan() {
    std::vector<domain::SourceArtifact> artifacts;
    
    if (!fs::exists(m_inboxPath)) {
        return artifacts;
    }

    for (const auto& entry : fs::directory_iterator(m_inboxPath)) {
        if (entry.is_regular_file()) {
            std::string ext = entry.path().extension().string();
            // Convert extension to lowercase for robust check
            std::transform(ext.begin(), ext.end(), ext.begin(), [](unsigned char c){ return std::tolower(c); });

            if (ext != ".txt" && ext != ".md" && ext != ".pdf" && ext != ".tex") {
                continue;
            }

            domain::SourceArtifact artifact;
            artifact.path = entry.path().string();
            artifact.filename = entry.path().filename().string();
            artifact.type = classifyByExtension(entry.path().extension().string());
            
            // Phase 1: simple "hash" based on last modified time for change detection
            auto ftime = fs::last_write_time(entry);
            artifact.lastModified = std::chrono::time_point_cast<std::chrono::system_clock::duration>(
                ftime - fs::file_time_type::clock::now() + std::chrono::system_clock::now()
            );
            
            artifact.sizeBytes = fs::file_size(entry);
            
            std::stringstream ss;
            ss << artifact.sizeBytes << "_" << artifact.lastModified.time_since_epoch().count();
            artifact.contentHash = ss.str();

            artifacts.push_back(artifact);
        }
    }

    return artifacts;
}

domain::SourceType FileSystemArtifactScanner::classifyByExtension(const std::string& extension) const {
    if (extension == ".txt") return domain::SourceType::PlainText;
    if (extension == ".md") return domain::SourceType::Markdown;
    if (extension == ".pdf") return domain::SourceType::PDF;
    if (extension == ".tex") return domain::SourceType::LaTeX;
    return domain::SourceType::Unknown;
}

} // namespace ideawalker::infrastructure
