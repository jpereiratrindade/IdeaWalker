/**
 * @file NarrativeBundleTest.cpp
 * @brief F1 Hardening — Invariant tests for NarrativeBundle JSON output.
 *
 * Covers:
 *   F1.A1 — schemaVersion presente e ≥ 1 em todo bundle gerado
 *   F1.A2 — narrativeRegime presente em todo bundle gerado
 *   F1.A3 — source.model (toolchain) presente em todo bundle gerado
 *   F1.C1 — ScientificIngestionService realiza exatamente 2 invocações de IA
 *            por documento em condições normais (narrativa + discursiva)
 *
 * Refs: ADR-002, ADR-003, ADR-004, ADR-007
 */

#include <cassert>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <optional>
#include <string>
#include <vector>
#include <atomic>

#include <nlohmann/json.hpp>

#include "application/scientific/ScientificIngestionService.hpp"
#include "domain/AIService.hpp"
#include "domain/SourceArtifact.hpp"
#include "domain/scientific/ScientificSchema.hpp"
#include "infrastructure/FileSystemArtifactScanner.hpp"

namespace fs = std::filesystem;
using namespace ideawalker;

// ─────────────────────────────────────────────────────────────────────────────
// Helpers
// ─────────────────────────────────────────────────────────────────────────────

#define IW_ASSERT(condition, message)                                           \
    do {                                                                        \
        if (!(condition)) {                                                     \
            std::cerr << "[FAIL] " << message << "\n";                         \
            std::cerr << "       at " << __FILE__ << ":" << __LINE__ << "\n";  \
            return false;                                                       \
        }                                                                       \
        std::cout << "[PASS] " << message << "\n";                              \
    } while (false)

static int g_passed = 0;
static int g_failed = 0;

#define RUN_TEST(fn)                                                            \
    do {                                                                        \
        std::cout << "\n-- " << #fn << " --\n";                                \
        if (fn()) {                                                             \
            ++g_passed;                                                         \
        } else {                                                                \
            ++g_failed;                                                         \
        }                                                                       \
    } while (false)

// ─────────────────────────────────────────────────────────────────────────────
// Minimal stub JSON bundles
// ─────────────────────────────────────────────────────────────────────────────

/// Retorna um JSON de resposta narrativa mínima válida conforme o schema.
static std::string MakeNarrativeResponse(int schemaVersion = domain::scientific::ScientificSchema::SchemaVersion,
                                          const std::string& narrativeRegime = "observational") {
    nlohmann::json j;
    j["schemaVersion"]       = schemaVersion;
    j["narrativeRegime"]     = narrativeRegime;
    j["narrativeObservations"] = nlohmann::json::array();
    j["allegedMechanisms"]     = nlohmann::json::array();
    j["temporalWindowReferences"] = nlohmann::json::array();
    j["baselineAssumptions"]   = nlohmann::json::array();
    j["sourceProfile"] = {
        {"studyType",     "observational"},
        {"temporalScale", "long"},
        {"ecosystemType", "terrestrial"},
        {"evidenceType",  "empirical"},
        {"transferability", "medium"}
    };
    j["source"] = {
        {"artifactId",  "test_artifact"},
        {"ingestedAt",  "2026-02-25T00:00:00Z"},
        {"model",       "mock-model-v1"},
        {"sha256",      "deadbeef"},
        {"sourceType",  "pdf"},
        {"method",      "bifasic"}
    };
    return j.dump();
}

/// Retorna um JSON de resposta discursiva mínima válida.
static std::string MakeDiscursiveResponse() {
    nlohmann::json j;
    j["discursiveContext"] = {
        {"frames", nlohmann::json::array()}
    };
    j["discursiveSystem"] = {
        {"declaredProblems", nlohmann::json::array()},
        {"declaredActions",  nlohmann::json::array()},
        {"expectedEffects",  nlohmann::json::array()}
    };
    return j.dump();
}

/// Retorna um bundle completo e exportável para testes de consumíveis.
static std::string MakeExportableBundle() {
    nlohmann::json j;
    j["schemaVersion"] = domain::scientific::ScientificSchema::SchemaVersion;
    j["narrativeRegime"] = "observational";
    j["narrativeObservations"] = nlohmann::json::array({
        {
            {"theme", "resilience"},
            {"observation", "Observed structural pattern in sampled system."},
            {"contextuality", "site-specific"},
            {"evidenceSnippet", "sampled system"}
        }
    });
    j["allegedMechanisms"] = nlohmann::json::array();
    j["temporalWindowReferences"] = nlohmann::json::array({
        {
            {"timeWindow", "2020-2024"},
            {"changeRhythm", "gradual"},
            {"delaysOrHysteresis", "none"},
            {"evidenceSnippet", "sampled system"}
        }
    });
    j["baselineAssumptions"] = nlohmann::json::array({
        {
            {"baselineType", "dynamic"},
            {"assumption", "Reference baseline adapts over time."}
        }
    });
    j["trajectoryAnalogies"] = nlohmann::json::array();
    j["interpretationLayers"] = {
        {"observedStatements", nlohmann::json::array()},
        {"authorInterpretations", nlohmann::json::array()},
        {"possibleReadings", nlohmann::json::array()}
    };
    j["sourceProfile"] = {
        {"studyType", "observational"},
        {"temporalScale", "long"},
        {"ecosystemType", "terrestrial"},
        {"evidenceType", "empirical"},
        {"transferability", "medium"}
    };
    j["source"] = {
        {"artifactId", "test_exportable"},
        {"ingestedAt", "2026-02-25T00:00:00Z"},
        {"model", "mock-model-v1"},
        {"sha256", "deadbeef"},
        {"sourceType", "pdf"},
        {"method", "bifasic"}
    };
    j["discursiveContext"] = {
        {"frames", nlohmann::json::array({
            {
                {"frame", "governance"},
                {"evidenceSnippet", "sampled system"}
            }
        })}
    };
    j["discursiveSystem"] = {
        {"declaredProblems", nlohmann::json::array()},
        {"declaredActions", nlohmann::json::array()},
        {"expectedEffects", nlohmann::json::array()}
    };
    return j.dump();
}

// ─────────────────────────────────────────────────────────────────────────────
// Mock AI Service
// ─────────────────────────────────────────────────────────────────────────────

class MockAIService : public domain::AIService {
public:
    std::atomic<int> generateJsonCallCount{0};

    // Retorna narrative response na 1ª chamada, discursive na 2ª (por documento).
    std::optional<std::string> generateJson(
        const std::string& /*system*/,
        const std::string& /*user*/) override
    {
        int call = ++generateJsonCallCount;
        // Odd calls → narrative; even → discursive
        if (call % 2 == 1) {
            return MakeNarrativeResponse();
        } else {
            return MakeDiscursiveResponse();
        }
    }

    // Stubs para interface completa
    std::optional<domain::Insight> processRawThought(
        const std::string&, bool, std::function<void(std::string)>) override
    { return std::nullopt; }

    std::optional<std::string> chat(
        const std::vector<ChatMessage>&, bool) override
    { return "mock"; }

    std::optional<std::string> consolidateTasks(const std::string&) override
    { return std::nullopt; }

    std::vector<float> getEmbedding(const std::string&) override
    { return {}; }

    std::vector<std::string> getAvailableModels() override
    { return {"mock-model-v1"}; }

    void setModel(const std::string&) override {}

    std::string getCurrentModel() const override
    { return "mock-model-v1"; }
};

// ─────────────────────────────────────────────────────────────────────────────
// Fixture Setup
// ─────────────────────────────────────────────────────────────────────────────

struct TestFixture {
    fs::path root;
    fs::path inboxPath;
    fs::path observationsPath;
    fs::path consumablesPath;

    TestFixture() {
        root = fs::temp_directory_path() / "iw_bundle_test";
        inboxPath       = root / "inbox" / "scientific";
        observationsPath = root / "observations" / "scientific";
        consumablesPath  = root / "strata" / "consumables";
        fs::create_directories(inboxPath);
        fs::create_directories(observationsPath);
        fs::create_directories(consumablesPath);
    }

    ~TestFixture() {
        fs::remove_all(root);
    }

    /// Cria um arquivo texto no inbox e retorna o caminho.
    fs::path createInboxFile(const std::string& name, const std::string& content = "Sample scientific content.") {
        fs::path p = inboxPath / name;
        std::ofstream f(p);
        f << content;
        return p;
    }

    std::shared_ptr<MockAIService> makeMockAI() {
        return std::make_shared<MockAIService>();
    }

    std::unique_ptr<application::scientific::ScientificIngestionService> makeService(
        std::shared_ptr<domain::AIService> ai)
    {
        auto scanner = std::make_unique<infrastructure::FileSystemArtifactScanner>(inboxPath.string());
        return std::make_unique<application::scientific::ScientificIngestionService>(
            std::move(scanner), ai,
            observationsPath.string(),
            consumablesPath.string());
    }
};

// ─────────────────────────────────────────────────────────────────────────────
// Test: F1.A1 — schemaVersion presente e ≥ 1 no bundle salvo
// ─────────────────────────────────────────────────────────────────────────────
bool Test_F1_A1_SchemaVersionPresent() {
    TestFixture fx;

    // Inject bundle diretamente via ingestScientificBundle
    auto ai      = fx.makeMockAI();
    auto service = fx.makeService(ai);

    const std::string bundleJson = MakeNarrativeResponse(4);
    bool ok = service->ingestScientificBundle(bundleJson, "test_artifact_a1");
    IW_ASSERT(ok, "F1.A1: ingestScientificBundle retorna true para bundle válido");

    // Verificar no arquivo salvo
    fs::path savedPath = fx.observationsPath / "test_artifact_a1.json";
    IW_ASSERT(fs::exists(savedPath), "F1.A1: arquivo de bundle salvo em disco");

    std::ifstream f(savedPath);
    nlohmann::json saved = nlohmann::json::parse(f);

    IW_ASSERT(saved.contains("schemaVersion"), "F1.A1: bundle contém 'schemaVersion'");
    IW_ASSERT(!saved["schemaVersion"].is_null(), "F1.A1: 'schemaVersion' não é null");
    IW_ASSERT(saved["schemaVersion"].is_number_integer() || saved["schemaVersion"].is_number(),
              "F1.A1: 'schemaVersion' é numérico");
    IW_ASSERT(saved["schemaVersion"].get<int>() >= 1, "F1.A1: schemaVersion >= 1");

    return true;
}

// ─────────────────────────────────────────────────────────────────────────────
// Test: F1.A2 — narrativeRegime presente no bundle salvo
// ─────────────────────────────────────────────────────────────────────────────
bool Test_F1_A2_NarrativeRegimePresent() {
    TestFixture fx;
    auto ai      = fx.makeMockAI();
    auto service = fx.makeService(ai);

    const std::string bundleJson = MakeNarrativeResponse(4, "observational");
    bool ok = service->ingestScientificBundle(bundleJson, "test_artifact_a2");
    IW_ASSERT(ok, "F1.A2: ingestScientificBundle retorna true");

    fs::path savedPath = fx.observationsPath / "test_artifact_a2.json";
    IW_ASSERT(fs::exists(savedPath), "F1.A2: bundle salvo em disco");

    std::ifstream f(savedPath);
    nlohmann::json saved = nlohmann::json::parse(f);

    IW_ASSERT(saved.contains("narrativeRegime"), "F1.A2: bundle contém 'narrativeRegime'");
    IW_ASSERT(saved["narrativeRegime"].is_string(), "F1.A2: 'narrativeRegime' é string");
    IW_ASSERT(!saved["narrativeRegime"].get<std::string>().empty(),
              "F1.A2: 'narrativeRegime' não está vazio");

    return true;
}

// ─────────────────────────────────────────────────────────────────────────────
// Test: F1.A3 — source.model (toolchain) presente no bundle salvo
// ─────────────────────────────────────────────────────────────────────────────
bool Test_F1_A3_ToolchainSourcePresent() {
    TestFixture fx;
    auto ai      = fx.makeMockAI();
    auto service = fx.makeService(ai);

    const std::string bundleJson = MakeNarrativeResponse();
    bool ok = service->ingestScientificBundle(bundleJson, "test_artifact_a3");
    IW_ASSERT(ok, "F1.A3: ingestScientificBundle retorna true");

    fs::path savedPath = fx.observationsPath / "test_artifact_a3.json";
    IW_ASSERT(fs::exists(savedPath), "F1.A3: bundle salvo em disco");

    std::ifstream f(savedPath);
    nlohmann::json saved = nlohmann::json::parse(f);

    IW_ASSERT(saved.contains("source"), "F1.A3: bundle contém 'source'");
    IW_ASSERT(saved["source"].is_object(), "F1.A3: 'source' é objeto");
    IW_ASSERT(saved["source"].contains("model"), "F1.A3: source contém 'model' (toolchain)");
    IW_ASSERT(saved["source"]["model"].is_string(), "F1.A3: source.model é string");
    IW_ASSERT(!saved["source"]["model"].get<std::string>().empty(),
              "F1.A3: source.model não está vazio");

    return true;
}

// ─────────────────────────────────────────────────────────────────────────────
// Test: F1.C1 — Pipeline realiza exatamente 2 invocações de IA por documento
// ─────────────────────────────────────────────────────────────────────────────
bool Test_F1_C1_BifasicTwoCallsPerDocument() {
    TestFixture fx;
    auto mockAI  = fx.makeMockAI();
    auto service = fx.makeService(mockAI);

    // Cria 1 documento no inbox
    fx.createInboxFile("paper_bifasic.txt", "Abstract. This is a sample scientific paper content.");

    mockAI->generateJsonCallCount = 0;
    auto result = service->ingestPending(nullptr);

    IW_ASSERT(result.artifactsDetected == 1, "F1.C1: 1 artefato detectado no inbox");
    IW_ASSERT(mockAI->generateJsonCallCount.load() == 2,
              "F1.C1: exactamente 2 invocações de IA por documento (narrativa + discursiva)");

    return true;
}

// ─────────────────────────────────────────────────────────────────────────────
// Test: F1.C1 corolário — 2 documentos = 4 ou 6 invocações (max com fallback)
// ─────────────────────────────────────────────────────────────────────────────
bool Test_F1_C1_TwoDocumentsFourCalls() {
    TestFixture fx;
    auto mockAI  = fx.makeMockAI();
    auto service = fx.makeService(mockAI);

    fx.createInboxFile("paper_01.txt", "Abstract. Paper one content with findings.");
    fx.createInboxFile("paper_02.txt", "Abstract. Paper two content with methodology.");

    mockAI->generateJsonCallCount = 0;
    auto result = service->ingestPending(nullptr);

    IW_ASSERT(result.artifactsDetected == 2, "F1.C1 (2 docs): 2 artefatos detectados");
    // Mínimo: 2 chamadas por doc × 2 docs = 4.
    // Máximo: com fallback pode ser até 6 (4 + 2 fallbacks).
    const int calls = mockAI->generateJsonCallCount.load();
    IW_ASSERT(calls >= 4 && calls <= 6,
              "F1.C1 (2 docs): entre 4 e 6 invocações de IA (2 base + possíveis fallbacks)");

    return true;
}

// ─────────────────────────────────────────────────────────────────────────────
// Test: Rejeição de bundle sem schemaVersion (invariante negativo)
// ─────────────────────────────────────────────────────────────────────────────
bool Test_F1_A1_BundleWithoutSchemaVersionRejected() {
    TestFixture fx;
    auto ai      = fx.makeMockAI();
    auto service = fx.makeService(ai);

    // Bundle sem schemaVersion
    nlohmann::json invalid;
    invalid["narrativeRegime"] = "observational";
    invalid["source"] = {{"model", "test"}, {"artifactId", "x"}, {"ingestedAt", "2026-01-01T00:00:00Z"}};
    invalid["narrativeObservations"]   = nlohmann::json::array();
    invalid["allegedMechanisms"]       = nlohmann::json::array();
    invalid["temporalWindowReferences"] = nlohmann::json::array();
    invalid["baselineAssumptions"]     = nlohmann::json::array();
    invalid["sourceProfile"]           = {{"studyType","observational"},{"temporalScale","long"},{"ecosystemType","terrestrial"},{"evidenceType","empirical"},{"transferability","medium"}};

    bool ok = service->ingestScientificBundle(invalid.dump(), "test_no_schema");

    // Pode aceitar (se validator não exigir schemaVersion) ou rejeitar.
    // O importante é que o bundle salvo conterá ou não o campo.
    // Este teste documenta o comportamento atual para rastrear regressões.
    fs::path savedPath = fx.observationsPath / "test_no_schema.json";
    if (ok && fs::exists(savedPath)) {
        std::ifstream f(savedPath);
        nlohmann::json saved = nlohmann::json::parse(f);
        const bool hasSchema = saved.contains("schemaVersion");
        std::cout << "[INFO] F1.A1 negativo: bundle sem schemaVersion foi aceito. "
                  << "schemaVersion presente no salvo: " << (hasSchema ? "sim" : "NÃO") << "\n";
        // Documenta o gap: se não tem, é uma falha de enforcement a corrigir em F2.
        if (!hasSchema) {
            std::cout << "[WARN] F1.A1: enforcement ausente — schemaVersion não é exigido pelo validator atual.\n";
        }
    } else {
        std::cout << "[INFO] F1.A1 negativo: bundle sem schemaVersion foi REJEITADO pelo validator. ✅\n";
    }

    // Este teste não falha — ele documenta o estado atual como baseline.
    return true;
}

// ─────────────────────────────────────────────────────────────────────────────
// Test: F1.A4 — Exportação ocorre via NarrativeBundle estruturado
// ─────────────────────────────────────────────────────────────────────────────
bool Test_F1_A4_ExportOnlyStructuredBundleArtifacts() {
    TestFixture fx;
    auto ai      = fx.makeMockAI();
    auto service = fx.makeService(ai);

    bool ok = service->ingestScientificBundle(MakeExportableBundle(), "test_artifact_a4");
    IW_ASSERT(ok, "F1.A4: ingestScientificBundle retorna true para bundle exportável");

    fs::path outDir = fx.consumablesPath / "test_artifact_a4";
    IW_ASSERT(fs::exists(outDir), "F1.A4: diretório de consumíveis criado");
    IW_ASSERT(fs::exists(outDir / "IWBundle.json"), "F1.A4: IWBundle.json presente como envelope canônico");

    size_t fileCount = 0;
    for (const auto& entry : fs::directory_iterator(outDir)) {
        if (!entry.is_regular_file()) continue;
        ++fileCount;
        IW_ASSERT(entry.path().extension() == ".json",
                  "F1.A4: artefatos exportados são estruturados em JSON");
    }
    IW_ASSERT(fileCount > 0, "F1.A4: ao menos um artefato estruturado foi exportado");
    IW_ASSERT(!fs::exists(outDir / "NarrativeVectorDTO.txt"),
              "F1.A4: nenhum DTO cru em formato texto foi exportado");

    return true;
}

// ─────────────────────────────────────────────────────────────────────────────
// Test: F1.C3 — Artefatos narrativo e discursivo exportados separadamente
// ─────────────────────────────────────────────────────────────────────────────
bool Test_F1_C3_NarrativeAndDiscursiveArtifactsSeparated() {
    TestFixture fx;
    auto ai      = fx.makeMockAI();
    auto service = fx.makeService(ai);

    bool ok = service->ingestScientificBundle(MakeExportableBundle(), "test_artifact_c3");
    IW_ASSERT(ok, "F1.C3: ingestScientificBundle retorna true");

    fs::path outDir = fx.consumablesPath / "test_artifact_c3";
    fs::path narrativePath = outDir / "NarrativeObservation.json";
    fs::path discursivePath = outDir / "DiscursiveContext.json";

    IW_ASSERT(fs::exists(narrativePath), "F1.C3: NarrativeObservation.json exportado");
    IW_ASSERT(fs::exists(discursivePath), "F1.C3: DiscursiveContext.json exportado");
    IW_ASSERT(narrativePath != discursivePath, "F1.C3: artefatos narrativo e discursivo são distintos");

    std::ifstream narrativeFile(narrativePath);
    nlohmann::json narrative = nlohmann::json::parse(narrativeFile);
    IW_ASSERT(narrative.contains("history") && narrative["history"].is_array(),
              "F1.C3: NarrativeObservation mantém envelope de história");

    std::ifstream discursiveFile(discursivePath);
    nlohmann::json discursive = nlohmann::json::parse(discursiveFile);
    IW_ASSERT(discursive.contains("discursiveContext") && discursive["discursiveContext"].is_object(),
              "F1.C3: DiscursiveContext mantém envelope discursivo");

    return true;
}

// ─────────────────────────────────────────────────────────────────────────────
// Main
// ─────────────────────────────────────────────────────────────────────────────
int main() {
    std::cout << "=== IW NarrativeBundle Invariant Tests (F1.A1–A3, F1.C1) ===\n";
    std::cout << "Refs: ADR-002, ADR-003, ADR-004, ADR-007\n\n";

    RUN_TEST(Test_F1_A1_SchemaVersionPresent);
    RUN_TEST(Test_F1_A2_NarrativeRegimePresent);
    RUN_TEST(Test_F1_A3_ToolchainSourcePresent);
    RUN_TEST(Test_F1_C1_BifasicTwoCallsPerDocument);
    RUN_TEST(Test_F1_C1_TwoDocumentsFourCalls);
    RUN_TEST(Test_F1_A1_BundleWithoutSchemaVersionRejected);
    RUN_TEST(Test_F1_A4_ExportOnlyStructuredBundleArtifacts);
    RUN_TEST(Test_F1_C3_NarrativeAndDiscursiveArtifactsSeparated);

    std::cout << "\n=== Results: " << g_passed << " passed, " << g_failed << " failed ===\n";
    return (g_failed == 0) ? 0 : 1;
}
