/**
 * @file ScientificIngestionService.hpp
 * @brief Service to ingest scientific sources and produce cognitive artifacts.
 */

#pragma once

#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>
#include "domain/AIService.hpp"
#include "domain/SourceArtifact.hpp"
#include "domain/scientific/ScientificSchema.hpp"
#include "infrastructure/FileSystemArtifactScanner.hpp"

namespace ideawalker::application::scientific {

/**
 * @class ScientificIngestionService
 * @brief Orchestrates ingestion of scientific sources and exports STRATA consumables.
 */
class ScientificIngestionService {
public:
    /**
     * @brief Result of a scientific ingestion batch.
     */
    struct IngestionResult {
        int artifactsDetected;
        int bundlesGenerated;
        std::vector<std::string> errors;
    };

    /**
     * @brief Summary of the latest epistemic validation report.
     */
    struct ValidationSummary {
        std::string path;
        std::string status;
        bool exportAllowed = false;
        size_t errorCount = 0;
        size_t warningCount = 0;
        std::string reportJson;
    };

    /**
     * @brief Constructs a scientific ingestion service.
     * @param scanner Artifact scanner for scientific inbox.
     * @param aiService AI service used to generate structured artifacts.
     * @param observationsPath Path where raw bundles are stored.
     * @param consumablesPath Path where STRATA consumables are exported.
     */
    ScientificIngestionService(std::unique_ptr<infrastructure::FileSystemArtifactScanner> scanner,
                               std::shared_ptr<domain::AIService> aiService,
                               const std::string& observationsPath,
                               const std::string& consumablesPath);

    /**
     * @brief Ingests a direct JSON bundle (Candidate) from an external source (e.g. PersonaOrchestrator).
     * @param jsonContent The raw JSON content of the bundle.
     * @param artifactId The unique ID for this artifact/bundle.
     * @return True if ingestion/validation was successful (even if export was blocked), False on critical error.
     */
    bool ingestScientificBundle(const std::string& jsonContent, const std::string& artifactId);

    /**
     * @brief Scans and processes all pending scientific documents.
     * @param statusCallback Optional UI feedback callback.
     * @return Summary of the operation.
     */
    IngestionResult ingestPending(std::function<void(std::string)> statusCallback = nullptr);

    /**
     * @brief Counts stored scientific bundles.
     * @return Number of raw scientific bundle files on disk.
     */
    size_t getBundlesCount() const;

    /**
     * @brief Retrieves the most recent epistemic validation summary, if available.
     * @return Optional summary with report status and location.
     */
    std::optional<ValidationSummary> getLatestValidationSummary() const;

private:
    std::unique_ptr<infrastructure::FileSystemArtifactScanner> m_scanner;
    std::shared_ptr<domain::AIService> m_aiService;
    std::string m_observationsPath;
    std::string m_consumablesPath;

    std::string buildNarrativeSystemPrompt() const;
    std::string buildNarrativeUserPrompt(const domain::SourceArtifact& artifact, const std::string& content) const;
    
    std::string buildDiscursiveSystemPrompt() const;
    std::string buildDiscursiveUserPrompt(const domain::SourceArtifact& artifact, const std::string& content) const;
    std::string buildArtifactId(const domain::SourceArtifact& artifact) const;

    bool validateBundleJson(const nlohmann::json& bundle, std::vector<std::string>& errors) const;
    void attachSourceMetadata(nlohmann::json& bundle,
                              const domain::SourceArtifact& artifact,
                              const std::string& artifactId,
                              const std::string& method) const;
    bool saveRawBundle(const nlohmann::json& bundle, const std::string& artifactId, std::string& error) const;
    bool exportConsumables(const nlohmann::json& bundle, const std::string& artifactId, std::string& error) const;
    bool saveErrorPayload(const std::string& artifactId, const std::string& payload, std::string& error) const;
};

} // namespace ideawalker::application::scientific
