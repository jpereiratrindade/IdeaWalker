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

std::string ScientificIngestionService::buildSystemPrompt() const {
    std::ostringstream ss;
    ss << "Você é um analista científico do IdeaWalker.\n"
       << "Objetivo: produzir ARTEFATOS COGNITIVOS explícitos, sem recomendações e sem normatividade.\n"
       << "Responda APENAS com JSON válido e estritamente no esquema solicitado.\n"
       << "Não inclua texto fora do JSON.\n"
       << "Todo item em narrativeObservations, allegedMechanisms e temporalWindowReferences deve conter evidenceSnippet, sourceSection e pageRange.\n"
       << "evidenceSnippet deve ser TRECHO LITERAL do artigo (copiado do texto extraído).\n"
       << "Todo item em narrativeObservations e allegedMechanisms deve declarar contextuality.\n"
       << "Se algo não puder ser inferido, use valores \"unknown\" ou listas vazias.\n";
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

    nlohmann::json narrative = baseEnvelope;
    narrative["narrativeObservations"] = bundle["narrativeObservations"];
    if (!WriteJsonFile(baseDir / "NarrativeObservation.json", narrative, error)) return false;

    nlohmann::json mechanisms = baseEnvelope;
    mechanisms["allegedMechanisms"] = bundle["allegedMechanisms"];
    if (!WriteJsonFile(baseDir / "AllegedMechanisms.json", mechanisms, error)) return false;

    nlohmann::json temporal = baseEnvelope;
    temporal["temporalWindowReferences"] = bundle["temporalWindowReferences"];
    if (!WriteJsonFile(baseDir / "TemporalWindowReference.json", temporal, error)) return false;

    nlohmann::json baseline = baseEnvelope;
    baseline["baselineAssumptions"] = bundle["baselineAssumptions"];
    if (!WriteJsonFile(baseDir / "BaselineAssumptions.json", baseline, error)) return false;

    nlohmann::json analogies = baseEnvelope;
    analogies["trajectoryAnalogies"] = bundle["trajectoryAnalogies"];
    if (!WriteJsonFile(baseDir / "TrajectoryAnalogies.json", analogies, error)) return false;

    nlohmann::json layers = baseEnvelope;
    layers["interpretationLayers"] = bundle["interpretationLayers"];
    if (!WriteJsonFile(baseDir / "InterpretationLayers.json", layers, error)) return false;

    nlohmann::json manifest = baseEnvelope;
    manifest["files"] = {
        "SourceProfile.json",
        "NarrativeObservation.json",
        "AllegedMechanisms.json",
        "TemporalWindowReference.json",
        "BaselineAssumptions.json",
        "TrajectoryAnalogies.json",
        "InterpretationLayers.json"
    };
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
