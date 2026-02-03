/**
 * @file EpistemicValidator.cpp
 * @brief Implementation of epistemic validation gate.
 */

#include "application/scientific/EpistemicValidator.hpp"

#include <algorithm>
#include <cctype>
#include <sstream>

namespace ideawalker::application::scientific {

namespace {

std::string Normalize(const std::string& input) {
    std::string out;
    out.reserve(input.size());
    for (unsigned char c : input) {
        out.push_back(static_cast<char>(std::tolower(c)));
    }
    return out;
}

bool ContainsAny(const std::string& haystack, const std::vector<std::string>& needles) {
    const std::string norm = Normalize(haystack);
    for (const auto& needle : needles) {
        if (norm.find(needle) != std::string::npos) return true;
    }
    return false;
}

bool IsNonEmptyString(const nlohmann::json& j, const char* key) {
    return j.contains(key) && j[key].is_string() && !j[key].get<std::string>().empty();
}

bool IsUnknownOrEmpty(const nlohmann::json& j, const char* key) {
    if (!j.contains(key) || !j[key].is_string()) return true;
    std::string value = j[key].get<std::string>();
    if (value.empty()) return true;
    std::string lowered = Normalize(value);
    return lowered == "unknown";
}

bool ArrayHasContent(const nlohmann::json& j, const char* key) {
    return j.contains(key) && j[key].is_array() && !j[key].empty();
}

std::string DetectTemporalVagueness(const std::string& timeWindow) {
    const std::string norm = Normalize(timeWindow);
    const bool hasDigits = std::any_of(timeWindow.begin(), timeWindow.end(), [](unsigned char c){ return std::isdigit(c); });
    if (!hasDigits && (norm.find("decade") != std::string::npos || norm.find("long") != std::string::npos ||
                       norm.find("years") != std::string::npos || norm.find("anos") != std::string::npos)) {
        return "timeWindow vago (sem números)";
    }
    return {};
}

} // namespace

void EpistemicValidator::AddError(nlohmann::json& report, const std::string& message) const {
    report["errors"].push_back(message);
}

void EpistemicValidator::AddWarning(nlohmann::json& report, const std::string& message) const {
    report["warnings"].push_back(message);
}

void EpistemicValidator::SetCheck(nlohmann::json& report, const std::string& key, const std::string& status) const {
    report["checks"][key] = status;
}

EpistemicValidator::Result EpistemicValidator::Validate(const nlohmann::json& bundle) const {
    Result result;
    nlohmann::json report = {
        {"status", "pass"},
        {"errors", nlohmann::json::array()},
        {"warnings", nlohmann::json::array()},
        {"checks", nlohmann::json::object()}
    };

    // A) Contextuality check
    bool contextOk = true;
    if (bundle.contains("narrativeObservations") && bundle["narrativeObservations"].is_array()) {
        for (const auto& obs : bundle["narrativeObservations"]) {
            if (IsUnknownOrEmpty(obs, "contextuality")) {
                contextOk = false;
                AddError(report, "NarrativeObservation sem contextuality.");
                break;
            }
        }
    }
    if (bundle.contains("allegedMechanisms") && bundle["allegedMechanisms"].is_array()) {
        for (const auto& mech : bundle["allegedMechanisms"]) {
            if (IsUnknownOrEmpty(mech, "contextuality")) {
                contextOk = false;
                AddError(report, "AllegedMechanism sem contextuality.");
                break;
            }
        }
    }
    SetCheck(report, "contextuality", contextOk ? "ok" : "error");

    // B) Baseline check
    bool baselineOk = true;
    bool baselineWarning = false;
    if (!bundle.contains("baselineAssumptions") || !bundle["baselineAssumptions"].is_array() || bundle["baselineAssumptions"].empty()) {
        baselineOk = false;
        AddError(report, "baselineAssumptions ausente.");
    } else {
        bool hasDynamic = false;
        bool hasMultiple = false;
        for (const auto& baseline : bundle["baselineAssumptions"]) {
            if (baseline.contains("baselineType") && baseline["baselineType"].is_string()) {
                const std::string bt = Normalize(baseline["baselineType"].get<std::string>());
                if (bt == "dynamic") hasDynamic = true;
                if (bt == "multiple") hasMultiple = true;
            }
        }
        if (!hasDynamic && !hasMultiple) {
            if (bundle.contains("sourceProfile") && bundle["sourceProfile"].is_object()) {
                if (bundle["sourceProfile"].contains("temporalScale") && bundle["sourceProfile"]["temporalScale"].is_string()) {
                    std::string scale = Normalize(bundle["sourceProfile"]["temporalScale"].get<std::string>());
                    if (scale == "long" || scale == "multi") {
                        baselineWarning = true;
                        AddWarning(report, "Baseline fixo em estudo de longa duração pode exigir baseline múltiplo/dinâmico.");
                    }
                }
            }
        }
    }
    if (!baselineOk) {
        SetCheck(report, "baseline", "error");
    } else if (baselineWarning) {
        SetCheck(report, "baseline", "warning");
    } else {
        SetCheck(report, "baseline", "ok");
    }

    // C) Temporal check
    bool temporalOk = true;
    bool temporalWarning = false;
    if (!bundle.contains("temporalWindowReferences") || !bundle["temporalWindowReferences"].is_array() || bundle["temporalWindowReferences"].empty()) {
        // Relaxed: treated as warning instead of blocking error
        temporalWarning = true;
        AddWarning(report, "temporalWindowReferences ausente (Validation Relaxed).");
    } else {
        for (const auto& tw : bundle["temporalWindowReferences"]) {
            if (!IsNonEmptyString(tw, "timeWindow") || !IsNonEmptyString(tw, "changeRhythm") || !IsNonEmptyString(tw, "delaysOrHysteresis")) {
                temporalOk = false;
                AddError(report, "TemporalWindowReference incompleto.");
                break;
            }
            if (tw.contains("timeWindow") && tw["timeWindow"].is_string()) {
                auto warning = DetectTemporalVagueness(tw["timeWindow"].get<std::string>());
                if (!warning.empty()) temporalWarning = true;
            }
        }
    }
    if (!temporalOk) {
        SetCheck(report, "temporal", "error");
    } else if (temporalWarning) {
        SetCheck(report, "temporal", "warning");
    } else {
        SetCheck(report, "temporal", "ok");
    }

    // D) Language check (normative verbs)
    const std::vector<std::string> banned = {
        "permite", "garante", "leva a", "ideal", "deve", "should", "must", "recommend"
    };
    bool languageOk = true;
    auto scanObjectArrayField = [&](const nlohmann::json& arr, const char* field, bool isError) {
        if (!arr.is_array()) return;
        for (const auto& item : arr) {
            if (!item.is_object() || !item.contains(field) || !item[field].is_string()) continue;
            if (ContainsAny(item[field].get<std::string>(), banned)) {
                if (isError) {
                    languageOk = false;
                    AddError(report, std::string("Linguagem normativa detectada em ") + field + ".");
                } else {
                    AddWarning(report, std::string("Linguagem normativa detectada em ") + field + ".");
                }
                break;
            }
        }
    };
    auto scanStringArray = [&](const nlohmann::json& arr, const char* label, bool isError) {
        if (!arr.is_array()) return;
        for (const auto& item : arr) {
            if (!item.is_string()) continue;
            if (ContainsAny(item.get<std::string>(), banned)) {
                if (isError) {
                    languageOk = false;
                    AddError(report, std::string("Linguagem normativa detectada em ") + label + ".");
                } else {
                    AddWarning(report, std::string("Linguagem normativa detectada em ") + label + ".");
                }
                break;
            }
        }
    };
    if (bundle.contains("narrativeObservations")) {
        scanObjectArrayField(bundle["narrativeObservations"], "observation", true);
    }
    if (bundle.contains("allegedMechanisms")) {
        scanObjectArrayField(bundle["allegedMechanisms"], "mechanism", true);
    }
    if (bundle.contains("interpretationLayers") && bundle["interpretationLayers"].is_object()) {
        if (bundle["interpretationLayers"].contains("authorInterpretations")) {
            scanStringArray(bundle["interpretationLayers"]["authorInterpretations"], "authorInterpretations", false);
        }
    }
    SetCheck(report, "language", languageOk ? "ok" : "error");

    // E) Mechanisms check
    bool mechanismsOk = true;
    if (bundle.contains("allegedMechanisms") && bundle["allegedMechanisms"].is_array()) {
        for (const auto& mech : bundle["allegedMechanisms"]) {
            if (IsUnknownOrEmpty(mech, "status")) {
                mechanismsOk = false;
                AddError(report, "AllegedMechanism sem status.");
                break;
            }
            if (IsUnknownOrEmpty(mech, "limitations")) {
                mechanismsOk = false;
                AddError(report, "AllegedMechanism sem limitations.");
                break;
            }
            if (mech.contains("status") && mech["status"].is_string()) {
                std::string status = Normalize(mech["status"].get<std::string>());
                if (status == "tested" && IsUnknownOrEmpty(mech, "evidenceSnippet")) {
                    mechanismsOk = false;
                    AddError(report, "AllegedMechanism marcado como tested sem evidenceSnippet.");
                    break;
                }
            }
        }
    }
    SetCheck(report, "mechanisms", mechanismsOk ? "ok" : "error");

    // F) Layer targeting check
    bool layerOk = true;
    bool hasInterpretation = false;
    if (bundle.contains("interpretationLayers") && bundle["interpretationLayers"].is_object()) {
        hasInterpretation = ArrayHasContent(bundle["interpretationLayers"], "observedStatements") ||
                            ArrayHasContent(bundle["interpretationLayers"], "authorInterpretations") ||
                            ArrayHasContent(bundle["interpretationLayers"], "possibleReadings");
    }
    if (bundle.contains("requestedTargets") && bundle["requestedTargets"].is_array()) {
        for (const auto& tgt : bundle["requestedTargets"]) {
            if (tgt.is_string() && Normalize(tgt.get<std::string>()) == "strata-core" && hasInterpretation) {
                layerOk = false;
                AddError(report, "InterpretationLayers presentes: STRATA-Core não permitido.");
                break;
            }
        }
    }
    SetCheck(report, "layer", layerOk ? "ok" : "error");

    // Status aggregation
    const bool hasErrors = !report["errors"].empty();
    const bool hasWarnings = !report["warnings"].empty();
    report["status"] = hasErrors ? "block" : (hasWarnings ? "pass-with-warnings" : "pass");

    // Seal
    nlohmann::json seal = {
        {"exportAllowed", !hasErrors},
        {"allowedTargets", nlohmann::json::array()}
    };
    if (!hasErrors) {
        if (hasInterpretation) {
            seal["allowedTargets"] = {"STRATA-CAC"};
        } else {
            seal["allowedTargets"] = {"STRATA-Core", "STRATA-CAC"};
        }
    }

    result.exportAllowed = !hasErrors;
    result.report = std::move(report);
    result.seal = std::move(seal);
    return result;
}

} // namespace ideawalker::application::scientific
