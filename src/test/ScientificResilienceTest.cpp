/**
 * @file ScientificResilienceTest.cpp
 * @brief F1 Hardening — Tests for extraction logging and pipeline resilience.
 *
 * Covers:
 *   F1.B2 — Log de exclusão estrutural escrito em toda extração de PDF
 *   F1.C2 — Ausência de DiscursiveContext não bloqueia pipeline (Modo Parcial)
 *
 * Refs: ADR-007, ADR-008, ADR-011
 */

#include <cassert>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <optional>
#include <sstream>
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
// Mocks
// ─────────────────────────────────────────────────────────────────────────────

class FailingDiscursiveAIService : public domain::AIService {
public:
    std::atomic<int> callCount{0};

    std::optional<std::string> generateJson(const std::string&, const std::string&) override {
        int call = ++callCount;
        if (call % 2 == 1) {
            // Narrative Turn - Success
            nlohmann::json j;
            j["schemaVersion"] = domain::scientific::ScientificSchema::SchemaVersion;
            j["narrativeRegime"] = "experimental";
            j["narrativeObservations"] = nlohmann::json::array();
            j["allegedMechanisms"] = nlohmann::json::array();
            j["temporalWindowReferences"] = nlohmann::json::array();
            j["baselineAssumptions"] = nlohmann::json::array();
            j["trajectoryAnalogies"] = nlohmann::json::array();
            j["interpretationLayers"] = {
                {"observedStatements", nlohmann::json::array()},
                {"authorInterpretations", nlohmann::json::array()},
                {"possibleReadings", nlohmann::json::array()}
            };
            j["sourceProfile"] = {
                {"studyType", "experimental"}, {"temporalScale", "short"},
                {"ecosystemType", "terrestrial"}, {"evidenceType", "empirical"},
                {"transferability", "high"}
            };
            return j.dump();
        } else {
            // Discursive Turn - Failure
            return std::nullopt;
        }
    }

    // Stubs
    std::optional<domain::Insight> processRawThought(const std::string&, bool, std::function<void(std::string)>) override { return std::nullopt; }
    std::optional<std::string> chat(const std::vector<ChatMessage>&, bool) override { return "mock"; }
    std::optional<std::string> consolidateTasks(const std::string&) override { return std::nullopt; }
    std::vector<float> getEmbedding(const std::string&) override { return {}; }
    std::vector<std::string> getAvailableModels() override { return {"mock"}; }
    void setModel(const std::string&) override {}
    std::string getCurrentModel() const override { return "mock-fail-discursive"; }
};

struct TestFixture {
    fs::path root;
    fs::path inboxPath;
    fs::path observationsPath;
    fs::path consumablesPath;

    TestFixture() {
        root = fs::temp_directory_path() / "iw_resilience_test";
        inboxPath = root / "inbox";
        observationsPath = root / "observations";
        consumablesPath = root / "consumables";
        fs::create_directories(inboxPath);
        fs::create_directories(observationsPath);
        fs::create_directories(consumablesPath);
    }

    ~TestFixture() {
        fs::remove_all(root);
    }

    void createFakePdf(const std::string& name) {
        // We can't easily create a real PDF, but pdftotext will fail and it might fallback.
        // For F1.B2 test, we might need a way to mock pdftotext output or use a real small PDF.
        // Since pdftotext is a system tool, we'll simulate a text file and see if we can trigger the exclusion logic
        // in a controlled way if we can bypass the extension check or just use a text file if the service allows.
        std::ofstream f(inboxPath / name);
        f << "Header A\nContent line 1\nFooter B\n\f"
          << "Header A\nContent line 2\nFooter B\n\f"
          << "Header A\nContent line 3\nFooter B\n";
    }
};

// ─────────────────────────────────────────────────────────────────────────────
// Test: F1.C2 — Discursive failure doesn't block pipeline
// ─────────────────────────────────────────────────────────────────────────────
bool Test_F1_C2_DiscursiveFailureNotBlocking() {
    TestFixture fx;
    auto ai = std::make_shared<FailingDiscursiveAIService>();
    auto scanner = std::make_unique<infrastructure::FileSystemArtifactScanner>(fx.inboxPath.string());
    application::scientific::ScientificIngestionService service(
        std::move(scanner), ai, fx.observationsPath.string(), fx.consumablesPath.string());

    // Create a plain text file (it uses ExtractText which doesn't have exclusion rule yet, 
    // but the ingestion logic is the same).
    std::ofstream f(fx.inboxPath / "resilience_test.txt");
    f << "Scientific content for resilience testing.";
    f.close();

    auto result = service.ingestPending(nullptr);

    IW_ASSERT(result.artifactsDetected == 1, "Artefato detectado");
    IW_ASSERT(result.errors.size() >= 1, "Erro da discursiva foi registrado no result");
    IW_ASSERT(result.bundlesGenerated == 1, "F1.C2: Bundle gerado APESAR da falha na discursiva");

    // Verificar se o bundle salvo tem marcação de partial
    bool foundPartial = false;
    for (const auto& entry : fs::directory_iterator(fx.observationsPath)) {
        if (entry.path().extension() == ".json") {
            std::ifstream bf(entry.path());
            nlohmann::json j = nlohmann::json::parse(bf);
            if (j.contains("source") && j["source"].is_object()) {
                const auto& src = j["source"];
                if (src.contains("isPartial") && src["isPartial"].get<bool>() == true) {
                    foundPartial = true;
                    IW_ASSERT(src["extractionStatus"] == "partial-narrative", "Status de extração correto (partial-narrative)");
                }
            }
        }
    }
    IW_ASSERT(foundPartial, "F1.C2: Bundle salvo contém metadados de 'partialExtraction'");

    return true;
}

// ─────────────────────────────────────────────────────────────────────────────
// Test: F1.B2 — Structural exclusion logging (Simulated)
// ─────────────────────────────────────────────────────────────────────────────
bool Test_F1_B2_ExclusionLogging() {
    TestFixture fx;
    const std::string artifactId = "b2_exclusion_logging";
    const std::vector<std::string> exclusions = {
        "Journal Header 2026",
        "Page 1 of 12"
    };

    std::string error;
    bool ok = application::scientific::ScientificIngestionService::WriteStructuralExclusionAuditLog(
        fx.observationsPath.string(),
        artifactId,
        exclusions,
        &error);

    IW_ASSERT(ok, "F1.B2: escrita do log de exclusão estrutural");
    IW_ASSERT(error.empty(), "F1.B2: sem erro na escrita do log");

    fs::path logPath = fx.observationsPath / (artifactId + ".exclusions.log");
    IW_ASSERT(fs::exists(logPath), "F1.B2: arquivo .exclusions.log foi criado");

    std::ifstream logFile(logPath);
    std::stringstream buffer;
    buffer << logFile.rdbuf();
    const std::string content = buffer.str();

    IW_ASSERT(content.find("Local Audit Artifact (LAA)") != std::string::npos,
              "F1.B2: classificação LAA registrada no log");
    IW_ASSERT(content.find("Journal Header 2026") != std::string::npos,
              "F1.B2: linha excluída registrada no log");

    return true;
}

int main() {
    std::cout << "=== IW Scientific Resilience Tests (F1.B2, F1.C2) ===\n";

    RUN_TEST(Test_F1_C2_DiscursiveFailureNotBlocking);
    RUN_TEST(Test_F1_B2_ExclusionLogging);

    std::cout << "\n=== Results: " << g_passed << " passed, " << g_failed << " failed ===\n";
    return (g_failed == 0) ? 0 : 1;
}
