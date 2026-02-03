/**
 * @file EpistemicValidator.hpp
 * @brief Epistemic validation gate for scientific consumables.
 */

#pragma once

#include <string>
#include <vector>

#include <nlohmann/json.hpp>

namespace ideawalker::application::scientific {

/**
 * @class EpistemicValidator
 * @brief Validates epistemic integrity before STRATA export.
 */
class EpistemicValidator {
public:
    struct Result {
        bool exportAllowed = false;
        nlohmann::json report;
        nlohmann::json seal;
    };

    /**
     * @brief Validate a scientific bundle and produce a report + export seal.
     * @param bundle Scientific bundle JSON.
     * @return Result with report and export permissions.
     */
    Result Validate(const nlohmann::json& bundle) const;

private:
    void AddError(nlohmann::json& report, const std::string& message) const;
    void AddWarning(nlohmann::json& report, const std::string& message) const;
    void SetCheck(nlohmann::json& report, const std::string& key, const std::string& status) const;
};

} // namespace ideawalker::application::scientific
