/**
 * @file DocumentIngestionService.hpp
 * @brief Service to manage document ingestion and observation generation.
 */

#pragma once
#include <memory>
#include <vector>
#include <functional>
#include "domain/SourceArtifact.hpp"
#include "domain/ObservationRecord.hpp"
#include "domain/AIService.hpp"
#include "infrastructure/FileSystemArtifactScanner.hpp"
#include <algorithm>

namespace ideawalker::application {

/**
 * @class DocumentIngestionService
 * @brief Orchestrates the ingestion pipeline from scan to observation.
 */
class DocumentIngestionService {
public:
    DocumentIngestionService(std::unique_ptr<infrastructure::FileSystemArtifactScanner> scanner,
                             std::shared_ptr<domain::AIService> aiService,
                             const std::string& observationsPath);

    /**
     * @brief Result of an ingestion process.
     */
    struct IngestionResult {
        int artifactsDetected;
        int observationsGenerated;
        std::vector<std::string> errors;
    };

    /**
     * @brief Scans and processes all pending documents in the inbox.
     * @param statusCallback UI feedback.
     * @return summary of the operation.
     */
    IngestionResult ingestPending(std::function<void(std::string)> statusCallback = nullptr);

    /** @brief Retrieves all previously generated observations. */
    std::vector<domain::ObservationRecord> getObservations() const;

private:
    std::unique_ptr<infrastructure::FileSystemArtifactScanner> m_scanner;
    std::shared_ptr<domain::AIService> m_aiService;
    std::string m_observationsPath;

    std::string generateObservationPrompt(const domain::SourceArtifact& artifact, const std::string& content);
    void saveObservation(const domain::ObservationRecord& record);
};

} // namespace ideawalker::application
