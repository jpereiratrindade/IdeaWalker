/**
 * @file ScientificIngestionService.cpp
 * @brief Implementation of ScientificIngestionService.
 */

#include "application/scientific/ScientificIngestionService.hpp"
#include "application/scientific/EpistemicValidator.hpp"

#include <chrono>
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <cctype>
#include <numeric>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "infrastructure/ContentExtractor.hpp"

namespace fs = std::filesystem;

namespace ideawalker::application::scientific {

namespace {

template <typename Container>
bool IsAllowedValue(const std::string& value, const Container& allowed) {
    for (const auto& entry : allowed) {
        if (value == entry) return true;
    }
    return false;
}

std::string ToIsoTimestamp(const std::chrono::system_clock::time_point& tp) {
    std::time_t tt = std::chrono::system_clock::to_time_t(tp);
    std::tm tm = {};
#if defined(_WIN32)
    gmtime_s(&tm, &tt);
#else
    gmtime_r(&tt, &tm);
#endif
    std::ostringstream oss;
    oss << std::put_time(&tm, "%Y-%m-%dT%H:%M:%SZ");
    return oss.str();
}

bool WriteJsonFile(const fs::path& path, const nlohmann::json& payload, std::string& error) {
    std::ofstream file(path);
    if (!file.is_open()) {
        error = "Falha ao abrir arquivo para escrita: " + path.string();
        return false;
    }
    file << payload.dump(2);
    return true;
}

const char* SourceTypeToString(domain::SourceType type) {
    switch (type) {
        case domain::SourceType::PlainText: return "text";
        case domain::SourceType::Markdown: return "markdown";
        case domain::SourceType::PDF: return "pdf";
        case domain::SourceType::LaTeX: return "latex";
        default: return "unknown";
    }
}

bool EndsWith(const std::string& value, const std::string& suffix) {
    if (suffix.size() > value.size()) return false;
    return std::equal(suffix.rbegin(), suffix.rend(), value.rbegin());
}

std::string StripErrorSuffix(const std::string& artifactId) {
    if (EndsWith(artifactId, "_narrative_err")) {
        return artifactId.substr(0, artifactId.size() - std::string("_narrative_err").size());
    }
    if (EndsWith(artifactId, "_discursive_err")) {
        return artifactId.substr(0, artifactId.size() - std::string("_discursive_err").size());
    }
    return artifactId;
}

std::string InferErrorStage(const std::string& artifactId) {
    if (EndsWith(artifactId, "_narrative_err")) return "narrative_json_invalid";
    if (EndsWith(artifactId, "_discursive_err")) return "discursive_json_invalid";
    return "unknown";
}

bool HasAnchoredField(const nlohmann::json& obj, const char* key) {
    if (!obj.contains(key) || !obj[key].is_string()) return false;
    std::string value = obj[key].get<std::string>();
    if (value.empty()) return false;
    std::string lowered = value;
    std::transform(lowered.begin(), lowered.end(), lowered.begin(), [](unsigned char c){ return std::tolower(c); });
    return lowered != "unknown";
}

std::string NormalizeForSearch(const std::string& input) {
    std::string out;
    out.reserve(input.size());
    bool lastWasSpace = false;
    for (unsigned char c : input) {
        if (std::isspace(c)) {
            if (!lastWasSpace) {
                out.push_back(' ');
                lastWasSpace = true;
            }
            continue;
        }
        out.push_back(static_cast<char>(std::tolower(c)));
        lastWasSpace = false;
    }
    return out;
}

std::string TrimCopy(const std::string& input) {
    if (input.empty()) return input;
    size_t start = 0;
    while (start < input.size() && std::isspace(static_cast<unsigned char>(input[start]))) {
        ++start;
    }
    if (start == input.size()) return "";
    size_t end = input.size() - 1;
    while (end > start && std::isspace(static_cast<unsigned char>(input[end]))) {
        --end;
    }
    return input.substr(start, end - start + 1);
}

std::string NormalizeEnumToken(const std::string& input) {
    std::string trimmed = TrimCopy(input);
    std::string out;
    out.reserve(trimmed.size());
    for (unsigned char c : trimmed) {
        out.push_back(static_cast<char>(std::tolower(c)));
    }
    return out;
}

std::vector<std::string> SplitEnumTokens(const std::string& input) {
    std::vector<std::string> out;
    std::string token;
    std::stringstream ss(input);
    while (std::getline(ss, token, '|')) {
        std::string norm = NormalizeEnumToken(token);
        if (!norm.empty()) out.push_back(norm);
    }
    return out;
}

template <typename Container>
nlohmann::json BuildEnumCandidates(const std::vector<std::string>& tokens, const Container& allowed) {
    std::vector<std::string> unique;
    for (const auto& token : tokens) {
        if (!IsAllowedValue(token, allowed)) continue;
        if (std::find(unique.begin(), unique.end(), token) == unique.end()) {
            unique.push_back(token);
        }
    }
    if (unique.empty()) return nlohmann::json();
    const double confidence = 1.0 / static_cast<double>(unique.size());
    nlohmann::json arr = nlohmann::json::array();
    for (const auto& val : unique) {
        arr.push_back({{"value", val}, {"confidence", confidence}});
    }
    return arr;
}

size_t CountArraySafe(const nlohmann::json& obj, const char* key) {
    if (!obj.is_object()) return 0;
    if (!obj.contains(key) || !obj[key].is_array()) return 0;
    return obj[key].size();
}

void SanitizeSourceProfileKeys(nlohmann::json& bundle) {
    if (!bundle.contains("sourceProfile") || !bundle["sourceProfile"].is_object()) return;
    auto& profile = bundle["sourceProfile"];
    std::unordered_set<std::string> allowed = {
        "studyType",
        "temporalScale",
        "ecosystemType",
        "evidenceType",
        "transferability",
        "contextNotes",
        "limitations",
        "studyTypeCandidates",
        "temporalScaleCandidates",
        "ecosystemTypeCandidates",
        "evidenceTypeCandidates",
        "transferabilityCandidates"
    };
    std::vector<std::string> toRemove;
    for (auto it = profile.begin(); it != profile.end(); ++it) {
        if (allowed.count(it.key()) == 0) {
            toRemove.push_back(it.key());
        }
    }
    for (const auto& key : toRemove) {
        profile.erase(key);
    }
}

std::string ExtractFocusedNarrativeText(const std::string& content) {
    const std::string lower = NormalizeForSearch(content);
    auto findSection = [&](const std::string& label) -> size_t {
        return lower.find(label);
    };
    size_t start = std::string::npos;
    size_t end = std::string::npos;

    const std::vector<std::string> abstractLabels = {"abstract", "resumo"};
    for (const auto& label : abstractLabels) {
        start = findSection(label);
        if (start != std::string::npos) break;
    }
    const std::vector<std::string> introLabels = {"introduction", "introducao", "introducción"};
    for (const auto& label : introLabels) {
        size_t pos = findSection(label);
        if (pos != std::string::npos && (start == std::string::npos || pos < start)) {
            start = pos;
        }
    }

    const std::vector<std::string> endLabels = {
        "methods", "materials", "method", "metodos", "métodos",
        "results", "discussion", "conclusion", "conclusao", "conclusión"
    };
    if (start != std::string::npos) {
        end = std::string::npos;
        for (const auto& label : endLabels) {
            size_t pos = lower.find(label, start + 1);
            if (pos != std::string::npos) {
                if (end == std::string::npos || pos < end) end = pos;
            }
        }
    }

    size_t sliceStart = (start != std::string::npos) ? start : 0;
    size_t sliceEnd = (end != std::string::npos && end > sliceStart) ? end : std::min(lower.size(), sliceStart + 3500);
    if (sliceEnd <= sliceStart || sliceEnd > content.size()) {
        sliceEnd = std::min(content.size(), sliceStart + 3500);
    }
    std::string snippet = content.substr(sliceStart, sliceEnd - sliceStart);
    if (snippet.size() < 800 && content.size() > snippet.size()) {
        snippet = content.substr(0, std::min<size_t>(content.size(), 3500));
    }
    return snippet;
}

void NormalizeBundleEnums(nlohmann::json& bundle) {
    auto sanitizeEnum = [](nlohmann::json& obj, const char* key, const auto& allowed) {
        if (!obj.contains(key) || !obj[key].is_string()) return;
        const std::string raw = obj[key].get<std::string>();
        const bool hasPipe = raw.find('|') != std::string::npos;
        const std::string normalized = NormalizeEnumToken(raw);
        const std::string candidatesKey = std::string(key) + "Candidates";

        if (hasPipe) {
            const auto tokens = SplitEnumTokens(raw);
            nlohmann::json candidates = BuildEnumCandidates(tokens, allowed);
            if (!candidates.is_null() && !obj.contains(candidatesKey)) {
                obj[candidatesKey] = candidates;
            }
            if (!candidates.is_null() && !candidates.empty() && candidates[0].contains("value")) {
                obj[key] = candidates[0]["value"];
                return;
            }
        }

        if (IsAllowedValue(normalized, allowed)) {
            obj[key] = normalized;
            return;
        }
        // If still invalid, keep original (validator will catch it).
    };

    if (bundle.contains("sourceProfile") && bundle["sourceProfile"].is_object()) {
        auto& p = bundle["sourceProfile"];
        sanitizeEnum(p, "studyType", domain::scientific::ScientificSchema::StudyTypes);
        sanitizeEnum(p, "temporalScale", domain::scientific::ScientificSchema::TemporalScales);
        sanitizeEnum(p, "ecosystemType", domain::scientific::ScientificSchema::EcosystemTypes);
        sanitizeEnum(p, "evidenceType", domain::scientific::ScientificSchema::EvidenceTypes);
        sanitizeEnum(p, "transferability", domain::scientific::ScientificSchema::TransferabilityLevels);
    }
    if (bundle.contains("allegedMechanisms") && bundle["allegedMechanisms"].is_array()) {
        for (auto& mech : bundle["allegedMechanisms"]) {
            if (!mech.is_object()) continue;
            sanitizeEnum(mech, "status", domain::scientific::ScientificSchema::MechanismStatus);
        }
    }
    if (bundle.contains("baselineAssumptions") && bundle["baselineAssumptions"].is_array()) {
        for (auto& base : bundle["baselineAssumptions"]) {
            if (!base.is_object()) continue;
            sanitizeEnum(base, "baselineType", domain::scientific::ScientificSchema::BaselineTypes);
        }
    }
}

// Tokenizer helper
std::vector<std::string> Tokenize(const std::string& input) {
    std::vector<std::string> tokens;
    std::string current;
    for (char c : input) {
        if (std::isspace(static_cast<unsigned char>(c))) {
            if (!current.empty()) {
                tokens.push_back(current);
                current.clear();
            }
        } else {
            current.push_back(std::tolower(static_cast<unsigned char>(c)));
        }
    }
    if (!current.empty()) tokens.push_back(current);
    return tokens;
}

// Levenshtein distance for tokens
size_t LevenshteinDistance(const std::string& s1, const std::string& s2) {
    const size_t m = s1.size();
    const size_t n = s2.size();
    if (m == 0) return n;
    if (n == 0) return m;
    std::vector<size_t> row(n + 1);
    std::iota(row.begin(), row.end(), 0);
    for (size_t i = 1; i <= m; ++i) {
        row[0] = i;
        size_t previous = i - 1;
        for (size_t j = 1; j <= n; ++j) {
            size_t old_row = row[j];
            row[j] = std::min({old_row + 1, row[j - 1] + 1, previous + (s1[i - 1] == s2[j - 1] ? 0 : 1)});
            previous = old_row;
        }
    }
    return row[n];
}

bool IsTokenMatch(const std::string& t1, const std::string& t2) {
    if (t1 == t2) return true;
    if (t1.size() < 4 || t2.size() < 4) return false; // Strict for short words
    size_t dist = LevenshteinDistance(t1, t2);
    // Allow 1 edit for medium words, 2 for long
    size_t threshold = (t1.size() > 6) ? 2 : 1;
    return dist <= threshold;
}

bool SnippetAppearsInContent(const std::string& snippet, const std::string& content) {
    if (snippet.empty()) return false;
    
    // 1. Fast Path: Exact Substring Search (Normalized)
    const std::string normSnippet = NormalizeForSearch(snippet);
    const std::string normContent = NormalizeForSearch(content);
    if (normContent.find(normSnippet) != std::string::npos) return true;

    // 2. Robust Path: Token-Based Sliding Window
    auto sTokens = Tokenize(normSnippet);
    if (sTokens.empty()) return false;
    auto cTokens = Tokenize(normContent);
    if (cTokens.size() < sTokens.size()) return false;

    // Sliding window
    size_t windowSize = sTokens.size();
    size_t maxErrors = std::max((size_t)1, windowSize / 4); // Allow 25% token mismatch
    
    for (size_t i = 0; i <= cTokens.size() - windowSize; ++i) {
        size_t errors = 0;
        for (size_t j = 0; j < windowSize; ++j) {
            if (!IsTokenMatch(sTokens[j], cTokens[i + j])) {
                errors++;
                if (errors > maxErrors) break;
            }
        }
        if (errors <= maxErrors) return true;
    }

    return false;
}

void FilterByAnchoring(nlohmann::json& array,
                       const std::vector<const char*>& requiredKeys,
                       const std::string& content) {
    if (!array.is_array()) return;
    nlohmann::json filtered = nlohmann::json::array();
    for (const auto& item : array) {
        if (!item.is_object()) continue;
        bool ok = true;
        for (const auto& key : requiredKeys) {
            if (!HasAnchoredField(item, key)) {
                ok = false;
                break;
            }
        }
        if (ok && item.contains("evidenceSnippet") && item["evidenceSnippet"].is_string()) {
            if (!SnippetAppearsInContent(item["evidenceSnippet"].get<std::string>(), content)) {
                ok = false;
            }
        }
        if (ok) filtered.push_back(item);
    }
    array = std::move(filtered);
}

void SanitizeBundleAnchoring(nlohmann::json& bundle, const std::string& content) {
    // Narrative Anchoring
    if (bundle.contains("narrativeObservations")) {
        FilterByAnchoring(bundle["narrativeObservations"], {"evidenceSnippet"}, content);
    }
    if (bundle.contains("allegedMechanisms")) {
        FilterByAnchoring(bundle["allegedMechanisms"], {"evidenceSnippet"}, content);
    }
    if (bundle.contains("temporalWindowReferences")) {
        FilterByAnchoring(bundle["temporalWindowReferences"], {"evidenceSnippet"}, content);
    }

    // Discursive Anchoring (Hallucination Mitigation)
    if (bundle.contains("discursiveContext") && bundle["discursiveContext"].is_object()) {
        auto& dc = bundle["discursiveContext"];
        if (dc.contains("frames")) {
            FilterByAnchoring(dc["frames"], {"evidenceSnippet"}, content);
        }
    }
    if (bundle.contains("discursiveSystem") && bundle["discursiveSystem"].is_object()) {
        auto& ds = bundle["discursiveSystem"];
        if (ds.contains("declaredProblems")) {
            FilterByAnchoring(ds["declaredProblems"], {"evidenceSnippet"}, content);
        }
        if (ds.contains("declaredActions")) {
            FilterByAnchoring(ds["declaredActions"], {"evidenceSnippet"}, content);
        }
        if (ds.contains("expectedEffects")) {
             FilterByAnchoring(ds["expectedEffects"], {"evidenceSnippet"}, content);
        }
    }
}

std::string JsonValueToString(const nlohmann::json& value) {
    if (value.is_string()) return value.get<std::string>();
    if (value.is_boolean()) return value.get<bool>() ? "true" : "false";
    if (value.is_number_integer()) return std::to_string(value.get<long long>());
    if (value.is_number_unsigned()) return std::to_string(value.get<unsigned long long>());
    if (value.is_number_float()) {
        std::ostringstream oss;
        oss << value.get<double>();
        return oss.str();
    }
    if (value.is_null()) return "";
    return value.dump();
}

void AddStringField(nlohmann::json& target, const std::string& key, const nlohmann::json& value) {
    target[key] = JsonValueToString(value);
}

void AppendStatementFrom(nlohmann::json& out, const nlohmann::json& item, const std::vector<std::string>& keys) {
    if (item.is_string()) {
        const auto text = item.get<std::string>();
        if (!text.empty()) out.push_back({{"statement", text}});
        return;
    }
    if (!item.is_object()) return;
    if (item.contains("statement") && item["statement"].is_string()) {
        const auto text = item["statement"].get<std::string>();
        if (!text.empty()) out.push_back({{"statement", text}});
        return;
    }
    for (const auto& key : keys) {
        if (item.contains(key) && item[key].is_string()) {
            const auto text = item[key].get<std::string>();
            if (!text.empty()) out.push_back({{"statement", text}});
            return;
        }
    }
}

bool ValidateStrataNarrativeEnvelope(const nlohmann::json& envelope, std::string& error) {
    if (!envelope.contains("history") || !envelope["history"].is_array()) {
        error = "Narrative envelope invalido: 'history' ausente ou nao-array.";
        return false;
    }
    for (const auto& item : envelope["history"]) {
        if (!item.is_object()) {
            error = "Narrative envelope invalido: item nao-objeto.";
            return false;
        }
        if (!item.contains("metadata") || !item["metadata"].is_object()) {
            error = "Narrative envelope invalido: metadata ausente ou nao-objeto.";
            return false;
        }
        for (auto it = item["metadata"].begin(); it != item["metadata"].end(); ++it) {
            if (!it.value().is_string()) {
                error = "Narrative envelope invalido: metadata deve conter apenas strings.";
                return false;
            }
        }
    }
    return true;
}

bool ValidateStrataDiscursiveEnvelope(const nlohmann::json& envelope, std::string& error) {
    if (!envelope.contains("systems") || !envelope["systems"].is_array()) {
        error = "Discursive envelope invalido: 'systems' ausente ou nao-array.";
        return false;
    }
    for (const auto& sys : envelope["systems"]) {
        if (!sys.is_object()) {
            error = "Discursive envelope invalido: system nao-objeto.";
            return false;
        }
        if (sys.contains("interpretationMetadata")) {
            if (!sys["interpretationMetadata"].is_object()) {
                error = "Discursive envelope invalido: interpretationMetadata nao-objeto.";
                return false;
            }
            for (auto it = sys["interpretationMetadata"].begin(); it != sys["interpretationMetadata"].end(); ++it) {
                if (!it.value().is_string()) {
                    error = "Discursive envelope invalido: interpretationMetadata deve conter apenas strings.";
                    return false;
                }
            }
        }
        const std::vector<std::string> statementArrays = {
            "declaredProblems", "declaredActions", "allegedMechanisms", "expectedEffects"
        };
        for (const auto& key : statementArrays) {
            if (!sys.contains(key) || !sys[key].is_array()) continue;
            for (const auto& item : sys[key]) {
                if (!item.is_object() || !item.contains("statement") || !item["statement"].is_string()) {
                    error = "Discursive envelope invalido: itens devem ter 'statement' string.";
                    return false;
                }
            }
        }
    }
    return true;
}

} // namespace

ScientificIngestionService::ScientificIngestionService(
    std::unique_ptr<infrastructure::FileSystemArtifactScanner> scanner,
    std::shared_ptr<domain::AIService> aiService,
    const std::string& observationsPath,
    const std::string& consumablesPath)
    : m_scanner(std::move(scanner)),
      m_aiService(std::move(aiService)),
      m_observationsPath(observationsPath),
      m_consumablesPath(consumablesPath) {
    if (!fs::exists(m_observationsPath)) {
        fs::create_directories(m_observationsPath);
    }
    if (!fs::exists(m_consumablesPath)) {
        fs::create_directories(m_consumablesPath);
    }
    if (!fs::exists(m_consumablesPath)) {
        fs::create_directories(m_consumablesPath);
    }
}

bool ScientificIngestionService::ingestScientificBundle(const std::string& jsonContent, const std::string& artifactId) {
    nlohmann::json bundle;
    try {
        bundle = nlohmann::json::parse(jsonContent);
    } catch (...) {
        return false;
    }

    NormalizeBundleEnums(bundle);

    // Note: We skip SanitizeBundleAnchoring as we don't have the original raw text here easily,
    // relying on the Rigorous Persona prompt integrity.

    std::vector<std::string> validationErrors;
    if (!validateBundleJson(bundle, validationErrors)) {
         std::string saveError;
         saveErrorPayload(artifactId, jsonContent, saveError);
         return false;
    }

    // Minimal Source Metadata Injection if missing
    if (!bundle.contains("source") || !bundle["source"].is_object()) {
         bundle["source"] = {
             {"artifactId", artifactId},
             {"ingestedAt", ToIsoTimestamp(std::chrono::system_clock::now())},
             {"model", "scientific-observer-persona"}
         };
    }

    std::string saveError;
    if (!saveRawBundle(bundle, artifactId, saveError)) return false;

    EpistemicValidator validator;
    auto validation = validator.Validate(bundle);
    
    std::string validationError;
    fs::path validationDir = fs::path(m_observationsPath) / "validation";
    if (!fs::exists(validationDir)) fs::create_directories(validationDir);
    
    WriteJsonFile(validationDir / (artifactId + ".json"), validation.report, validationError);

    if (!validation.exportAllowed) return true; // Successfully processed, even if blocked

    if (!exportConsumables(bundle, artifactId, saveError)) return false;

    fs::path consumableDir = fs::path(m_consumablesPath) / artifactId;
    WriteJsonFile(consumableDir / "EpistemicValidationReport.json", validation.report, saveError);
    WriteJsonFile(consumableDir / "ExportSeal.json", validation.seal, saveError);

    return true;
}

ScientificIngestionService::IngestionResult ScientificIngestionService::ingestPending(
    std::function<void(std::string)> statusCallback) {
    if (statusCallback) statusCallback("Varrendo inbox científica...");
    auto artifacts = m_scanner->scan();
    return processArtifacts(artifacts, false, statusCallback);
}

std::vector<domain::SourceArtifact> ScientificIngestionService::listInboxArtifacts() {
    if (!m_scanner) return {};
    auto artifacts = m_scanner->scan();
    std::sort(artifacts.begin(), artifacts.end(), [](const auto& a, const auto& b) {
        return a.filename < b.filename;
    });
    return artifacts;
}

ScientificIngestionService::IngestionResult ScientificIngestionService::ingestSelected(
    const std::vector<domain::SourceArtifact>& artifacts,
    bool purgeExisting,
    std::function<void(std::string)> statusCallback) {
    return processArtifacts(artifacts, purgeExisting, statusCallback);
}

ScientificIngestionService::IngestionResult ScientificIngestionService::processArtifacts(
    const std::vector<domain::SourceArtifact>& artifacts,
    bool purgeExisting,
    std::function<void(std::string)> statusCallback) {
    IngestionResult result{0, 0, {}};
    result.artifactsDetected = static_cast<int>(artifacts.size());

    bool purgePerformed = false;
    if (purgeExisting) {
        for (const auto& artifact : artifacts) {
            std::string purgeError;
            if (!purgeExistingArtifacts(artifact.filename, purgeError)) {
                if (!purgeError.empty()) {
                    result.errors.push_back(purgeError);
                }
            } else {
                purgePerformed = true;
            }
        }
    }

    for (const auto& artifact : artifacts) {
        if (!m_aiService) {
            result.errors.push_back("Serviço de IA não configurado para ingestão científica.");
            break;
        }
        if (statusCallback) statusCallback("Processando artigo: " + artifact.filename);

        auto resultData = infrastructure::ContentExtractor::Extract(artifact.path, statusCallback);

        if (!resultData.success || resultData.content.empty()) {
            std::string err = "Falha na extração de texto para " + artifact.filename + ": Conteúdo vazio ou ilegível.";
            result.errors.push_back(err);
            std::string saveError;
            saveErrorPayload(buildArtifactId(artifact), err, saveError);
            continue;
        }

        if (statusCallback) {
             std::string msg = "Extraído via " + resultData.method;
             if (!resultData.warnings.empty()) msg += " (com avisos)";
             statusCallback(msg);
        }

        std::string content = resultData.content;
        const std::string artifactId = buildArtifactId(artifact);

        // --- Phase 1: Narrative Extraction ---
        if (statusCallback) statusCallback("Extraindo Narrativa (1/2)...");
        const std::string narrativeSystemPrompt = buildNarrativeSystemPrompt();
        const std::string narrativeUserPrompt = buildNarrativeUserPrompt(artifact, content);
        
        auto narrativeResponse = m_aiService->generateJson(narrativeSystemPrompt, narrativeUserPrompt);
        if (!narrativeResponse) {
            result.errors.push_back("Falha na IA (Narrativa) para: " + artifact.filename);
            continue;
        }

        nlohmann::json narrativeBundle;
        try {
            narrativeBundle = nlohmann::json::parse(*narrativeResponse);
        } catch (...) {
            std::string saveError;
            saveErrorPayload(artifactId + "_narrative_err", *narrativeResponse, saveError);
            result.errors.push_back("JSON inválido (Narrativa) para " + artifact.filename);
            continue;
        }
        SanitizeSourceProfileKeys(narrativeBundle);

        // Quick anchoring check to decide fallback
        nlohmann::json narrativeProbe = narrativeBundle;
        SanitizeBundleAnchoring(narrativeProbe, content);
        size_t probeObs = CountArraySafe(narrativeProbe, "narrativeObservations");
        size_t probeMech = CountArraySafe(narrativeProbe, "allegedMechanisms");
        if (probeObs == 0 || probeMech == 0) {
            if (statusCallback) statusCallback("Narrativa vazia após ancoragem. Tentando fallback (Abstract/Introduction)...");
            const std::string focusedContent = ExtractFocusedNarrativeText(content);
            std::string fallbackSystem = buildNarrativeSystemPrompt();
            fallbackSystem += "FOCO: você está vendo apenas um recorte (Abstract/Introduction). Use apenas trechos literais.\n";
            const std::string fallbackUser = buildNarrativeUserPrompt(artifact, focusedContent);
            auto fallbackResponse = m_aiService->generateJson(fallbackSystem, fallbackUser);
            if (fallbackResponse) {
                try {
                    narrativeBundle = nlohmann::json::parse(*fallbackResponse);
                    SanitizeSourceProfileKeys(narrativeBundle);
                } catch (...) {
                    std::string saveError;
                    saveErrorPayload(artifactId + "_narrative_err", *fallbackResponse, saveError);
                    result.errors.push_back("JSON inválido (Narrativa Fallback) para " + artifact.filename);
                }
            }
        }

        // --- Phase 2: Discursive Extraction ---
        if (statusCallback) statusCallback("Extraindo Discursiva (2/2)...");
        const std::string discursiveSystemPrompt = buildDiscursiveSystemPrompt();
        const std::string discursiveUserPrompt = buildDiscursiveUserPrompt(artifact, content);

        auto discursiveResponse = m_aiService->generateJson(discursiveSystemPrompt, discursiveUserPrompt);
        nlohmann::json discursiveBundle = nlohmann::json::object();
        if (discursiveResponse) {
             try {
                discursiveBundle = nlohmann::json::parse(*discursiveResponse);
             } catch (...) {
                 std::string saveError;
                 saveErrorPayload(artifactId + "_discursive_err", *discursiveResponse, saveError);
                 result.errors.push_back("JSON inválido (Discursiva) para " + artifact.filename + " (ignorando parcial)");
             }
        } else {
             result.errors.push_back("Falha na IA (Discursiva) para: " + artifact.filename + " (ignorando parcial)");
        }

        // Discursive fallback when everything is empty after anchoring
        nlohmann::json discursiveProbe = discursiveBundle;
        SanitizeBundleAnchoring(discursiveProbe, content);
        size_t probeFrames = 0, probeProb = 0, probeAct = 0, probeEff = 0;
        if (discursiveProbe.contains("discursiveContext") && discursiveProbe["discursiveContext"].is_object()) {
            probeFrames = CountArraySafe(discursiveProbe["discursiveContext"], "frames");
        }
        if (discursiveProbe.contains("discursiveSystem") && discursiveProbe["discursiveSystem"].is_object()) {
            probeProb = CountArraySafe(discursiveProbe["discursiveSystem"], "declaredProblems");
            probeAct = CountArraySafe(discursiveProbe["discursiveSystem"], "declaredActions");
            probeEff = CountArraySafe(discursiveProbe["discursiveSystem"], "expectedEffects");
        }
        if (probeFrames + probeProb + probeAct + probeEff == 0) {
            if (statusCallback) statusCallback("Discursiva vazia após ancoragem. Tentando fallback (Abstract/Introduction)...");
            const std::string focusedContent = ExtractFocusedNarrativeText(content);
            std::string fallbackSystem = buildDiscursiveSystemPrompt();
            fallbackSystem += "FOCO: você está vendo apenas um recorte (Abstract/Introduction). Use apenas trechos literais.\n";
            const std::string fallbackUser = buildDiscursiveUserPrompt(artifact, focusedContent);
            auto fallbackResponse = m_aiService->generateJson(fallbackSystem, fallbackUser);
            if (fallbackResponse) {
                try {
                    discursiveBundle = nlohmann::json::parse(*fallbackResponse);
                } catch (...) {
                    std::string saveError;
                    saveErrorPayload(artifactId + "_discursive_err", *fallbackResponse, saveError);
                    result.errors.push_back("JSON inválido (Discursiva Fallback) para " + artifact.filename + " (ignorando parcial)");
                }
            }
        }

        // --- Merge Bundles ---
        nlohmann::json bundle = narrativeBundle;
        
        // Merge Discursive Context
        if (discursiveBundle.contains("discursiveContext")) {
            bundle["discursiveContext"] = discursiveBundle["discursiveContext"];
        }
        // Merge Discursive System
        if (discursiveBundle.contains("discursiveSystem")) {
            bundle["discursiveSystem"] = discursiveBundle["discursiveSystem"];
        }
        // Merge Interpretation Layers (combine if both exist, prefer discursive for author interpretations)
        if (discursiveBundle.contains("interpretationLayers")) {
             if (!bundle.contains("interpretationLayers")) {
                 bundle["interpretationLayers"] = discursiveBundle["interpretationLayers"];
             } else {
                 auto& target = bundle["interpretationLayers"];
                 const auto& source = discursiveBundle["interpretationLayers"];
                 if (source.contains("authorInterpretations")) target["authorInterpretations"] = source["authorInterpretations"];
                 if (source.contains("possibleReadings")) target["possibleReadings"] = source["possibleReadings"];
             }
        }

        if (discursiveBundle.contains("interpretationLayers")) {
             if (!bundle.contains("interpretationLayers")) {
                 bundle["interpretationLayers"] = discursiveBundle["interpretationLayers"];
             } else {
                 auto& target = bundle["interpretationLayers"];
                 const auto& source = discursiveBundle["interpretationLayers"];
                 if (source.contains("authorInterpretations")) target["authorInterpretations"] = source["authorInterpretations"];
                 if (source.contains("possibleReadings")) target["possibleReadings"] = source["possibleReadings"];
             }
        }

        NormalizeBundleEnums(bundle);
        SanitizeSourceProfileKeys(bundle);

        size_t preNarr = CountArraySafe(bundle, "narrativeObservations");
        size_t preMech = CountArraySafe(bundle, "allegedMechanisms");
        size_t preTemp = CountArraySafe(bundle, "temporalWindowReferences");
        size_t preFrames = 0, preProb = 0, preAct = 0, preEff = 0;
        if (bundle.contains("discursiveContext") && bundle["discursiveContext"].is_object()) {
            preFrames = CountArraySafe(bundle["discursiveContext"], "frames");
        }
        if (bundle.contains("discursiveSystem") && bundle["discursiveSystem"].is_object()) {
            preProb = CountArraySafe(bundle["discursiveSystem"], "declaredProblems");
            preAct = CountArraySafe(bundle["discursiveSystem"], "declaredActions");
            preEff = CountArraySafe(bundle["discursiveSystem"], "expectedEffects");
        }

        SanitizeBundleAnchoring(bundle, content);

        size_t postNarr = CountArraySafe(bundle, "narrativeObservations");
        size_t postMech = CountArraySafe(bundle, "allegedMechanisms");
        size_t postTemp = CountArraySafe(bundle, "temporalWindowReferences");
        size_t postFrames = 0, postProb = 0, postAct = 0, postEff = 0;
        if (bundle.contains("discursiveContext") && bundle["discursiveContext"].is_object()) {
            postFrames = CountArraySafe(bundle["discursiveContext"], "frames");
        }
        if (bundle.contains("discursiveSystem") && bundle["discursiveSystem"].is_object()) {
            postProb = CountArraySafe(bundle["discursiveSystem"], "declaredProblems");
            postAct = CountArraySafe(bundle["discursiveSystem"], "declaredActions");
            postEff = CountArraySafe(bundle["discursiveSystem"], "expectedEffects");
        }
        if (statusCallback) {
            std::ostringstream oss;
            oss << "Anchoring: narr " << preNarr << "->" << postNarr
                << " | mech " << preMech << "->" << postMech
                << " | temp " << preTemp << "->" << postTemp
                << " | frames " << preFrames << "->" << postFrames
                << " | problems " << preProb << "->" << postProb
                << " | actions " << preAct << "->" << postAct
                << " | effects " << preEff << "->" << postEff;
            statusCallback(oss.str());
        }

        std::vector<std::string> validationErrors;
        if (!validateBundleJson(bundle, validationErrors)) {
            std::ostringstream oss;
            oss << "Falha de validação para " << artifact.filename << ": ";
            for (size_t i = 0; i < validationErrors.size(); ++i) {
                if (i > 0) oss << "; ";
                oss << validationErrors[i];
            }
            result.errors.push_back(oss.str());
            std::string saveError;
            saveErrorPayload(artifactId, bundle.dump(2), saveError);
            continue;
        }

        attachSourceMetadata(bundle, artifact, artifactId, resultData.method, resultData.sourceSha256);

        std::string saveError;
        if (!saveRawBundle(bundle, artifactId, saveError)) {
            result.errors.push_back(saveError);
            continue;
        }

        EpistemicValidator validator;
        auto validation = validator.Validate(bundle);
        std::string validationError;
        fs::path validationDir = fs::path(m_observationsPath) / "validation";
        if (!fs::exists(validationDir)) {
            fs::create_directories(validationDir);
        }
        WriteJsonFile(validationDir / (artifactId + ".json"), validation.report, validationError);

        if (!validation.exportAllowed) {
            result.errors.push_back("Exportação bloqueada pelo Validador Epistemológico: " + artifact.filename);
            continue;
        }

        if (!exportConsumables(bundle, artifactId, saveError)) {
            result.errors.push_back(saveError);
            continue;
        }

        fs::path consumableDir = fs::path(m_consumablesPath) / artifactId;
        WriteJsonFile(consumableDir / "EpistemicValidationReport.json", validation.report, saveError);
        WriteJsonFile(consumableDir / "ExportSeal.json", validation.seal, saveError);

        result.bundlesGenerated++;
    }

    if (result.bundlesGenerated > 0 || purgePerformed) {
        generateIngestionReport();
    }

    return result;
}

bool ScientificIngestionService::purgeExistingArtifacts(const std::string& filename, std::string& error) const {
    bool removed = false;
    std::string suffix = "_" + filename;

    auto removeFileIfMatch = [&](const fs::path& path) {
        if (!path.has_filename()) return;
        const std::string stem = path.stem().string();
        const std::string base = StripErrorSuffix(stem);
        if (EndsWith(base, suffix)) {
            std::error_code ec;
            fs::remove(path, ec);
            if (ec) {
                if (!error.empty()) error += "; ";
                error += "Falha ao remover " + path.string() + ": " + ec.message();
            } else {
                removed = true;
            }
        }
    };

    fs::path obsDir = fs::path(m_observationsPath);
    if (fs::exists(obsDir)) {
        for (const auto& entry : fs::directory_iterator(obsDir)) {
            if (entry.is_regular_file() && entry.path().extension() == ".json") {
                removeFileIfMatch(entry.path());
            }
        }
        fs::path validationDir = obsDir / "validation";
        if (fs::exists(validationDir)) {
            for (const auto& entry : fs::directory_iterator(validationDir)) {
                if (entry.is_regular_file() && entry.path().extension() == ".json") {
                    removeFileIfMatch(entry.path());
                }
            }
        }
        fs::path errorDir = obsDir / "errors";
        if (fs::exists(errorDir)) {
            for (const auto& entry : fs::directory_iterator(errorDir)) {
                if (entry.is_regular_file() && entry.path().extension() == ".json") {
                    removeFileIfMatch(entry.path());
                }
            }
        }
    }

    fs::path consDir = fs::path(m_consumablesPath);
    if (fs::exists(consDir)) {
        for (const auto& entry : fs::directory_iterator(consDir)) {
            if (!entry.is_directory()) continue;
            const std::string dirName = entry.path().filename().string();
            if (EndsWith(dirName, suffix)) {
                std::error_code ec;
                fs::remove_all(entry.path(), ec);
                if (ec) {
                    if (!error.empty()) error += "; ";
                    error += "Falha ao remover " + entry.path().string() + ": " + ec.message();
                } else {
                    removed = true;
                }
            }
        }
    }

    return removed;
}

size_t ScientificIngestionService::getBundlesCount() const {
    if (!fs::exists(m_observationsPath)) return 0;
    size_t count = 0;
    for (const auto& entry : fs::directory_iterator(m_observationsPath)) {
        if (entry.is_regular_file() && entry.path().extension() == ".json") {
            ++count;
        }
    }
    return count;
}

std::optional<ScientificIngestionService::ValidationSummary> ScientificIngestionService::getLatestValidationSummary() const {
    fs::path validationDir = fs::path(m_observationsPath) / "validation";
    if (!fs::exists(validationDir) || !fs::is_directory(validationDir)) {
        return std::nullopt;
    }

    fs::path latestFile;
    std::chrono::system_clock::time_point latestTime;
    bool hasFile = false;

    for (const auto& entry : fs::directory_iterator(validationDir)) {
        if (!entry.is_regular_file() || entry.path().extension() != ".json") continue;
        auto ftime = fs::last_write_time(entry);
        auto sysTime = std::chrono::time_point_cast<std::chrono::system_clock::duration>(
            ftime - fs::file_time_type::clock::now() + std::chrono::system_clock::now()
        );
        if (!hasFile || sysTime > latestTime) {
            latestTime = sysTime;
            latestFile = entry.path();
            hasFile = true;
        }
    }

    if (!hasFile) return std::nullopt;

    std::ifstream file(latestFile);
    if (!file.is_open()) return std::nullopt;

    std::stringstream buffer;
    buffer << file.rdbuf();
    const std::string reportJson = buffer.str();

    nlohmann::json report;
    try {
        report = nlohmann::json::parse(reportJson);
    } catch (...) {
        return std::nullopt;
    }

    ValidationSummary summary;
    summary.path = latestFile.string();
    summary.reportJson = reportJson;
    if (report.contains("status") && report["status"].is_string()) {
        summary.status = report["status"].get<std::string>();
    }
    if (report.contains("errors") && report["errors"].is_array()) {
        summary.errorCount = report["errors"].size();
    }
    if (report.contains("warnings") && report["warnings"].is_array()) {
        summary.warningCount = report["warnings"].size();
    }
    summary.exportAllowed = (summary.status != "block");

    return summary;
}

std::string ScientificIngestionService::buildNarrativeSystemPrompt() const {
    std::ostringstream ss;
    ss << "Você é um analista científico do IdeaWalker.\n"
       << "Objetivo: produzir ARTEFATOS COGNITIVOS NARRATIVOS (Observações e Mecanismos).\n"
       << "Foque em extrair o que ACONTECEU (observações) e COMO funciona (mecanismos) com base na evidência empírica.\n"
       << "Responda APENAS com JSON válido e estritamente no esquema solicitado.\n"
       << "Campos categóricos DEVEM ter exatamente UM valor permitido. Nunca combine valores com '|'.\n"
       << "Se houver ambiguidade, use 'unknown' e, opcionalmente, inclua <campo>Candidates com {value, confidence 0-1}.\n"
       << "Se o texto contiver evidência explícita no Abstract/Introduction/Methods/Results, não deixe os arrays vazios.\n"
       << "Inclua pelo menos 1 narrativeObservation e 1 allegedMechanism quando houver evidência textual clara.\n"
       << "Todo item deve conter evidenceSnippet (trecho literal), sourceSection e pageRange.\n";
    return ss.str();
}

std::string ScientificIngestionService::buildNarrativeUserPrompt(
    const domain::SourceArtifact& artifact,
    const std::string& content) const {
    std::ostringstream ss;
    ss << "ARQUIVO: " << artifact.filename << "\n"
       << "CONTEÚDO DO ARTIGO:\n"
       << "------------------------\n"
       << content << "\n"
       << "------------------------\n\n"
       << "ESQUEMA JSON OBRIGATÓRIO (Narrative Focus):\n"
       << "VALORES PERMITIDOS (campos categóricos):\n"
       << "sourceProfile:\n"
       << "- studyType: experimental|observational|review|theoretical|simulation|mixed|unknown\n"
       << "- temporalScale: short|medium|long|multi|unknown\n"
       << "- ecosystemType: terrestrial|aquatic|urban|agro|industrial|social|digital|mixed|unknown\n"
       << "- evidenceType: empirical|theoretical|mixed|unknown\n"
       << "- transferability: high|medium|low|contextual|unknown\n"
       << "narrativeObservations:\n"
       << "- confidence: low|medium|high|unknown\n"
       << "- evidence: direct|inferred|unknown\n"
       << "- contextuality: site-specific|conditional|comparative|non-universal\n"
       << "allegedMechanisms:\n"
       << "- status: tested|inferred|speculative|unknown\n"
       << "baselineAssumptions:\n"
       << "- baselineType: fixed|dynamic|multiple|none|unknown\n"
       << "Regra: escolha exatamente UM valor. Nunca combine com '|'.\n"
       << "Se houver ambiguidade, use 'unknown' e (opcional) inclua <campo>Candidates com {value, confidence}.\n"
       << "Obrigatório: se houver evidência explícita no Abstract/Introduction/Methods/Results, gere pelo menos 1 narrativeObservation e 1 allegedMechanism.\n"
       << "{\n"
       << "  \"schemaVersion\": " << domain::scientific::ScientificSchema::SchemaVersion << ",\n"
       << "  \"sourceProfile\": {\n"
       << "    \"studyType\": \"theoretical\",\n"
       << "    \"studyTypeCandidates\": [ { \"value\": \"theoretical\", \"confidence\": 0.7 }, { \"value\": \"simulation\", \"confidence\": 0.3 } ],\n"
       << "    \"temporalScale\": \"short\",\n"
       << "    \"ecosystemType\": \"terrestrial\",\n"
       << "    \"evidenceType\": \"theoretical\",\n"
       << "    \"transferability\": \"contextual\",\n"
       << "    \"contextNotes\": \"texto curto ou unknown\",\n"
       << "    \"limitations\": \"texto curto ou unknown\"\n"
       << "  },\n"
       << "  \"narrativeObservations\": [\n"
       << "    {\n"
       << "      \"observation\": \"...\",\n"
       << "      \"context\": \"...\",\n"
       << "      \"limits\": \"...\",\n"
       << "      \"confidence\": \"medium\",\n"
       << "      \"evidence\": \"direct\",\n"
       << "      \"evidenceSnippet\": \"trecho curto do artigo\",\n"
       << "      \"sourceSection\": \"Results\",\n"
       << "      \"pageRange\": \"pp. 3-4\",\n"
       << "      \"contextuality\": \"site-specific\" \n"
       << "    }\n"
       << "  ],\n"
       << "  \"allegedMechanisms\": [\n"
       << "    {\n"
       << "      \"mechanism\": \"...\",\n"
       << "      \"status\": \"inferred\",\n"
       << "      \"context\": \"...\",\n"
       << "      \"limitations\": \"...\",\n"
       << "      \"evidenceSnippet\": \"trecho curto do artigo\",\n"
       << "      \"sourceSection\": \"Discussion\",\n"
       << "      \"pageRange\": \"pp. 5-6\",\n"
       << "      \"contextuality\": \"conditional\" \n"
       << "    }\n"
       << "  ],\n"
       << "  \"temporalWindowReferences\": [\n"
       << "    {\n"
       << "      \"timeWindow\": \"...\",\n"
       << "      \"changeRhythm\": \"...\",\n"
       << "      \"delaysOrHysteresis\": \"...\",\n"
       << "      \"context\": \"...\",\n"
       << "      \"evidenceSnippet\": \"trecho curto do artigo\",\n"
       << "      \"sourceSection\": \"Methods\",\n"
       << "      \"pageRange\": \"pp. 7-9\" \n"
       << "    }\n"
       << "  ],\n"
       << "  \"baselineAssumptions\": [\n"
       << "    {\n"
       << "      \"baselineType\": \"dynamic\",\n"
       << "      \"description\": \"...\",\n"
       << "      \"context\": \"...\"\n"
       << "    }\n"
       << "  ],\n"
       << "  \"trajectoryAnalogies\": [\n"
       << "    {\n"
       << "      \"analogy\": \"...\",\n"
       << "      \"scope\": \"...\",\n"
       << "      \"justification\": \"...\"\n"
       << "    }\n"
       << "  ],\n"
       << "  \"interpretationLayers\": {\n"
       << "    \"observedStatements\": [\"...\"],\n"
        // Force valid empty arrays for other interpretation fields in narrative bundle
       << "    \"authorInterpretations\": [],\n"
       << "    \"possibleReadings\": []\n"
       << "  }\n"
       << "}\n";
    return ss.str();
}

std::string ScientificIngestionService::buildDiscursiveSystemPrompt() const {
    std::ostringstream ss;
    ss << "Você é um analista científico do IdeaWalker.\n"
       << "Objetivo: produzir ARTEFATOS COGNITIVOS DISCURSIVOS (Sistemas de problemas/ações, Frames).\n"
       << "Foque em COMO O AUTOR ARGUMENTA e quais PROBLEMAS/SOLUÇÕES são declarados.\n"
       << "Responda APENAS com JSON válido e estritamente no esquema solicitado.\n"
       << "Campos categóricos DEVEM ter exatamente UM valor permitido. Nunca combine valores com '|'.\n"
       << "Se houver ambiguidade, use 'unknown' e, opcionalmente, inclua <campo>Candidates com {value, confidence 0-1}.\n"
       << "MITIGAÇÃO DE ALUCINAÇÃO: Todo item (Problem, Action, Effect, Frame) DEVE ter 'evidenceSnippet'.\n"
       << "O evidenceSnippet deve ser uma CÓPIA LITERAL do texto. Se não houver evidência explícita, NÃO INCLUA O ITEM.\n";
    return ss.str();
}

std::string ScientificIngestionService::buildDiscursiveUserPrompt(
    const domain::SourceArtifact& artifact,
    const std::string& content) const {
    std::ostringstream ss;
    ss << "ARQUIVO: " << artifact.filename << "\n"
       << "CONTEÚDO DO ARTIGO:\n"
       << "------------------------\n"
       << content << "\n"
       << "------------------------\n\n"
       << "ESQUEMA JSON OBRIGATÓRIO (Discursive Focus):\n"
       << "VALORES PERMITIDOS (campos categóricos):\n"
       << "- valence: normative|descriptive|critical|implicit|unknown\n"
       << "- status (declaredActions): proposed|implemented|unknown\n"
       << "Regra: escolha exatamente UM valor. Nunca combine com '|'.\n"
       << "Se houver ambiguidade, use 'unknown' e (opcional) inclua <campo>Candidates com {value, confidence}.\n"
       << "{\n"
       << "  \"discursiveContext\": {\n"
       << "    \"frames\": [\n"
       << "      {\n"
       << "        \"label\": \"...\",\n"
       << "        \"description\": \"...\",\n"
       << "        \"valence\": \"descriptive\",\n"
       << "        \"valenceCandidates\": [ { \"value\": \"descriptive\", \"confidence\": 0.8 }, { \"value\": \"critical\", \"confidence\": 0.2 } ],\n"
       << "        \"evidenceSnippet\": \"trecho literal...\"\n"
       << "      }\n"
       << "    ],\n"
       << "    \"epistemicRole\": \"discursive-reading\"\n"
       << "  },\n"
       << "  \"discursiveSystem\": {\n"
       << "    \"declaredProblems\": [ { \"statement\": \"...\", \"context\": \"...\", \"evidenceSnippet\": \"trecho literal...\" } ],\n"
       << "    \"declaredActions\": [ { \"statement\": \"...\", \"status\": \"proposed\", \"evidenceSnippet\": \"trecho literal...\" } ],\n"
       << "    \"expectedEffects\": [ { \"statement\": \"...\", \"likelihood\": \"...\", \"evidenceSnippet\": \"trecho literal...\" } ]\n"
       << "  },\n"
       << "  \"interpretationLayers\": {\n"
       << "    \"authorInterpretations\": [\"...\"],\n"
       << "    \"possibleReadings\": [\"...\"]\n"
       << "  }\n"
       << "}\n";
    return ss.str();
}

std::string ScientificIngestionService::buildArtifactId(const domain::SourceArtifact& artifact) const {
    auto now = std::chrono::system_clock::now();
    auto tt = std::chrono::system_clock::to_time_t(now);
    std::stringstream idss;
    idss << std::put_time(std::localtime(&tt), "%Y%m%d_%H%M%S") << "_" << artifact.filename;
    return idss.str();
}

bool ScientificIngestionService::validateBundleJson(const nlohmann::json& bundle, std::vector<std::string>& errors) const {
    if (!bundle.contains("schemaVersion") || !bundle["schemaVersion"].is_number_integer()) {
        errors.push_back("schemaVersion ausente ou inválido");
    } else if (bundle["schemaVersion"].get<int>() != domain::scientific::ScientificSchema::SchemaVersion) {
        errors.push_back("schemaVersion incompatível");
    }

    if (!bundle.contains("sourceProfile") || !bundle["sourceProfile"].is_object()) {
        errors.push_back("sourceProfile ausente ou inválido");
    } else {
        const auto& profile = bundle["sourceProfile"];
        if (!profile.contains("studyType") || !profile["studyType"].is_string() ||
            !IsAllowedValue(profile["studyType"].get<std::string>(), domain::scientific::ScientificSchema::StudyTypes)) {
            errors.push_back("studyType inválido");
        }
        if (!profile.contains("temporalScale") || !profile["temporalScale"].is_string() ||
            !IsAllowedValue(profile["temporalScale"].get<std::string>(), domain::scientific::ScientificSchema::TemporalScales)) {
            errors.push_back("temporalScale inválido");
        }
        if (!profile.contains("ecosystemType") || !profile["ecosystemType"].is_string() ||
            !IsAllowedValue(profile["ecosystemType"].get<std::string>(), domain::scientific::ScientificSchema::EcosystemTypes)) {
            errors.push_back("ecosystemType inválido");
        }
        if (!profile.contains("evidenceType") || !profile["evidenceType"].is_string() ||
            !IsAllowedValue(profile["evidenceType"].get<std::string>(), domain::scientific::ScientificSchema::EvidenceTypes)) {
            errors.push_back("evidenceType inválido");
        }
        if (!profile.contains("transferability") || !profile["transferability"].is_string() ||
            !IsAllowedValue(profile["transferability"].get<std::string>(), domain::scientific::ScientificSchema::TransferabilityLevels)) {
            errors.push_back("transferability inválido");
        }
    }

    if (!bundle.contains("narrativeObservations") || !bundle["narrativeObservations"].is_array()) {
        errors.push_back("narrativeObservations ausente ou inválido");
    }
    if (!bundle.contains("allegedMechanisms") || !bundle["allegedMechanisms"].is_array()) {
        errors.push_back("allegedMechanisms ausente ou inválido");
    } else {
        for (const auto& mech : bundle["allegedMechanisms"]) {
            if (mech.contains("status") && mech["status"].is_string()) {
                if (!IsAllowedValue(mech["status"].get<std::string>(), domain::scientific::ScientificSchema::MechanismStatus)) {
                    errors.push_back("allegedMechanisms.status inválido");
                    break;
                }
            }
        }
    }
    if (!bundle.contains("temporalWindowReferences") || !bundle["temporalWindowReferences"].is_array()) {
        errors.push_back("temporalWindowReferences ausente ou inválido");
    }
    if (!bundle.contains("baselineAssumptions") || !bundle["baselineAssumptions"].is_array()) {
        errors.push_back("baselineAssumptions ausente ou inválido");
    } else {
        for (const auto& baseline : bundle["baselineAssumptions"]) {
            if (baseline.contains("baselineType") && baseline["baselineType"].is_string()) {
                if (!IsAllowedValue(baseline["baselineType"].get<std::string>(), domain::scientific::ScientificSchema::BaselineTypes)) {
                    errors.push_back("baselineAssumptions.baselineType inválido");
                    break;
                }
            }
        }
    }
    if (!bundle.contains("trajectoryAnalogies") || !bundle["trajectoryAnalogies"].is_array()) {
        errors.push_back("trajectoryAnalogies ausente ou inválido");
    }
    if (!bundle.contains("interpretationLayers") || !bundle["interpretationLayers"].is_object()) {
        errors.push_back("interpretationLayers ausente ou inválido");
    } else {
        const auto& layers = bundle["interpretationLayers"];
        if (!layers.contains("observedStatements") || !layers["observedStatements"].is_array()) {
            errors.push_back("interpretationLayers.observedStatements inválido");
        }
        if (!layers.contains("authorInterpretations") || !layers["authorInterpretations"].is_array()) {
            errors.push_back("interpretationLayers.authorInterpretations inválido");
        }
        if (!layers.contains("possibleReadings") || !layers["possibleReadings"].is_array()) {
            errors.push_back("interpretationLayers.possibleReadings inválido");
        }
    }
    
    // Discursive Context (Optional)
    if (bundle.contains("discursiveContext")) {
        const auto& dc = bundle["discursiveContext"];
        if (!dc.is_object()) {
            errors.push_back("discursiveContext deve ser um objeto");
        } else {
            if (dc.contains("frames") && !dc["frames"].is_array()) {
                errors.push_back("discursiveContext.frames deve ser array");
            }
        }
    }
    
    // Discursive System (Optional, but fields must be arrays)
    if (bundle.contains("discursiveSystem")) {
        const auto& ds = bundle["discursiveSystem"];
        if (ds.is_object()) {
            if (ds.contains("declaredProblems") && !ds["declaredProblems"].is_array()) errors.push_back("discursiveSystem.declaredProblems deve ser array");
            if (ds.contains("declaredActions") && !ds["declaredActions"].is_array()) errors.push_back("discursiveSystem.declaredActions deve ser array");
            if (ds.contains("expectedEffects") && !ds["expectedEffects"].is_array()) errors.push_back("discursiveSystem.expectedEffects deve ser array");
        }
    }

    return errors.empty();
}

void ScientificIngestionService::attachSourceMetadata(nlohmann::json& bundle,
                                                      const domain::SourceArtifact& artifact,
                                                      const std::string& artifactId,
                                                      const std::string& method,
                                                      const std::string& sha256) const {
    nlohmann::json source = {
        {"artifactId", artifactId},
        {"path", artifact.path},
        {"filename", artifact.filename},
        {"contentHash", artifact.contentHash},
        {"ingestedAt", ToIsoTimestamp(std::chrono::system_clock::now())},
        {"model", m_aiService ? m_aiService->getCurrentModel() : "unknown"},
        {"extractionMethod", method},
        {"sourceType", SourceTypeToString(artifact.type)},
        {"sizeBytes", artifact.sizeBytes},
        {"lastModified", ToIsoTimestamp(artifact.lastModified)}
    };
    if (!sha256.empty()) {
        source["sourceSha256"] = sha256;
    }
    bundle["source"] = source;
}

bool ScientificIngestionService::saveRawBundle(
    const nlohmann::json& bundle,
    const std::string& artifactId,
    std::string& error) const {
    fs::path outPath = fs::path(m_observationsPath) / (artifactId + ".json");
    return WriteJsonFile(outPath, bundle, error);
}

bool ScientificIngestionService::exportConsumables(
    const nlohmann::json& bundle,
    const std::string& artifactId,
    std::string& error) const {
    if (!bundle.contains("source") || !bundle["source"].is_object()) {
        error = "Bundle sem metadados de fonte para exportação.";
        return false;
    }

    fs::path baseDir = fs::path(m_consumablesPath) / artifactId;
    if (!fs::exists(baseDir)) {
        fs::create_directories(baseDir);
    }

    nlohmann::json baseEnvelope = {
        {"schemaVersion", domain::scientific::ScientificSchema::SchemaVersion},
        {"source", bundle["source"]}
    };

    nlohmann::json sourceProfile = baseEnvelope;
    sourceProfile["sourceProfile"] = bundle["sourceProfile"];
    if (!WriteJsonFile(baseDir / "SourceProfile.json", sourceProfile, error)) return false;
    if (!WriteJsonFile(baseDir / "IWBundle.json", bundle, error)) return false;

    nlohmann::json allegedMechanisms = baseEnvelope;
    allegedMechanisms["allegedMechanisms"] = bundle["allegedMechanisms"];
    if (!WriteJsonFile(baseDir / "AllegedMechanisms.json", allegedMechanisms, error)) return false;

    nlohmann::json temporalWindows = baseEnvelope;
    temporalWindows["temporalWindowReferences"] = bundle["temporalWindowReferences"];
    if (!WriteJsonFile(baseDir / "TemporalWindowReference.json", temporalWindows, error)) return false;

    nlohmann::json baselineAssumptions = baseEnvelope;
    baselineAssumptions["baselineAssumptions"] = bundle["baselineAssumptions"];
    if (!WriteJsonFile(baseDir / "BaselineAssumptions.json", baselineAssumptions, error)) return false;

    nlohmann::json trajectoryAnalogies = baseEnvelope;
    trajectoryAnalogies["trajectoryAnalogies"] = bundle["trajectoryAnalogies"];
    if (!WriteJsonFile(baseDir / "TrajectoryAnalogies.json", trajectoryAnalogies, error)) return false;

    nlohmann::json interpretationLayers = baseEnvelope;
    interpretationLayers["interpretationLayers"] = bundle["interpretationLayers"];
    if (!WriteJsonFile(baseDir / "InterpretationLayers.json", interpretationLayers, error)) return false;

    // Export NarrativeState Candidate Objects
    if (bundle.contains("narrativeObservations")) {
        nlohmann::json narrativeList = nlohmann::json::array();
        for (const auto& obs : bundle["narrativeObservations"]) {
            nlohmann::json stateCandidate;
            stateCandidate["id"] = "candidate_" + ToIsoTimestamp(std::chrono::system_clock::now()); // simplified unique ID
            
            // Map source metadata to STRATA SourceReference keys
            nlohmann::json sourceRef;
            std::string author = "unknown";
            std::string productionDate = "unknown";

            if (bundle.contains("source") && bundle["source"].is_object()) {
                const auto& src = bundle["source"];
                sourceRef["type"] = 3; // 3 = Scientific Article (Candidate Int)
                sourceRef["sourceId"] = src.contains("artifactId") ? src["artifactId"] : "unknown";
                if (src.contains("ingestedAt")) productionDate = src["ingestedAt"];
                // author remains unknown
            } else {
                 sourceRef["type"] = 3;
                 sourceRef["sourceId"] = "unknown";
            }
            sourceRef["productionDate"] = productionDate;
            sourceRef["author"] = author;
            stateCandidate["source"] = sourceRef;
            
            stateCandidate["intent"] = { {"type", 0} }; // 0 = Descriptive Record
            stateCandidate["temporalContext"] = { 
                {"category", 3}, // 3 = Contemporary
                {"label", obs.contains("context") ? obs["context"] : "unknown"} 
            };
            stateCandidate["axes"] = { 
                { 
                    {"label", obs.contains("theme") ? obs["theme"] : "extracted_theme"}, 
                    {"description", obs.contains("observation") ? obs["observation"] : ""}, 
                    {"level", 0} // 0 = Local
                } 
            };
            
            // Move Global Metadata into Item Metadata
            nlohmann::json meta = nlohmann::json::object();
            if (obs.is_object()) {
                for (auto it = obs.begin(); it != obs.end(); ++it) {
                    AddStringField(meta, it.key(), it.value());
                }
            }
            if (bundle.contains("source") && bundle["source"].is_object()) {
                const auto& src = bundle["source"];
                AddStringField(meta, "schemaVersion", std::to_string(domain::scientific::ScientificSchema::SchemaVersion));
                if (src.contains("artifactId")) AddStringField(meta, "artifactId", src["artifactId"]);
                if (src.contains("contentHash")) AddStringField(meta, "contentHash", src["contentHash"]);
                if (src.contains("filename")) AddStringField(meta, "filename", src["filename"]);
                if (src.contains("ingestedAt")) AddStringField(meta, "ingestedAt", src["ingestedAt"]);
                if (src.contains("model")) AddStringField(meta, "model", src["model"]);
                if (src.contains("path")) AddStringField(meta, "path", src["path"]);
            }
            stateCandidate["metadata"] = meta;

            stateCandidate["spatialScope"] = { {"type", 0} }; // 0 = NONE

            narrativeList.push_back(stateCandidate);
        }
        
    if (!narrativeList.empty()) {
        nlohmann::json narrativeEnvelope;
        narrativeEnvelope["history"] = narrativeList;
        std::string validateError;
        if (!ValidateStrataNarrativeEnvelope(narrativeEnvelope, validateError)) {
            error = validateError;
            return false;
        }
        if (!WriteJsonFile(baseDir / "NarrativeObservation.json", narrativeEnvelope, error)) return false;
    }
    }

    if (bundle.contains("discursiveContext")) {
        nlohmann::json discursiveEnvelope;
        discursiveEnvelope["discursiveContext"] = bundle["discursiveContext"];
        if (!WriteJsonFile(baseDir / "DiscursiveContext.json", discursiveEnvelope, error)) return false;
    }

    // Export DiscursiveSystem Candidate Object
    nlohmann::json discursiveSystemCandidate;
    discursiveSystemCandidate["id"] = "ds_candidate_" + artifactId;
    
    // Source References must be an array of objects
    nlohmann::json sourceRefObj;
    if (bundle.contains("source") && bundle["source"].is_object()) {
         const auto& src = bundle["source"];
         sourceRefObj["type"] = 4; // REPORT (Discursive SourceType)
         sourceRefObj["sourceId"] = src.contains("artifactId") ? src["artifactId"] : "unknown";
         sourceRefObj["productionDate"] = src.contains("ingestedAt") ? src["ingestedAt"] : "unknown";
         sourceRefObj["author"] = "unknown";
    } else {
         sourceRefObj["type"] = 4;
         sourceRefObj["sourceId"] = "unknown";
         sourceRefObj["productionDate"] = "unknown";
         sourceRefObj["author"] = "unknown";
    }
    discursiveSystemCandidate["sourceReferences"] = nlohmann::json::array({sourceRefObj});

    discursiveSystemCandidate["temporalContext"] = { {"category", 3}, {"label", "general"} };
    
    nlohmann::json interpMeta = nlohmann::json::object();
    AddStringField(interpMeta, "context", "scientific_ingestion");
    // Inject global metadata here for discursive too
    if (bundle.contains("source") && bundle["source"].is_object()) {
        const auto& src = bundle["source"];
        if (src.contains("filename")) AddStringField(interpMeta, "filename", src["filename"]);
        if (src.contains("model")) AddStringField(interpMeta, "model", src["model"]);
    }

    // Map allegedMechanisms to use 'statement' key and strip non-STRATA fields.
    // Preserve full evidence payload as a JSON string in interpretationMetadata.
    nlohmann::json mechanismsArr = nlohmann::json::array();
    nlohmann::json mechanismsEvidence = nlohmann::json::array();
    if (bundle.contains("allegedMechanisms") && bundle["allegedMechanisms"].is_array()) {
        for (const auto& item : bundle["allegedMechanisms"]) {
             AppendStatementFrom(mechanismsArr, item, {"mechanism"});
             if (item.is_object()) {
                 mechanismsEvidence.push_back(item);
             }
        }
    }
    discursiveSystemCandidate["allegedMechanisms"] = mechanismsArr;

    if (!mechanismsEvidence.empty()) {
        AddStringField(interpMeta, "allegedMechanismsEvidence", mechanismsEvidence.dump());
    }
    if (bundle.contains("discursiveContext") && bundle["discursiveContext"].is_object()) {
        AddStringField(interpMeta, "discursiveContext", bundle["discursiveContext"].dump());
    }
    discursiveSystemCandidate["interpretationMetadata"] = interpMeta;

    discursiveSystemCandidate["declaredProblems"] = nlohmann::json::array();
    discursiveSystemCandidate["declaredActions"] = nlohmann::json::array();
    discursiveSystemCandidate["expectedEffects"] = nlohmann::json::array();

    if (bundle.contains("discursiveSystem")) {
        const auto& ds = bundle["discursiveSystem"];
        if (ds.contains("declaredProblems") && ds["declaredProblems"].is_array()) {
            nlohmann::json out = nlohmann::json::array();
            for (const auto& item : ds["declaredProblems"]) {
                AppendStatementFrom(out, item, {"problem", "declaredProblem"});
            }
            discursiveSystemCandidate["declaredProblems"] = out;
        }
        if (ds.contains("declaredActions") && ds["declaredActions"].is_array()) {
            nlohmann::json out = nlohmann::json::array();
            for (const auto& item : ds["declaredActions"]) {
                AppendStatementFrom(out, item, {"action", "declaredAction"});
            }
            discursiveSystemCandidate["declaredActions"] = out;
        }
        if (ds.contains("expectedEffects") && ds["expectedEffects"].is_array()) {
            nlohmann::json out = nlohmann::json::array();
            for (const auto& item : ds["expectedEffects"]) {
                AppendStatementFrom(out, item, {"effect", "expectedEffect"});
            }
            discursiveSystemCandidate["expectedEffects"] = out;
        }
    }
    
    // Only export if we have meaningful content
    if (!discursiveSystemCandidate["allegedMechanisms"].empty() || 
        !discursiveSystemCandidate["declaredProblems"].empty() ||
        !discursiveSystemCandidate["declaredActions"].empty()) {
            
        nlohmann::json dsEnvelope;
        dsEnvelope["systems"] = nlohmann::json::array({discursiveSystemCandidate});
        std::string validateError;
        if (!ValidateStrataDiscursiveEnvelope(dsEnvelope, validateError)) {
            error = validateError;
            return false;
        }
        if (!WriteJsonFile(baseDir / "DiscursiveSystem.json", dsEnvelope, error)) return false;
    }

    nlohmann::json manifest = baseEnvelope;
    std::vector<std::string> files = {
        "SourceProfile.json",
        "IWBundle.json",
        "AllegedMechanisms.json",
        "TemporalWindowReference.json",
        "BaselineAssumptions.json",
        "TrajectoryAnalogies.json",
        "InterpretationLayers.json"
    };
    if (fs::exists(baseDir / "NarrativeObservation.json")) {
        files.push_back("NarrativeObservation.json");
    }
    if (bundle.contains("discursiveContext")) {
        files.push_back("DiscursiveContext.json");
    }
    // Check if we created DiscursiveSystem.json
    if (fs::exists(baseDir / "DiscursiveSystem.json")) {
        files.push_back("DiscursiveSystem.json");
    }
    // Light metadata for quick indexing
    if (bundle.contains("source") && bundle["source"].is_object()) {
        const auto& src = bundle["source"];
        if (src.contains("artifactId")) manifest["artifactId"] = src["artifactId"];
        if (src.contains("filename")) manifest["filename"] = src["filename"];
        if (src.contains("path")) manifest["sourcePath"] = src["path"];
        if (src.contains("contentHash")) manifest["contentHash"] = src["contentHash"];
        if (src.contains("ingestedAt")) manifest["ingestedAt"] = src["ingestedAt"];
        if (src.contains("model")) manifest["model"] = src["model"];
        if (src.contains("extractionMethod")) manifest["extractionMethod"] = src["extractionMethod"];
        if (src.contains("sourceType")) manifest["sourceType"] = src["sourceType"];
        if (src.contains("sizeBytes")) manifest["sizeBytes"] = src["sizeBytes"];
        if (src.contains("lastModified")) manifest["lastModified"] = src["lastModified"];
        if (src.contains("sourceSha256")) manifest["sourceSha256"] = src["sourceSha256"];
    }
    manifest["files"] = files;
    nlohmann::json fileIndex = nlohmann::json::array();
    for (const auto& name : files) {
        nlohmann::json entry;
        entry["name"] = name;
        entry["path"] = "./" + name;
        fs::path p = baseDir / name;
        entry["exists"] = fs::exists(p);
        if (fs::exists(p)) {
            entry["sizeBytes"] = static_cast<long long>(fs::file_size(p));
        }
        fileIndex.push_back(entry);
    }
    manifest["file_index"] = fileIndex;
    if (!WriteJsonFile(baseDir / "Manifest.json", manifest, error)) return false;

    return true;
}

bool ScientificIngestionService::saveErrorPayload(
    const std::string& artifactId,
    const std::string& payload,
    std::string& error) const {
    fs::path errorDir = fs::path(m_observationsPath) / "errors";
    if (!fs::exists(errorDir)) {
        fs::create_directories(errorDir);
    }
    fs::path outPath = errorDir / (artifactId + ".json");

    nlohmann::json envelope;
    envelope["schemaVersion"] = domain::scientific::ScientificSchema::SchemaVersion;
    envelope["artifactId"] = artifactId;
    envelope["artifactIdBase"] = StripErrorSuffix(artifactId);
    envelope["stage"] = InferErrorStage(artifactId);
    envelope["createdAt"] = ToIsoTimestamp(std::chrono::system_clock::now());

    try {
        nlohmann::json parsed = nlohmann::json::parse(payload);
        envelope["payloadType"] = "json";
        envelope["payload"] = parsed;
    } catch (...) {
        envelope["payloadType"] = "text";
        envelope["payload"] = payload;
    }

    return WriteJsonFile(outPath, envelope, error);
}


void ScientificIngestionService::generateIngestionReport() const {
    if (!fs::exists(m_consumablesPath)) return;

    nlohmann::json manifest;
    auto now = std::chrono::system_clock::now();
    const std::string nowIso = ToIsoTimestamp(now);
    manifest["project_ingestion_id"] = nowIso;
    manifest["generatedAt"] = nowIso;
    manifest["schema_version"] = domain::scientific::ScientificSchema::SchemaVersion;
    manifest["schemaVersion"] = domain::scientific::ScientificSchema::SchemaVersion;

    nlohmann::json layout;
    layout["consumables_root"] = fs::path(m_consumablesPath).string();
    layout["observations_root"] = fs::path(m_observationsPath).string();
    layout["errors_root"] = (fs::path(m_observationsPath) / "errors").string();
    layout["validation_root"] = (fs::path(m_observationsPath) / "validation").string();
    layout["article_dir_pattern"] = "<artifactId>/";
    layout["per_article_files"] = {
        {"required", {
            "SourceProfile.json",
            "IWBundle.json",
            "AllegedMechanisms.json",
            "TemporalWindowReference.json",
            "BaselineAssumptions.json",
            "TrajectoryAnalogies.json",
            "InterpretationLayers.json",
            "Manifest.json"
        }},
        {"optional", {
            "NarrativeObservation.json",
            "DiscursiveContext.json",
            "DiscursiveSystem.json",
            "EpistemicValidationReport.json",
            "ExportSeal.json"
        }}
    };
    manifest["layout"] = layout;

    // Collect error payloads (observations/scientific/errors)
    std::unordered_map<std::string, nlohmann::json> errorsByArtifact;
    nlohmann::json allErrors = nlohmann::json::array();
    fs::path errorDir = fs::path(m_observationsPath) / "errors";
    if (fs::exists(errorDir)) {
        for (const auto& entry : fs::directory_iterator(errorDir)) {
            if (!entry.is_regular_file()) continue;
            const std::string filename = entry.path().filename().string();
            const std::string errId = entry.path().stem().string();
            const std::string baseId = StripErrorSuffix(errId);

            nlohmann::json err;
            err["artifactId"] = errId;
            err["artifactIdBase"] = baseId;
            err["stage"] = InferErrorStage(errId);
            err["path"] = entry.path().string();

            try {
                std::ifstream ef(entry.path());
                nlohmann::json ej;
                ef >> ej;
                if (ej.contains("createdAt")) err["createdAt"] = ej["createdAt"];
                if (ej.contains("payloadType")) err["payloadType"] = ej["payloadType"];
            } catch (...) {
                err["payloadType"] = "unknown";
            }

            allErrors.push_back(err);
            auto& arr = errorsByArtifact[baseId];
            if (!arr.is_array()) arr = nlohmann::json::array();
            arr.push_back(err);
        }
    }
    manifest["errors"] = allErrors;
    
    nlohmann::json articles = nlohmann::json::array();
    
    for (const auto& entry : fs::directory_iterator(m_consumablesPath)) {
        if (!entry.is_directory()) continue;
        
        fs::path dir = entry.path();
        fs::path manifestPath = dir / "Manifest.json";
        
        if (!fs::exists(manifestPath)) continue;

        std::ifstream mf(manifestPath);
        if (!mf.is_open()) continue;
        nlohmann::json artManifest;
        try {
            mf >> artManifest;
        } catch (...) { continue; }

        nlohmann::json articleEntry;
        const std::string dirName = dir.filename().string();
        std::string artifactId = dirName;
        if (artManifest.contains("artifactId") && artManifest["artifactId"].is_string()) {
            artifactId = artManifest["artifactId"].get<std::string>();
        } else if (artManifest.contains("source") && artManifest["source"].is_object()) {
            const auto& src = artManifest["source"];
            if (src.contains("artifactId") && src["artifactId"].is_string()) {
                artifactId = src["artifactId"].get<std::string>();
            }
        }

        articleEntry["artifactId"] = artifactId;
        articleEntry["relative_path"] = "./" + dirName + "/";
        articleEntry["manifest_path"] = "./" + dirName + "/Manifest.json";

        if (artManifest.contains("files")) articleEntry["files"] = artManifest["files"];
        if (artManifest.contains("file_index")) articleEntry["file_index"] = artManifest["file_index"];
        if (artManifest.contains("source") && artManifest["source"].is_object()) {
            articleEntry["source"] = artManifest["source"];
            if (artManifest["source"].contains("filename")) {
                articleEntry["filename"] = artManifest["source"]["filename"];
            }
        }
        if (artManifest.contains("filename")) articleEntry["filename"] = artManifest["filename"];

        // Enrich from SourceProfile.json (nested schema)
        fs::path sourceProfilePath = dir / "SourceProfile.json";
        if (fs::exists(sourceProfilePath)) {
            try {
                std::ifstream spf(sourceProfilePath);
                nlohmann::json sourceProfile;
                spf >> sourceProfile;

                if (!articleEntry.contains("source") && sourceProfile.contains("source") && sourceProfile["source"].is_object()) {
                    articleEntry["source"] = sourceProfile["source"];
                    if (sourceProfile["source"].contains("filename")) {
                        articleEntry["filename"] = sourceProfile["source"]["filename"];
                    }
                }

                if (sourceProfile.contains("sourceProfile") && sourceProfile["sourceProfile"].is_object()) {
                    const auto& sp = sourceProfile["sourceProfile"];
                    if (sp.contains("studyType")) articleEntry["studyType"] = sp["studyType"];
                    if (sp.contains("temporalScale")) articleEntry["temporalScale"] = sp["temporalScale"];
                    if (sp.contains("ecosystemType")) articleEntry["ecosystemType"] = sp["ecosystemType"];
                    if (sp.contains("evidenceType")) articleEntry["evidenceType"] = sp["evidenceType"];
                    if (sp.contains("transferability")) articleEntry["transferability"] = sp["transferability"];
                }
            } catch (...) {}
        }

        // Validation Status + Export Seal
        fs::path validationPath = dir / "EpistemicValidationReport.json";
        if (fs::exists(validationPath)) {
            try {
                std::ifstream vf(validationPath);
                nlohmann::json validation;
                vf >> validation;
                if (validation.contains("status")) {
                    articleEntry["validationStatus"] = validation["status"];
                } else {
                    articleEntry["validationStatus"] = "unknown";
                }
                articleEntry["validationReportPath"] = "./" + dirName + "/EpistemicValidationReport.json";
            } catch (...) {
                articleEntry["validationStatus"] = "error";
            }
        } else {
            articleEntry["validationStatus"] = "pending";
        }

        fs::path sealPath = dir / "ExportSeal.json";
        if (fs::exists(sealPath)) {
            try {
                std::ifstream sf(sealPath);
                nlohmann::json seal;
                sf >> seal;
                if (seal.contains("exportAllowed")) articleEntry["exportAllowed"] = seal["exportAllowed"];
                articleEntry["exportSealPath"] = "./" + dirName + "/ExportSeal.json";
            } catch (...) {}
        }

        // Observation bundle + validation paths (if present)
        fs::path rawBundlePath = fs::path(m_observationsPath) / (artifactId + ".json");
        if (fs::exists(rawBundlePath)) {
            articleEntry["rawBundlePath"] = rawBundlePath.string();
        }
        fs::path obsValidationPath = fs::path(m_observationsPath) / "validation" / (artifactId + ".json");
        if (fs::exists(obsValidationPath)) {
            articleEntry["rawValidationPath"] = obsValidationPath.string();
        }

        // Attach error payloads if any
        auto it = errorsByArtifact.find(artifactId);
        if (it != errorsByArtifact.end()) {
            articleEntry["errors"] = it->second;
        }

        articles.push_back(articleEntry);
    }
    
    manifest["total_articles"] = articles.size();
    manifest["articles"] = articles;
    
    // Write STRATA_Manifest.json
    fs::path outPath = fs::path(m_consumablesPath) / "STRATA_Manifest.json";
    std::ofstream outFile(outPath);
    if (outFile.is_open()) {
        outFile << manifest.dump(4);
    }
}

} // namespace ideawalker::application::scientific
