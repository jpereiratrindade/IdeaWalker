/**
 * @file SourceArtifact.hpp
 * @brief Domain entity representing a document in the ingestion pipeline.
 */

#pragma once
#include <string>
#include <chrono>

namespace ideawalker::domain {

/**
 * @enum SourceType
 * @brief Categorization of source documents.
 */
enum class SourceType {
    PlainText,
    Markdown,
    PDF,
    LaTeX,
    Unknown
};

/**
 * @class SourceArtifact
 * @brief Entity representing a physical file detected in the inbox.
 */
class SourceArtifact {
public:
    std::string path;              ///< Path relative to project root.
    std::string filename;          ///< Basename of the file.
    SourceType type;               ///< Classified type.
    std::string contentHash;       ///< To detect changes.
    std::chrono::system_clock::time_point lastModified;
    long long sizeBytes;

    SourceArtifact() : type(SourceType::Unknown), sizeBytes(0) {}
};

} // namespace ideawalker::domain
