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

bool SnippetAppearsInContent(const std::string& snippet, const std::string& content) {
    if (snippet.empty()) return false;
    const std::string normSnippet = NormalizeForSearch(snippet);
    const std::string normContent = NormalizeForSearch(content);
    return normContent.find(normSnippet) != std::string::npos;
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
    if (bundle.contains("narrativeObservations")) {
        FilterByAnchoring(bundle["narrativeObservations"], {"evidenceSnippet", "sourceSection", "pageRange"}, content);
    }
    if (bundle.contains("allegedMechanisms")) {
        FilterByAnchoring(bundle["allegedMechanisms"], {"evidenceSnippet", "sourceSection", "pageRange"}, content);
    }
    if (bundle.contains("temporalWindowReferences")) {
        FilterByAnchoring(bundle["temporalWindowReferences"], {"evidenceSnippet", "sourceSection", "pageRange"}, content);
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
}

ScientificIngestionService::IngestionResult ScientificIngestionService::ingestPending(
    std::function<void(std::string)> statusCallback) {
    IngestionResult result{0, 0, {}};

    if (statusCallback) statusCallback("Varrendo inbox científica...");
    auto artifacts = m_scanner->scan();
    result.artifactsDetected = static_cast<int>(artifacts.size());

    for (const auto& artifact : artifacts) {
        if (!m_aiService) {
            result.errors.push_back("Serviço de IA não configurado para ingestão científica.");
            break;
        }
        if (statusCallback) statusCallback("Processando artigo: " + artifact.filename);

        std::string content = infrastructure::ContentExtractor::Extract(artifact.path);

        const std::string systemPrompt = buildSystemPrompt();
        const std::string userPrompt = buildUserPrompt(artifact, content);

        const std::string artifactId = buildArtifactId(artifact);

        auto response = m_aiService->generateJson(systemPrompt, userPrompt);
        if (!response) {
            result.errors.push_back("Falha na IA para: " + artifact.filename);
            continue;
        }

        nlohmann::json bundle;
        try {
            bundle = nlohmann::json::parse(*response);
        } catch (const std::exception& e) {
            std::string err = std::string("JSON inválido para ") + artifact.filename + ": " + e.what();
            result.errors.push_back(err);
            std::string saveError;
            saveErrorPayload(artifactId, *response, saveError);
            continue;
        }

        SanitizeBundleAnchoring(bundle, content);

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

        attachSourceMetadata(bundle, artifact, artifactId);

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
        if (!WriteJsonFile(validationDir / (artifactId + ".json"), validation.report, validationError)) {
            result.errors.push_back(validationError);
            continue;
        }
        if (!validation.exportAllowed) {
            result.errors.push_back("Exportação bloqueada pelo Validador Epistemológico: " + artifact.filename);
            continue;
        }

        if (!exportConsumables(bundle, artifactId, saveError)) {
            result.errors.push_back(saveError);
            continue;
        }

        fs::path consumableDir = fs::path(m_consumablesPath) / artifactId;
        if (!WriteJsonFile(consumableDir / "EpistemicValidationReport.json", validation.report, saveError)) {
            result.errors.push_back(saveError);
            continue;
        }
        if (!WriteJsonFile(consumableDir / "ExportSeal.json", validation.seal, saveError)) {
            result.errors.push_back(saveError);
            continue;
        }

        result.bundlesGenerated++;
    }

    return result;
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

std::string ScientificIngestionService::buildSystemPrompt() const {
    std::ostringstream ss;
    ss << "Você é um analista científico do IdeaWalker.\n"
       << "Objetivo: produzir ARTEFATOS COGNITIVOS explícitos, sem recomendações e sem normatividade.\n"
       << "Responda APENAS com JSON válido e estritamente no esquema solicitado.\n"
       << "Não inclua texto fora do JSON.\n"
       << "Todo item em narrativeObservations, allegedMechanisms e temporalWindowReferences deve conter evidenceSnippet, sourceSection e pageRange.\n"
       << "evidenceSnippet deve ser TRECHO LITERAL do artigo (copiado do texto extraído).\n"
       << "Todo item em narrativeObservations e allegedMechanisms deve declarar contextuality.\n"
       << "Se algo não puder ser inferido, use valores \"unknown\" ou listas vazias.\n"
       << "\n"
       << "EXTRAÇÃO DISCURSIVA (OPCIONAL):\n"
       << "Além da extração narrativa (o que aconteceu), identifique frames discursivos (como o autor fala).\n"
       << "Separe claramente o que é observação factual do que é enquadramento retórico ou normativo.\n";
    return ss.str();
}

std::string ScientificIngestionService::buildUserPrompt(
    const domain::SourceArtifact& artifact,
    const std::string& content) const {
    std::ostringstream ss;
    ss << "ARQUIVO: " << artifact.filename << "\n"
       << "TIPO: " << (artifact.type == domain::SourceType::PDF ? "PDF" :
                       artifact.type == domain::SourceType::LaTeX ? "LaTeX" :
                       artifact.type == domain::SourceType::Markdown ? "Markdown" : "Texto")
       << "\n\n"
       << "CONTEÚDO DO ARTIGO:\n"
       << "------------------------\n"
       << content << "\n"
       << "------------------------\n\n"
       << "ESQUEMA JSON OBRIGATÓRIO (schemaVersion=" << domain::scientific::ScientificSchema::SchemaVersion << "):\n"
       << "{\n"
       << "  \"schemaVersion\": " << domain::scientific::ScientificSchema::SchemaVersion << ",\n"
       << "  \"sourceProfile\": {\n"
       << "    \"studyType\": \"experimental|observational|review|theoretical|simulation|mixed|unknown\",\n"
       << "    \"temporalScale\": \"short|medium|long|multi|unknown\",\n"
       << "    \"ecosystemType\": \"terrestrial|aquatic|urban|agro|industrial|social|digital|mixed|unknown\",\n"
       << "    \"evidenceType\": \"empirical|theoretical|mixed|unknown\",\n"
       << "    \"transferability\": \"high|medium|low|contextual|unknown\",\n"
       << "    \"contextNotes\": \"texto curto ou unknown\",\n"
       << "    \"limitations\": \"texto curto ou unknown\"\n"
       << "  },\n"
       << "  \"narrativeObservations\": [\n"
       << "    {\n"
       << "      \"observation\": \"...\",\n"
       << "      \"context\": \"...\",\n"
       << "      \"limits\": \"...\",\n"
       << "      \"confidence\": \"low|medium|high|unknown\",\n"
       << "      \"evidence\": \"direct|inferred|unknown\",\n"
       << "      \"evidenceSnippet\": \"trecho curto do artigo\",\n"
       << "      \"sourceSection\": \"Results|Discussion|Methods|Unknown\",\n"
       << "      \"pageRange\": \"pp. 3-4\",\n"
       << "      \"contextuality\": \"site-specific|conditional|comparative|non-universal\" \n"
       << "    }\n"
       << "  ],\n"
       << "  \"allegedMechanisms\": [\n"
       << "    {\n"
       << "      \"mechanism\": \"...\",\n"
       << "      \"status\": \"tested|inferred|speculative|unknown\",\n"
       << "      \"context\": \"...\",\n"
       << "      \"limitations\": \"...\",\n"
       << "      \"evidenceSnippet\": \"trecho curto do artigo\",\n"
       << "      \"sourceSection\": \"Results|Discussion|Methods|Unknown\",\n"
       << "      \"pageRange\": \"pp. 5-6\",\n"
       << "      \"contextuality\": \"site-specific|conditional|comparative|non-universal\" \n"
       << "    }\n"
       << "  ],\n"
       << "  \"temporalWindowReferences\": [\n"
       << "    {\n"
       << "      \"timeWindow\": \"...\",\n"
       << "      \"changeRhythm\": \"...\",\n"
       << "      \"delaysOrHysteresis\": \"...\",\n"
       << "      \"context\": \"...\",\n"
       << "      \"evidenceSnippet\": \"trecho curto do artigo\",\n"
       << "      \"sourceSection\": \"Results|Discussion|Methods|Unknown\",\n"
       << "      \"pageRange\": \"pp. 7-9\" \n"
       << "    }\n"
       << "  ],\n"
       << "  \"baselineAssumptions\": [\n"
       << "    {\n"
       << "      \"baselineType\": \"fixed|dynamic|multiple|none|unknown\",\n"
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
       << "    \"authorInterpretations\": [\"...\"],\n"
       << "    \"possibleReadings\": [\"...\"]\n"
       << "    \"possibleReadings\": [\"...\"]\n"
       << "  },\n"
       << "  \"discursiveContext\": {\n"
       << "    \"frames\": [\n"
       << "      {\n"
       << "        \"label\": \"...\",\n"
       << "        \"description\": \"...\",\n"
       << "        \"valence\": \"normative|descriptive|critical|implicit\",\n"
       << "        \"evidenceSnippet\": \"...\"\n"
       << "      }\n"
       << "    ],\n"
       << "    \"epistemicRole\": \"discursive-reading\"\n"
       << "  },\n"
       << "  \"discursiveSystem\": {\n"
       << "    \"declaredProblems\": [ { \"statement\": \"...\", \"context\": \"...\" } ],\n"
       << "    \"declaredActions\": [ { \"statement\": \"...\", \"status\": \"proposed|implemented\" } ],\n"
       << "    \"expectedEffects\": [ { \"statement\": \"...\", \"likelihood\": \"...\" } ]\n"
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
                                                      const std::string& artifactId) const {
    nlohmann::json source = {
        {"artifactId", artifactId},
        {"path", artifact.path},
        {"filename", artifact.filename},
        {"contentHash", artifact.contentHash},
        {"ingestedAt", ToIsoTimestamp(std::chrono::system_clock::now())},
        {"model", m_aiService ? m_aiService->getCurrentModel() : "unknown"}
    };
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
                    {"label", "extracted_theme"}, 
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
    manifest["files"] = files;
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
    fs::path outPath = errorDir / (artifactId + ".txt");
    std::ofstream file(outPath);
    if (!file.is_open()) {
        error = "Falha ao salvar payload de erro: " + outPath.string();
        return false;
    }
    file << payload;
    return true;
}

} // namespace ideawalker::application::scientific
