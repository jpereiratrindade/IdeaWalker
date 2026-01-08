/**
 * @file FileSystemArtifactScanner.hpp
 * @brief Scanner for detecting documents in the inbox.
 */

#pragma once
#include <vector>
#include <string>
#include "domain/SourceArtifact.hpp"

namespace ideawalker::infrastructure {

/**
 * @class FileSystemArtifactScanner
 * @brief Infrastructure adapter to scan the project's inbox directory.
 */
class FileSystemArtifactScanner {
public:
    explicit FileSystemArtifactScanner(const std::string& inboxPath);

    /**
     * @brief Scans for new or modified artifacts.
     * @return List of SourceArtifacts found.
     */
    std::vector<domain::SourceArtifact> scan();

private:
    std::string m_inboxPath;
    
    domain::SourceType classifyByExtension(const std::string& extension) const;
    std::string calculateHash(const std::string& filePath) const;
};

} // namespace ideawalker::infrastructure
