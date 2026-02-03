/**
 * @file ScientificSchema.hpp
 * @brief Shared schema definitions for scientific ingestion artifacts.
 */

#pragma once

#include <array>

namespace ideawalker::domain::scientific {

/**
 * @class ScientificSchema
 * @brief Defines the stable schema version and allowed categorical values for scientific artifacts.
 */
class ScientificSchema {
public:
    /** @brief Current schema version for scientific consumables. */
    static constexpr int SchemaVersion = 1;

    /** @brief Allowed study types. */
    static constexpr std::array<const char*, 7> StudyTypes = {
        "experimental", "observational", "review", "theoretical", "simulation", "mixed", "unknown"
    };

    /** @brief Allowed temporal scales. */
    static constexpr std::array<const char*, 5> TemporalScales = {
        "short", "medium", "long", "multi", "unknown"
    };

    /** @brief Allowed ecosystem types (broad, extensible). */
    static constexpr std::array<const char*, 9> EcosystemTypes = {
        "terrestrial", "aquatic", "urban", "agro", "industrial", "social", "digital", "mixed", "unknown"
    };

    /** @brief Allowed evidence types. */
    static constexpr std::array<const char*, 4> EvidenceTypes = {
        "empirical", "theoretical", "mixed", "unknown"
    };

    /** @brief Allowed transferability levels. */
    static constexpr std::array<const char*, 5> TransferabilityLevels = {
        "high", "medium", "low", "contextual", "unknown"
    };

    /** @brief Allowed confidence levels for claims. */
    static constexpr std::array<const char*, 4> ConfidenceLevels = {
        "low", "medium", "high", "unknown"
    };

    /** @brief Allowed mechanism status values. */
    static constexpr std::array<const char*, 4> MechanismStatus = {
        "tested", "inferred", "speculative", "unknown"
    };

    /** @brief Allowed baseline types. */
    static constexpr std::array<const char*, 5> BaselineTypes = {
        "fixed", "dynamic", "multiple", "none", "unknown"
    };
};

} // namespace ideawalker::domain::scientific
