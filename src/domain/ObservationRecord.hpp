/**
 * @file ObservationRecord.hpp
 * @brief Domain entity for observations generated from documents.
 */

#pragma once
#include <string>
#include <chrono>

namespace ideawalker::domain {

/**
 * @class ObservationRecord
 * @brief Represents an AI-generated observation from an inbox artifact.
 */
class ObservationRecord {
public:
    std::string id;                ///< Derived from ArtifactId + timestamp.
    std::string sourcePath;        ///< Original file path.
    std::string sourceHash;        ///< For traceability.
    std::string content;           ///< The observation text.
    std::chrono::system_clock::time_point createdAt;

    ObservationRecord() = default;
};

} // namespace ideawalker::domain
