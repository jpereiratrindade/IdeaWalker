#include "ui/panels/MainPanels.hpp"
#include "imgui.h"

#include <atomic>
#include <chrono>
#include <cstring>
#include <cstdio>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <mutex>
#include <sstream>
#include <string>
#include <vector>
#include <algorithm>
#include <cctype>
#include <cfloat>

#if !defined(_WIN32)
#include <sys/wait.h>
#endif

namespace ideawalker::ui {

namespace {

std::string EscapeForDoubleQuotes(std::string value) {
    // Minimal escaping for a string inserted inside "...".
    // (We use this only for `cd "<dir>"`.)
    std::string out;
    out.reserve(value.size() + 8);
    for (char c : value) {
        switch (c) {
            case '\\': out += "\\\\"; break;
            case '"': out += "\\\""; break;
            case '$': out += "\\$"; break;
            case '`': out += "\\`"; break;
            default: out += c; break;
        }
    }
    return out;
}

int ExtractExitCode(int pcloseStatus) {
#if defined(_WIN32)
    return pcloseStatus;
#else
    if (WIFEXITED(pcloseStatus)) return WEXITSTATUS(pcloseStatus);
    if (WIFSIGNALED(pcloseStatus)) return 128 + WTERMSIG(pcloseStatus);
    return pcloseStatus;
#endif
}

std::tm LocalTimeNow() {
    const auto now = std::chrono::system_clock::now();
    const std::time_t t = std::chrono::system_clock::to_time_t(now);
    std::tm tm{};
#if defined(_WIN32)
    localtime_s(&tm, &t);
#else
    localtime_r(&t, &tm);
#endif
    return tm;
}

std::string CurrentDateIso() {
    const std::tm tm = LocalTimeNow();
    std::ostringstream oss;
    oss << std::put_time(&tm, "%Y-%m-%d");
    return oss.str();
}

std::string CurrentTimestampCompact() {
    const std::tm tm = LocalTimeNow();
    std::ostringstream oss;
    oss << std::put_time(&tm, "%Y%m%d-%H%M%S");
    return oss.str();
}

int TextEditCallback(ImGuiInputTextCallbackData* data) {
    if (data->EventFlag == ImGuiInputTextFlags_CallbackResize) {
        auto* str = static_cast<std::string*>(data->UserData);
        str->resize(data->BufTextLen);
        data->Buf = str->data();
    }
    return 0;
}

bool InputTextMultilineString(const char* label, std::string* str, const ImVec2& size) {
    ImGuiInputTextFlags flags = ImGuiInputTextFlags_CallbackResize | ImGuiInputTextFlags_NoHorizontalScroll;
    if (str->capacity() == 0) str->reserve(2048);
    return ImGui::InputTextMultiline(label, str->data(), str->capacity() + 1, size, flags, TextEditCallback, str);
}

bool WriteTextFile(const std::filesystem::path& path, const std::string& content) {
    std::ofstream out(path);
    if (!out) return false;
    out << content;
    return true;
}

bool ReadTextFile(const std::filesystem::path& path, std::string& contentOut) {
    std::ifstream in(path);
    if (!in) return false;
    std::stringstream buffer;
    buffer << in.rdbuf();
    contentOut = buffer.str();
    return true;
}

std::filesystem::path ResolveTargetPath(const std::filesystem::path& wsRoot, const char* target) {
    const std::filesystem::path raw = std::filesystem::path(target ? target : "");
    if (raw.empty()) return {};
    if (raw.is_absolute()) return raw;
    return wsRoot / raw;
}

std::filesystem::path FindFileByNameInWorkspace(const std::filesystem::path& wsRoot, const std::string& filename) {
    std::error_code ec;
    if (filename.empty() || !std::filesystem::exists(wsRoot, ec)) return {};
    for (const auto& entry : std::filesystem::recursive_directory_iterator(wsRoot, ec)) {
        if (ec) break;
        if (!entry.is_regular_file(ec)) continue;
        if (entry.path().filename() == filename) {
            return entry.path();
        }
    }
    return {};
}

bool IsIgnoredDirectoryName(const std::string& name) {
    return name == ".git" || name == "build" || name == "build-test" || name == ".iwcache" || name == "html";
}

bool IsEditableDocumentPath(const std::filesystem::path& filePath) {
    std::string ext = filePath.extension().string();
    std::transform(ext.begin(), ext.end(), ext.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return ext == ".md" || ext == ".tex" || ext == ".txt";
}

std::string TrimWhitespace(std::string text);

std::string FileExtensionLower(const std::filesystem::path& filePath) {
    std::string ext = filePath.extension().string();
    std::transform(ext.begin(), ext.end(), ext.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return ext;
}

std::string ShellQuoteSingle(std::string value) {
    std::string out;
    out.reserve(value.size() + 8);
    out.push_back('\'');
    for (char c : value) {
        if (c == '\'') out += "'\\''";
        else out.push_back(c);
    }
    out.push_back('\'');
    return out;
}

std::string BuildDocTypeCommand(
    const std::filesystem::path& workspaceRoot,
    const std::filesystem::path& relativeFile,
    bool buildMode
) {
    if (relativeFile.empty()) return {};
    const std::string ext = FileExtensionLower(relativeFile);
    const std::string quotedFile = ShellQuoteSingle(relativeFile.string());
    const std::string quotedAbs = ShellQuoteSingle((workspaceRoot / relativeFile).string());

    if (ext == ".tex") {
        return "latexmk -pdf -interaction=nonstopmode -halt-on-error " + quotedFile;
    }
    if (ext == ".md") {
        const std::string outputPath = buildMode
            ? ShellQuoteSingle("/tmp/docops-md-build.html")
            : ShellQuoteSingle("/tmp/docops-md-check.html");
        return "pandoc -f markdown -t html " + quotedFile + " -o " + outputPath;
    }
    if (ext == ".txt") {
        if (buildMode) {
            return "cp " + quotedFile + " " + ShellQuoteSingle("/tmp/docops-txt-build.txt");
        }
        return "test -f " + quotedAbs;
    }
    return {};
}

struct CommandResult {
    int exitCode = 0;
    std::string output;
};

CommandResult RunShellCommandInWorkspace(const std::filesystem::path& workspaceRoot, const std::string& cmd) {
    CommandResult result;
    const std::string cd = "cd \"" + EscapeForDoubleQuotes(workspaceRoot.string()) + "\"";
    const std::string bash = "bash -lc \"" + cd + " && " + cmd + " 2>&1\"";
    FILE* pipe = popen(bash.c_str(), "r");
    if (!pipe) {
        result.exitCode = 127;
        result.output = "ERROR: popen() falhou ao executar comando.\n";
        return result;
    }

    char buffer[512];
    while (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
        result.output += buffer;
    }

    const int rc = pclose(pipe);
    result.exitCode = ExtractExitCode(rc);
    return result;
}

bool LooksLikeUnifiedDiff(const std::string& text) {
    if (text.find("diff --git ") != std::string::npos) return true;
    if (text.find("\n--- ") != std::string::npos && text.find("\n+++ ") != std::string::npos) return true;
    if (text.rfind("--- ", 0) == 0 && text.find("\n+++ ") != std::string::npos) return true;
    return false;
}

std::string ExtractFencedCodeBlock(const std::string& text, const char* languageHint) {
    std::string fence = "```";
    if (languageHint && std::strlen(languageHint) > 0) fence += languageHint;
    const std::size_t start = text.find(fence);
    if (start == std::string::npos) return {};
    const std::size_t lineEnd = text.find('\n', start);
    if (lineEnd == std::string::npos) return {};
    const std::size_t end = text.find("```", lineEnd + 1);
    if (end == std::string::npos || end <= lineEnd + 1) return {};
    return TrimWhitespace(text.substr(lineEnd + 1, end - (lineEnd + 1)));
}

std::string ExtractBetweenMarkers(const std::string& text, const std::string& beginMarker, const std::string& endMarker) {
    const std::size_t begin = text.find(beginMarker);
    if (begin == std::string::npos) return {};
    std::size_t contentStart = begin + beginMarker.size();
    while (contentStart < text.size() && (text[contentStart] == '\n' || text[contentStart] == '\r')) {
        ++contentStart;
    }
    const std::size_t end = text.find(endMarker, contentStart);
    if (end == std::string::npos || end <= contentStart) return {};
    return TrimWhitespace(text.substr(contentStart, end - contentStart));
}

std::string ExtractUnifiedDiffCandidate(const std::string& assistantText) {
    std::string patch = ExtractFencedCodeBlock(assistantText, "diff");
    if (!patch.empty() && LooksLikeUnifiedDiff(patch)) return patch;

    patch = ExtractFencedCodeBlock(assistantText, "patch");
    if (!patch.empty() && LooksLikeUnifiedDiff(patch)) return patch;

    const std::size_t diffStart = assistantText.find("diff --git ");
    if (diffStart != std::string::npos) {
        const std::string candidate = TrimWhitespace(assistantText.substr(diffStart));
        if (LooksLikeUnifiedDiff(candidate)) return candidate;
    }

    if (LooksLikeUnifiedDiff(assistantText)) return TrimWhitespace(assistantText);
    return {};
}

bool ApplyUnifiedDiffPatch(const std::filesystem::path& workspaceRoot, const std::string& patchText, CommandResult& resultOut) {
    std::error_code ec;
    const std::filesystem::path cacheDir = workspaceRoot / ".iwcache";
    std::filesystem::create_directories(cacheDir, ec);
    if (ec) {
        resultOut.exitCode = 1;
        resultOut.output = "ERROR: falha ao criar .iwcache para patch.\n";
        return false;
    }

    const std::filesystem::path patchPath = cacheDir / "docops-last-assistant.patch";
    if (!WriteTextFile(patchPath, patchText)) {
        resultOut.exitCode = 1;
        resultOut.output = "ERROR: falha ao gravar arquivo de patch temporario.\n";
        return false;
    }

    const std::string quotedPatch = ShellQuoteSingle(patchPath.string());
    const std::string cmd =
        "if git rev-parse --is-inside-work-tree >/dev/null 2>&1; then "
        "git apply --check --whitespace=nowarn --recount " + quotedPatch + " && "
        "git apply --whitespace=nowarn --recount " + quotedPatch + "; "
        "else echo 'Workspace sem git inicializado para aplicar patch seguro.'; exit 2; fi";

    resultOut = RunShellCommandInWorkspace(workspaceRoot, cmd);
    return resultOut.exitCode == 0;
}

std::vector<std::filesystem::path> ListEditableDocuments(const std::filesystem::path& wsRoot) {
    std::vector<std::filesystem::path> files;
    std::error_code ec;
    if (!std::filesystem::exists(wsRoot, ec) || !std::filesystem::is_directory(wsRoot, ec)) return files;

    for (std::filesystem::recursive_directory_iterator it(wsRoot, ec), end; it != end; it.increment(ec)) {
        if (ec) break;
        const auto& entry = *it;
        if (entry.is_directory(ec)) {
            if (IsIgnoredDirectoryName(entry.path().filename().string())) {
                it.disable_recursion_pending();
            }
            continue;
        }
        if (!entry.is_regular_file(ec)) continue;
        if (!IsEditableDocumentPath(entry.path())) continue;
        files.push_back(entry.path().lexically_relative(wsRoot));
    }

    std::sort(files.begin(), files.end());
    return files;
}

std::string TrimWhitespace(std::string text) {
    auto isSpace = [](unsigned char c) { return std::isspace(c) != 0; };
    while (!text.empty() && isSpace(static_cast<unsigned char>(text.front()))) text.erase(text.begin());
    while (!text.empty() && isSpace(static_cast<unsigned char>(text.back()))) text.pop_back();
    return text;
}

std::string ToSingleLineCompact(const std::string& text, std::size_t maxChars) {
    std::string out;
    out.reserve(text.size());
    for (char c : text) {
        if (c == '\n' || c == '\r' || c == '\t') out.push_back(' ');
        else out.push_back(c);
    }
    out = TrimWhitespace(out);
    if (out.size() > maxChars && maxChars > 3) {
        out.resize(maxChars - 3);
        out += "...";
    }
    return out;
}

std::string ExtractAssistantPayloadForDocument(const std::string& assistantText) {
    const std::string marked = ExtractBetweenMarkers(
        assistantText,
        "<<DOCOPS_CONTENT_BEGIN>>",
        "<<DOCOPS_CONTENT_END>>"
    );
    if (!marked.empty()) return marked;

    const char* preferredFences[] = {"text", "markdown", "md", "tex", "latex"};
    for (const char* hint : preferredFences) {
        const std::string fenced = ExtractFencedCodeBlock(assistantText, hint);
        if (!fenced.empty() && !LooksLikeUnifiedDiff(fenced)) return fenced;
    }

    std::string candidate = TrimWhitespace(assistantText);
    const std::vector<std::string> cutMarkers = {
        "\n```diff",
        "\n```patch",
        "\n### Diferenças",
        "\n### Diferencas",
        "\n## Diferenças",
        "\n## Diferencas",
        "\nEssas alteracoes",
        "\nEssas alterações"
    };
    std::size_t cutAt = std::string::npos;
    for (const auto& marker : cutMarkers) {
        const std::size_t pos = candidate.find(marker);
        if (pos != std::string::npos && (cutAt == std::string::npos || pos < cutAt)) {
            cutAt = pos;
        }
    }
    if (cutAt != std::string::npos) {
        candidate = TrimWhitespace(candidate.substr(0, cutAt));
    }
    return candidate;
}

std::string GetLastAssistantResponse(application::ConversationService* conversationService) {
    if (!conversationService) return {};
    const auto history = conversationService->getHistory();
    for (auto it = history.rbegin(); it != history.rend(); ++it) {
        if (it->role == domain::AIService::ChatMessage::Role::Assistant && !it->content.empty()) {
            return it->content;
        }
    }
    return {};
}

std::string BuildTargetFileTemplate(
    const std::filesystem::path& targetPath,
    const char* reason,
    const char* adrRef,
    const char* profile
) {
    std::string ext = targetPath.extension().string();
    std::transform(ext.begin(), ext.end(), ext.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });

    std::ostringstream oss;
    const std::string reasonText = (reason && std::strlen(reason) > 0) ? reason : "<pending>";
    const std::string adrText = (adrRef && std::strlen(adrRef) > 0) ? adrRef : "editorial-only";
    const std::string profileText = (profile && std::strlen(profile) > 0) ? profile : "editor";

    if (ext == ".tex") {
        oss << "% DocOps target file scaffold\n";
        oss << "% Profile: " << profileText << "\n";
        oss << "% ADR: " << adrText << "\n";
        oss << "% Motivo: " << reasonText << "\n\n";
        oss << "\\section{Draft Inicial}\n\n";
        oss << "Texto inicial gerado para iniciar a sessao DocOps.\n";
    } else if (ext == ".md") {
        oss << "# Draft Inicial\n\n";
        oss << "- Profile: `" << profileText << "`\n";
        oss << "- ADR: `" << adrText << "`\n";
        oss << "- Motivo: " << reasonText << "\n\n";
        oss << "## Conteudo\n\n";
        oss << "Texto inicial gerado para iniciar a sessao DocOps.\n";
    } else {
        oss << "DocOps Draft\n";
        oss << "Profile: " << profileText << "\n";
        oss << "ADR: " << adrText << "\n";
        oss << "Motivo: " << reasonText << "\n\n";
        oss << "Texto inicial gerado para iniciar a sessao DocOps.\n";
    }

    return oss.str();
}

bool EnsureDocOpsContractSkeleton(const std::filesystem::path& wsRoot, std::string& errorOut) {
    std::error_code ec;
    std::filesystem::create_directories(wsRoot / "docops" / "profiles", ec);
    if (ec) {
        errorOut = "cannot create docops/profiles";
        return false;
    }
    std::filesystem::create_directories(wsRoot / "docops" / "templates", ec);
    if (ec) {
        errorOut = "cannot create docops/templates";
        return false;
    }
    std::filesystem::create_directories(wsRoot / "reports" / "docops" / "requests", ec);
    if (ec) {
        errorOut = "cannot create reports/docops/requests";
        return false;
    }

    const std::filesystem::path contract = wsRoot / "docops" / "docops.yaml";
    if (!std::filesystem::exists(contract)) {
        const std::string yaml =
            "version: \"1.0\"\n"
            "context: \"DocOps-lite\"\n"
            "profiles_dir: \"docops/profiles\"\n"
            "templates_dir: \"docops/templates\"\n\n"
            "project_registry: \"docops/project_registry.yaml\"\n\n"
            "workspace_layout:\n"
            "  required_paths:\n"
            "    - \"adr\"\n"
            "    - \"docs\"\n"
            "    - \"scripts\"\n"
            "    - \"docops/profiles\"\n"
            "    - \"docops/templates\"\n\n"
            "edit_protocol:\n"
            "  mode: \"plan-propose-review-apply\"\n"
            "  require_human_approval: true\n"
            "  require_diff_preview: true\n"
            "  require_adr_reference_for_structural_changes: true\n\n"
            "governance:\n"
            "  adr_index: \"adr/ADR_INDEX.md\"\n"
            "  ddd_reference: \"docs/DDD_CONTEXT.md\"\n"
            "  evaluations_dir: \"docs/avaliacoes\"\n"
            "  release_log_dir: \"reports/docops\"\n\n"
            "llm_assistance:\n"
            "  enabled: true\n"
            "  model_runtime: \"local-or-configured-by-workspace\"\n"
            "  write_mode: \"proposal_only\"\n\n"
            "filetype_enforcement:\n"
            "  supported_extensions:\n"
            "    - \".tex\"\n"
            "    - \".md\"\n"
            "    - \".txt\"\n"
            "  check_commands:\n"
            "    \".tex\": \"latexmk -pdf -interaction=nonstopmode -halt-on-error {file}\"\n"
            "    \".md\": \"pandoc -f markdown -t html {file} -o /tmp/docops-md-check.html\"\n"
            "    \".txt\": \"test -f {file}\"\n"
            "  build_commands:\n"
            "    \".tex\": \"latexmk -pdf -interaction=nonstopmode -halt-on-error {file}\"\n"
            "    \".md\": \"pandoc -f markdown -t html {file} -o /tmp/docops-md-build.html\"\n"
            "    \".txt\": \"cp {file} /tmp/docops-txt-build.txt\"\n";
        if (!WriteTextFile(contract, yaml)) {
            errorOut = "cannot write docops/docops.yaml";
            return false;
        }
    }

    const std::filesystem::path registry = wsRoot / "docops" / "project_registry.yaml";
    if (!std::filesystem::exists(registry)) {
        const std::string registryContent =
            "project_name: \"ProjectName\"\n"
            "governance_status: \"baseline\"\n\n"
            "ddd:\n"
            "  required: true\n"
            "  canonical_doc: \"docs/DDD_CONTEXT.md\"\n\n"
            "adr:\n"
            "  required: true\n"
            "  index: \"adr/ADR_INDEX.md\"\n"
            "  readme: \"adr/README.md\"\n\n"
            "notes:\n"
            "  - \"Todo projeto DocOps deve explicitar DDD e ADR antes de escrita assistida.\"\n";
        if (!WriteTextFile(registry, registryContent)) {
            errorOut = "cannot write docops/project_registry.yaml";
            return false;
        }
    }

    const std::vector<std::pair<std::string, std::string>> profiles = {
        {"architect.yaml",
         "id: \"architect\"\nname: \"Architect\"\nmode: \"analysis\"\nobjective: \"Estruturar documentos e decisoes com consistencia DDD/ADR.\"\ninputs:\n  - \"contexto_do_projeto\"\nallowed_actions:\n  - \"propor_estrutura\"\nprohibited_actions:\n  - \"editar_arquivo_sem_aprovacao\"\nprompt_template: |\n  Voce atua como Architect do DocOps.\n"},
        {"author.yaml",
         "id: \"author\"\nname: \"Author\"\nmode: \"drafting\"\nobjective: \"Redigir versoes iniciais alinhadas ao escopo.\"\ninputs:\n  - \"brief\"\nallowed_actions:\n  - \"propor_redacao\"\nprohibited_actions:\n  - \"aplicar_patch_sem_review\"\nprompt_template: |\n  Voce atua como Author do DocOps.\n"},
        {"editor.yaml",
         "id: \"editor\"\nname: \"Editor\"\nmode: \"refinement\"\nobjective: \"Melhorar clareza sem alterar compromisso normativo.\"\ninputs:\n  - \"texto_corrente\"\nallowed_actions:\n  - \"refatorar_redacao\"\nprohibited_actions:\n  - \"mudar_status_de_adr\"\nprompt_template: |\n  Voce atua como Editor do DocOps.\n"},
        {"reviewer.yaml",
         "id: \"reviewer\"\nname: \"Reviewer\"\nmode: \"critical_review\"\nobjective: \"Apontar riscos e aderencia a ADR/DDD.\"\ninputs:\n  - \"documento_proposto\"\nallowed_actions:\n  - \"identificar_riscos\"\nprohibited_actions:\n  - \"aprovar_sem_criterio\"\nprompt_template: |\n  Voce atua como Reviewer do DocOps.\n"},
    };

    for (const auto& [filename, content] : profiles) {
        const std::filesystem::path p = wsRoot / "docops" / "profiles" / filename;
        if (!std::filesystem::exists(p) && !WriteTextFile(p, content)) {
            errorOut = "cannot write profile " + filename;
            return false;
        }
    }

    const std::filesystem::path tpl = wsRoot / "docops" / "templates" / "EDIT_REQUEST_TEMPLATE.md";
    if (!std::filesystem::exists(tpl)) {
        const std::string content =
            "# Edit Request\n\n"
            "**Date:** YYYY-MM-DD  \n"
            "**Profile:** architect | author | editor | reviewer  \n"
            "**Target File:** path/to/file.md  \n"
            "**Reason:** short rationale  \n"
            "**ADR Reference:** ADR-XXX (optional if editorial only)\n\n"
            "---\n\n"
            "## 1) Plan\n\n## 2) Proposal\n\n## 3) Review Checklist\n\n## 4) Apply Decision\n";
        if (!WriteTextFile(tpl, content)) {
            errorOut = "cannot write templates/EDIT_REQUEST_TEMPLATE.md";
            return false;
        }
    }

    return true;
}

std::vector<std::string> ListAdrIds(const std::filesystem::path& wsRoot) {
    std::vector<std::string> ids;
    std::error_code ec;
    const std::filesystem::path adrDir = wsRoot / "adr";
    if (!std::filesystem::exists(adrDir, ec) || !std::filesystem::is_directory(adrDir, ec)) return ids;
    for (const auto& entry : std::filesystem::directory_iterator(adrDir, ec)) {
        if (!entry.is_regular_file(ec)) continue;
        const std::string name = entry.path().filename().string();
        if (name.rfind("ADR-", 0) == 0 && name.size() >= 7 && name.find(".md") != std::string::npos
            && name.find("ADR_INDEX") == std::string::npos
            && name.find("ADR_TEMPLATE") == std::string::npos) {
            ids.push_back(name.substr(0, 7));
        }
    }
    std::sort(ids.begin(), ids.end());
    ids.erase(std::unique(ids.begin(), ids.end()), ids.end());
    return ids;
}

int NextAdrNumber(const std::vector<std::string>& ids) {
    int maxNum = -1;
    for (const auto& id : ids) {
        if (id.size() == 7) {
            try {
                const int n = std::stoi(id.substr(4, 3));
                if (n > maxNum) maxNum = n;
            } catch (...) {}
        }
    }
    return maxNum + 1;
}

std::string SlugifyForFilename(const std::string& title) {
    std::string out;
    out.reserve(title.size());
    for (char c : title) {
        if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9')) out += c;
        else if (c == ' ' || c == '-' || c == '_') out += '_';
    }
    if (out.empty()) out = "Untitled";
    return out;
}

std::string BuildAdrDraft(const std::string& adrId, const std::string& title, const std::string& decisionType) {
    std::ostringstream oss;
    oss << "# " << adrId << ": " << title << "\n\n";
    oss << "Status: Proposed\n";
    oss << "Date: " << CurrentDateIso() << "\n";
    oss << "Decision Type: " << decisionType << "\n";
    oss << "Supersedes: (none)\n";
    oss << "Superseded by: (none)\n\n";
    oss << "## Context\n\n";
    oss << "Describe the architectural or epistemological problem.\n\n";
    oss << "## Decision\n\n";
    oss << "Describe the normative decision.\n\n";
    oss << "## Scope / Boundaries\n\n";
    oss << "State where this ADR applies and where it does not.\n\n";
    oss << "## Allowed and Prohibited Flows\n\n";
    oss << "- Allowed:\n";
    oss << "- Prohibited:\n\n";
    oss << "## Consequences and Trade-offs\n\n";
    oss << "List technical and institutional impacts.\n\n";
    oss << "## Validation Criteria\n\n";
    oss << "List executable checks, tests, or CI gates that enforce this ADR.\n\n";
    oss << "## Related References\n\n";
    oss << "- Related ADRs:\n";
    oss << "- Related docs:\n";
    return oss.str();
}

application::ContextBundle BuildDocOpsFallbackBundle(
    const std::filesystem::path& workspace,
    const char* targetFile,
    const char* reason,
    const char* adrRef,
    const char* profile
) {
    application::ContextBundle bundle;
    bundle.activeNoteId = (targetFile && std::strlen(targetFile) > 0) ? targetFile : "DocOpsSession";
    std::ostringstream content;
    content << "DocOps Session Context\n";
    content << "Workspace: " << workspace.string() << "\n";
    content << "Profile: " << (profile ? profile : "editor") << "\n";
    content << "Target: " << bundle.activeNoteId << "\n";
    content << "Reason: " << ((reason && std::strlen(reason) > 0) ? reason : "<pending>") << "\n";
    content << "ADR: " << ((adrRef && std::strlen(adrRef) > 0) ? adrRef : "editorial-only") << "\n";
    content << "\nObservacao: arquivo alvo indisponivel no momento; continuar em modo de proposta.\n";
    bundle.activeNoteContent = content.str();
    return bundle;
}

std::string BuildDirectEditPrompt(
    const std::string& profile,
    const std::string& targetFile,
    const char* reason,
    const char* adrRef,
    const std::string& instruction,
    const std::string& currentContent
) {
    std::ostringstream prompt;
    prompt << "Modo DocOps: edicao direta de documento.\n";
    prompt << "Perfil: " << profile << "\n";
    prompt << "Arquivo alvo: " << (targetFile.empty() ? "<pending>" : targetFile) << "\n";
    prompt << "Motivo: " << ((reason && std::strlen(reason) > 0) ? reason : "<pending>") << "\n";
    prompt << "ADR referencia: " << ((adrRef && std::strlen(adrRef) > 0) ? adrRef : "editorial-only") << "\n\n";
    prompt << "Instrucao de edicao:\n" << instruction << "\n\n";
    prompt << "Regra de resposta obrigatoria:\n";
    prompt << "- Retorne somente o conteudo FINAL completo do arquivo.\n";
    prompt << "- Nao inclua explicacoes.\n";
    prompt << "- Nao inclua markdown fences (```)\n";
    prompt << "- Use este envelope EXATO:\n";
    prompt << "<<DOCOPS_CONTENT_BEGIN>>\n";
    prompt << "<conteudo final do arquivo>\n";
    prompt << "<<DOCOPS_CONTENT_END>>\n\n";
    prompt << "Conteudo atual do arquivo:\n";
    prompt << "```text\n" << currentContent << "\n```";
    return prompt.str();
}

std::string BuildConformityEvaluationDraft(
    const std::string& dateIso,
    const std::string& author,
    const std::string& base,
    const std::string& scope
) {
    std::ostringstream out;
    out << "# Avaliacao de Conformidade — DocOps\n\n";
    out << "**Data:** " << dateIso << "  \n";
    out << "**Autor:** " << (author.empty() ? "IW-DocOps" : author) << "  \n";
    out << "**Base:** " << (base.empty() ? "Editor DocOps" : base) << "  \n";
    out << "**Escopo:** " << (scope.empty() ? "texto ativo" : scope) << "  \n";
    out << "**Status:** Proposed\n\n";
    out << "## 1) Contexto\n\n";
    out << "Descreva objetivo e contexto da avaliacao.\n\n";
    out << "## 2) Critérios de Conformidade\n\n";
    out << "- Aderencia a ADR/DDD\n";
    out << "- Coerencia textual\n";
    out << "- Rastreabilidade de mudancas\n\n";
    out << "## 3) Achados\n\n";
    out << "- (preencher)\n\n";
    out << "## 4) Recomendacoes\n\n";
    out << "- (preencher)\n\n";
    out << "## 5) Resposta do IW\n\n";
    out << "- (preencher)\n";
    return out.str();
}

} // namespace

void DrawDocOpsTab(AppState& app) {
    auto label = [&app](const char* withEmoji, const char* plain) {
        return app.ui.emojiEnabled ? withEmoji : plain;
    };

    if (!ImGui::BeginTabItem(label("📝 DocOps", "DocOps"))) return;

    app.ui.activeTab = 6;

    static bool initialized = false;
    static char workspacePath[512] = "";
    static std::string lastProjectRoot;
    static int presetIndex = 0;
    static char customCommand[512] = "make check";
    static int editProfileIndex = 0;
    static char editTargetFile[512] = "";
    static char editReason[512] = "";
    static char editAdrRef[128] = "";
    static int adrDecisionTypeIndex = 4;
    static char newAdrTitle[256] = "";
    static int selectedAdrIndex = -1;
    static std::string llmPromptDraft;
    static std::string docopsSessionStatus = "idle";
    static std::vector<std::filesystem::path> editorDocFiles;
    static int selectedEditorDocIndex = -1;
    static std::filesystem::path loadedEditorDocPath;
    static std::filesystem::path loadedEditorDocRelativePath;
    static std::string editorBuffer;
    static std::string directEditInstruction;
    static char evaluationAuthor[128] = "IW-DocOps";
    static bool editorDirty = false;
    static std::string editorStatus = "idle";
    static std::string lastEditorWorkspace;
    static bool textFocusMode = true;
    static bool autoApplyLastAiToEditor = true;
    static bool autoSaveAfterAiApply = false;
    static bool applyPendingFromChat = false;
    static std::string pendingAssistantBaseline;

    static std::mutex outputMutex;
    static std::string output;
    static std::atomic<bool> running{false};
    static int lastExitCode = 0;
    const char* editProfiles[] = {"architect", "author", "editor", "reviewer"};

    if (!initialized) {
        std::snprintf(workspacePath, sizeof(workspacePath), "%s", app.project.root.c_str());
        lastProjectRoot = app.project.root;
        initialized = true;
    } else if (app.project.root != lastProjectRoot) {
        // Keep DocOps aligned with active project unless user explicitly changed workspace.
        if (std::strlen(workspacePath) == 0 || std::string(workspacePath) == lastProjectRoot) {
            std::snprintf(workspacePath, sizeof(workspacePath), "%s", app.project.root.c_str());
        }
        lastProjectRoot = app.project.root;
    }

    ImGui::TextWrapped(
        "DocOps = suporte a documentos governados (checks, releases, rastreabilidade). "
        "Aqui o IW atua como orquestrador: ele roda comandos no workspace selecionado e registra logs."
    );
    ImGui::Spacing();

    ImGui::Text("Workspace (diretório-alvo):");
    ImGui::PushItemWidth(-1);
    ImGui::InputText("##docops_workspace", workspacePath, sizeof(workspacePath));
    ImGui::PopItemWidth();

    if (ImGui::Button(label("📌 Usar raiz do projeto atual", "Use current project root"))) {
        std::snprintf(workspacePath, sizeof(workspacePath), "%s", app.project.root.c_str());
    }
    ImGui::SameLine();
    if (ImGui::Button(label("🧹 Limpar saída", "Clear output"))) {
        std::lock_guard<std::mutex> lock(outputMutex);
        output.clear();
        lastExitCode = 0;
    }

    const std::filesystem::path ws(workspacePath);
    const bool workspaceOk = std::filesystem::exists(ws) && std::filesystem::is_directory(ws);
    const bool hasDocOpsContract = workspaceOk && std::filesystem::exists(ws / "docops" / "docops.yaml");
    const bool hasProjectRegistry = workspaceOk && std::filesystem::exists(ws / "docops" / "project_registry.yaml");
    if (!workspaceOk) {
        ImGui::TextColored(ImVec4(1, 0.5f, 0.0f, 1), "Workspace inválido ou inexistente.");
    } else if (!hasDocOpsContract) {
        ImGui::TextColored(ImVec4(1, 0.75f, 0.2f, 1),
                           "Workspace sem contrato DocOps (`docops/docops.yaml`).");
        if (ImGui::Button(label("🧱 Inicializar contrato DocOps", "Initialize DocOps contract"))) {
            std::string err;
            if (EnsureDocOpsContractSkeleton(ws, err)) {
                app.AppendLog("[DocOps] Contract initialized in workspace.\n");
            } else {
                app.AppendLog("[DocOps] Contract initialization failed: " + err + "\n");
            }
        }
    } else if (!hasProjectRegistry) {
        ImGui::TextColored(ImVec4(1, 0.75f, 0.2f, 1),
                           "Registro de governanca ausente (`docops/project_registry.yaml`).");
        if (ImGui::Button(label("🧱 Gerar project_registry", "Create project_registry"))) {
            std::string err;
            if (EnsureDocOpsContractSkeleton(ws, err)) {
                app.AppendLog("[DocOps] project_registry created.\n");
            } else {
                app.AppendLog("[DocOps] Registry creation failed: " + err + "\n");
            }
        }
    }

    if (workspaceOk && lastEditorWorkspace != ws.string()) {
        lastEditorWorkspace = ws.string();
        editorDocFiles = ListEditableDocuments(ws);
        selectedEditorDocIndex = -1;
        loadedEditorDocPath.clear();
        loadedEditorDocRelativePath.clear();
        editorBuffer.clear();
        editorDirty = false;
        editorStatus = "workspace refreshed";
    }

    auto runCommandAsync = [&](const std::string& cmdStr) -> bool {
        if (!workspaceOk || !app.services.taskManager || running.load() || cmdStr.empty()) return false;
        running.store(true);

        const std::string wsStr = ws.string();
        {
            std::lock_guard<std::mutex> lock(outputMutex);
            output.clear();
            output += "[DocOps] Workspace: " + wsStr + "\n";
            output += "[DocOps] Command:   " + cmdStr + "\n\n";
        }
        app.AppendLog("[DocOps] Running: " + cmdStr + " (in " + wsStr + ")\n");
        app.SetProcessingStatus("DocOps: running command...");

        app.services.taskManager->SubmitTask(
            application::TaskType::Indexing,
            "DocOps",
            [wsStr, cmdStr, &app](std::shared_ptr<application::TaskStatus> status) {
                status->progress.store(0.1f);

                const std::string cd = "cd \"" + EscapeForDoubleQuotes(wsStr) + "\"";
                const std::string bash = "bash -lc \"" + cd + " && " + cmdStr + " 2>&1\"";

                FILE* pipe = popen(bash.c_str(), "r");
                if (!pipe) {
                    std::lock_guard<std::mutex> lock(outputMutex);
                    output += "ERROR: popen() falhou ao executar comando.\n";
                    lastExitCode = 127;
                    running.store(false);
                    return;
                }

                char buffer[512];
                while (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
                    std::lock_guard<std::mutex> lock(outputMutex);
                    output += buffer;
                }

                int rc = pclose(pipe);
                int exitCode = ExtractExitCode(rc);

                {
                    std::lock_guard<std::mutex> lock(outputMutex);
                    output += "\n[DocOps] Exit code: " + std::to_string(exitCode) + "\n";
                    lastExitCode = exitCode;
                }

                app.AppendLog("[DocOps] Completed with exit code: " + std::to_string(exitCode) + "\n");
                app.SetProcessingStatus("DocOps: done");
                status->progress.store(1.0f);
                running.store(false);
            }
        );
        return true;
    };

    ImGui::Checkbox("Modo foco no texto", &textFocusMode);
    if (textFocusMode) {
        ImGui::TextDisabled("Pre-configuracoes ficam em 'Assistente e Governanca'.");
    }

    if (ImGui::CollapsingHeader("Assistente e Governanca", textFocusMode ? 0 : ImGuiTreeNodeFlags_DefaultOpen)) {
    ImGui::Separator();
    ImGui::Text("Assistencia de edicao (MVP):");
    ImGui::Combo("Perfil##docops_edit_profile", &editProfileIndex, editProfiles, IM_ARRAYSIZE(editProfiles));
    ImGui::InputText("Arquivo alvo##docops_edit_target", editTargetFile, sizeof(editTargetFile));
    ImGui::InputText("Motivo##docops_edit_reason", editReason, sizeof(editReason));
    ImGui::InputText("ADR referencia##docops_edit_adr", editAdrRef, sizeof(editAdrRef));

    const std::filesystem::path editTargetResolved = ResolveTargetPath(ws, editTargetFile);
    const bool hasTargetPathInput = std::strlen(editTargetFile) > 0;
    const bool targetFileExists = hasTargetPathInput && !editTargetResolved.empty() &&
                                  std::filesystem::exists(editTargetResolved) &&
                                  std::filesystem::is_regular_file(editTargetResolved);
    if (hasTargetPathInput) {
        if (targetFileExists) {
            ImGui::TextColored(ImVec4(0.4f, 1.0f, 0.4f, 1.0f), "Arquivo alvo existe: %s", editTargetResolved.string().c_str());
        } else {
            ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.2f, 1.0f), "Arquivo alvo ausente: %s", editTargetResolved.string().c_str());
        }
    }
    const bool canCreateTargetFile = workspaceOk && hasTargetPathInput && !targetFileExists;
    if (!canCreateTargetFile) ImGui::BeginDisabled();
    if (ImGui::Button("Criar arquivo alvo", ImVec2(160, 0))) {
        std::error_code ec;
        if (!editTargetResolved.parent_path().empty()) {
            std::filesystem::create_directories(editTargetResolved.parent_path(), ec);
        }
        if (ec) {
            app.AppendLog("[DocOps] Failed creating parent directories for target file.\n");
        } else {
            const std::string templateContent = BuildTargetFileTemplate(
                editTargetResolved, editReason, editAdrRef, editProfiles[editProfileIndex]
            );
            if (WriteTextFile(editTargetResolved, templateContent)) {
                app.AppendLog("[DocOps] Target file created: " + editTargetResolved.string() + "\n");
                editorDocFiles = ListEditableDocuments(ws);
                loadedEditorDocPath = editTargetResolved;
                loadedEditorDocRelativePath = editTargetResolved.lexically_relative(ws);
                editorBuffer = templateContent;
                editorDirty = false;
                editorStatus = "created " + loadedEditorDocRelativePath.string();
                selectedEditorDocIndex = -1;
                for (int index = 0; index < static_cast<int>(editorDocFiles.size()); ++index) {
                    if (editorDocFiles[index] == loadedEditorDocRelativePath) {
                        selectedEditorDocIndex = index;
                        break;
                    }
                }
            } else {
                app.AppendLog("[DocOps] Failed creating target file: " + editTargetResolved.string() + "\n");
            }
        }
    }
    if (!canCreateTargetFile) ImGui::EndDisabled();

    const bool canCreateEditRequest = workspaceOk && hasDocOpsContract && hasProjectRegistry && !running.load();
    if (!canCreateEditRequest) ImGui::BeginDisabled();
    if (ImGui::Button(label("🧾 Gerar Edit Request", "Create edit request"), ImVec2(200, 0))) {
        const std::filesystem::path requestDir = ws / "reports" / "docops" / "requests";
        std::error_code ec;
        std::filesystem::create_directories(requestDir, ec);

        if (ec) {
            app.AppendLog("[DocOps] Failed creating requests directory: " + requestDir.string() + "\n");
        } else {
            const std::string stamp = CurrentTimestampCompact();
            const std::string date = CurrentDateIso();
            const std::filesystem::path requestPath = requestDir / ("EDIT_REQUEST-" + stamp + ".md");
            std::ofstream out(requestPath);
            if (!out) {
                app.AppendLog("[DocOps] Failed writing edit request file: " + requestPath.string() + "\n");
            } else {
                out << "# Edit Request\n\n";
                out << "**Date:** " << date << "  \n";
                out << "**Profile:** " << editProfiles[editProfileIndex] << "  \n";
                out << "**Target File:** " << (std::strlen(editTargetFile) > 0 ? editTargetFile : "<pending>") << "  \n";
                out << "**Reason:** " << (std::strlen(editReason) > 0 ? editReason : "<pending>") << "  \n";
                out << "**ADR Reference:** "
                    << (std::strlen(editAdrRef) > 0 ? editAdrRef : "editorial-only")
                    << "  \n\n";
                out << "---\n\n";
                out << "## 1) Plan\n\n";
                out << "Descreva o objetivo da mudanca.\n\n";
                out << "## 2) Proposal\n\n";
                out << "Descreva o patch/proposta.\n\n";
                out << "## 3) Review Checklist\n\n";
                out << "- [ ] Mantem consistencia com ADRs aplicaveis\n";
                out << "- [ ] Mantem rastreabilidade\n";
                out << "- [ ] Nao altera compromisso normativo sem ADR\n\n";
                out << "## 4) Apply Decision\n\n";
                out << "- [ ] Approved\n";
                out << "- [ ] Rejected\n";
                out << "- [ ] Needs revision\n";
                out.close();

                app.AppendLog("[DocOps] Edit request created: " + requestPath.string() + "\n");
            }
        }
    }
    if (!canCreateEditRequest) ImGui::EndDisabled();
    if (workspaceOk && (!hasDocOpsContract || !hasProjectRegistry)) {
        ImGui::TextDisabled("Requer `docops/docops.yaml` e `docops/project_registry.yaml`.");
    }

    std::vector<std::string> adrIds = ListAdrIds(ws);
    if (ImGui::CollapsingHeader("ADR helper")) {
        std::vector<const char*> adrItems;
        adrItems.reserve(adrIds.size());
        for (const auto& id : adrIds) adrItems.push_back(id.c_str());

        if (!adrItems.empty()) {
            if (selectedAdrIndex < 0 || selectedAdrIndex >= static_cast<int>(adrItems.size())) selectedAdrIndex = 0;
            ImGui::Combo("ADR existente##docops_adr_select", &selectedAdrIndex, adrItems.data(), static_cast<int>(adrItems.size()));
            ImGui::SameLine();
            if (ImGui::Button("Usar ADR selecionado")) {
                std::snprintf(editAdrRef, sizeof(editAdrRef), "%s", adrIds[selectedAdrIndex].c_str());
            }
        } else {
            ImGui::TextDisabled("Nenhum ADR encontrado em `adr/`.");
        }

        const char* decisionTypes[] = {
            "Foundational governance",
            "Domain boundary",
            "Contract",
            "Hardening",
            "Other",
        };
        ImGui::InputText("Novo ADR titulo##docops_new_adr_title", newAdrTitle, sizeof(newAdrTitle));
        ImGui::Combo("Decision type##docops_new_adr_type", &adrDecisionTypeIndex, decisionTypes, IM_ARRAYSIZE(decisionTypes));
        const bool canCreateAdr = workspaceOk && std::filesystem::exists(ws / "adr");
        if (!canCreateAdr) ImGui::BeginDisabled();
        if (ImGui::Button(label("📄 Gerar ADR draft", "Create ADR draft"), ImVec2(180, 0))) {
            if (std::strlen(newAdrTitle) == 0) {
                app.AppendLog("[DocOps] ADR title is required.\n");
            } else {
                const int nextNumber = NextAdrNumber(adrIds);
                std::ostringstream idOss;
                idOss << "ADR-" << std::setw(3) << std::setfill('0') << nextNumber;
                const std::string adrId = idOss.str();
                const std::string slug = SlugifyForFilename(newAdrTitle);
                const std::filesystem::path adrPath = ws / "adr" / (adrId + "_" + slug + ".md");
                const std::string content = BuildAdrDraft(adrId, newAdrTitle, decisionTypes[adrDecisionTypeIndex]);
                if (WriteTextFile(adrPath, content)) {
                    std::snprintf(editAdrRef, sizeof(editAdrRef), "%s", adrId.c_str());
                    app.AppendLog("[DocOps] ADR draft created: " + adrPath.string() + "\n");
                } else {
                    app.AppendLog("[DocOps] Failed creating ADR draft: " + adrPath.string() + "\n");
                }
            }
        }
        if (!canCreateAdr) ImGui::EndDisabled();
        if (!canCreateAdr && workspaceOk) {
            ImGui::TextDisabled("Workspace sem pasta `adr/`.");
        }
    }

    if (ImGui::CollapsingHeader("Bridge IA (Avancado)")) {
        if (llmPromptDraft.empty()) llmPromptDraft = "Escreva proposta de alteracao seguindo ADR/DDD e gere patch revisavel.";
        InputTextMultilineString("##docops_llm_prompt", &llmPromptDraft, ImVec2(-1, 90));
        if (ImGui::Button("Gerar prompt base a partir do formulario")) {
            std::ostringstream prompt;
            prompt << "Perfil: " << editProfiles[editProfileIndex] << "\n";
            prompt << "Arquivo alvo: " << (std::strlen(editTargetFile) > 0 ? editTargetFile : "<pending>") << "\n";
            prompt << "Motivo: " << (std::strlen(editReason) > 0 ? editReason : "<pending>") << "\n";
            prompt << "ADR referencia: " << (std::strlen(editAdrRef) > 0 ? editAdrRef : "editorial-only") << "\n";
            prompt << "Tarefa: Proponha alteracoes em formato de diff/patch revisavel sem aplicar direto.";
            llmPromptDraft = prompt.str();
        }
        ImGui::SameLine();
        if (ImGui::Button("Copiar prompt")) {
            ImGui::SetClipboardText(llmPromptDraft.c_str());
            app.AppendLog("[DocOps] Prompt copied to clipboard.\n");
        }
        ImGui::SameLine();
        const bool canSendChat = app.services.conversationService && app.services.conversationService->isSessionActive()
                                 && !app.services.conversationService->isThinking();
        if (!canSendChat) ImGui::BeginDisabled();
        if (ImGui::Button("Enviar para Chat")) {
            pendingAssistantBaseline = GetLastAssistantResponse(app.services.conversationService.get());
            applyPendingFromChat = autoApplyLastAiToEditor && !loadedEditorDocPath.empty();
            app.services.conversationService->sendMessage(llmPromptDraft);
            app.AppendLog("[DocOps] Prompt sent to Conversation chat.\n");
        }
        if (!canSendChat) ImGui::EndDisabled();
        if (!canSendChat) {
            ImGui::TextDisabled("Para enviar ao LLM, inicie uma sessao no chat (painel inferior).");
        }
        ImGui::Checkbox("Auto-aplicar resposta IA no editor", &autoApplyLastAiToEditor);
        ImGui::SameLine();
        ImGui::Checkbox("Auto-salvar apos aplicar", &autoSaveAfterAiApply);
    }

    const bool canStartDocOpsSession =
        app.services.conversationService &&
        workspaceOk &&
        std::strlen(editTargetFile) > 0;
    if (!canStartDocOpsSession) ImGui::BeginDisabled();
    if (ImGui::Button("Iniciar sessao DocOps")) {
        std::filesystem::path targetPath = ResolveTargetPath(ws, editTargetFile);
        if ((!targetPath.empty() && !std::filesystem::exists(targetPath)) && std::string(editTargetFile).find('/') == std::string::npos) {
            const std::filesystem::path found = FindFileByNameInWorkspace(ws, editTargetFile);
            if (!found.empty()) targetPath = found;
        }

        if (targetPath.empty() || !std::filesystem::exists(targetPath) || !std::filesystem::is_regular_file(targetPath)) {
            const auto fallback = BuildDocOpsFallbackBundle(
                ws, editTargetFile, editReason, editAdrRef, editProfiles[editProfileIndex]
            );
            app.services.conversationService->startSession(fallback);
            app.ui.selectedFilename = fallback.activeNoteId;
            app.ui.selectedNoteContent = fallback.activeNoteContent;
            docopsSessionStatus = "fallback:" + fallback.activeNoteId;
            {
                std::lock_guard<std::mutex> lock(outputMutex);
                output += "[DocOps] Session started (fallback context): " + fallback.activeNoteId + "\n";
            }
            app.AppendLog("[DocOps] Target file not found; session started with fallback context.\n");
        } else {
            std::string fileContent;
            if (!ReadTextFile(targetPath, fileContent)) {
                const auto fallback = BuildDocOpsFallbackBundle(
                    ws, editTargetFile, editReason, editAdrRef, editProfiles[editProfileIndex]
                );
                app.services.conversationService->startSession(fallback);
                app.ui.selectedFilename = fallback.activeNoteId;
                app.ui.selectedNoteContent = fallback.activeNoteContent;
                docopsSessionStatus = "fallback-read-error:" + fallback.activeNoteId;
                {
                    std::lock_guard<std::mutex> lock(outputMutex);
                    output += "[DocOps] Session started (read fallback): " + fallback.activeNoteId + "\n";
                }
                app.AppendLog("[DocOps] Failed reading target file; session started with fallback context.\n");
            } else {
                const std::string noteId = targetPath.lexically_relative(ws).string();
                application::ContextBundle bundle;
                if (app.services.contextAssembler) {
                    bundle = app.services.contextAssembler->assemble(noteId, fileContent);
                } else {
                    bundle = BuildDocOpsFallbackBundle(
                        ws, noteId.c_str(), editReason, editAdrRef, editProfiles[editProfileIndex]
                    );
                    bundle.activeNoteContent = fileContent;
                }
                app.services.conversationService->startSession(bundle);
                app.ui.selectedFilename = noteId;
                app.ui.selectedNoteContent = fileContent;
                docopsSessionStatus = "file:" + noteId;
                {
                    std::lock_guard<std::mutex> lock(outputMutex);
                    output += "[DocOps] Session started with file context: " + noteId + "\n";
                }
                app.AppendLog("[DocOps] Chat session started for target: " + targetPath.string() + "\n");
            }
        }
    }
    if (!canStartDocOpsSession) ImGui::EndDisabled();
    ImGui::SameLine();
    if (app.services.conversationService && app.services.conversationService->isSessionActive()) {
        const std::string activeSession = ToSingleLineCompact(app.services.conversationService->getCurrentNoteId(), 90);
        ImGui::TextColored(ImVec4(0.4f, 1.0f, 0.4f, 1.0f), "Sessao ativa: %s", activeSession.c_str());
    } else {
        const std::string inactiveStatus = ToSingleLineCompact(docopsSessionStatus, 90);
        ImGui::TextDisabled("Sessao inativa (%s)", inactiveStatus.c_str());
    }
    }

    if (applyPendingFromChat && !loadedEditorDocPath.empty()) {
        const std::string currentAssistant = GetLastAssistantResponse(app.services.conversationService.get());
        if (!currentAssistant.empty() && currentAssistant != pendingAssistantBaseline) {
            editorBuffer = ExtractAssistantPayloadForDocument(currentAssistant);
            editorDirty = true;
            editorStatus = "assistant response applied";
            applyPendingFromChat = false;
            if (autoSaveAfterAiApply) {
                if (WriteTextFile(loadedEditorDocPath, editorBuffer)) {
                    editorDirty = false;
                    editorStatus = "assistant applied and saved";
                    app.AppendLog("[DocOps] Assistant response applied and saved: " + loadedEditorDocPath.string() + "\n");
                } else {
                    editorStatus = "assistant apply save failed";
                    app.AppendLog("[DocOps] Assistant apply save failed: " + loadedEditorDocPath.string() + "\n");
                }
            } else {
                app.AppendLog("[DocOps] Assistant response applied to editor buffer.\n");
            }
        }
    }

    ImGui::Separator();
    ImGui::Text("Editor DocOps:");
    if (workspaceOk) {
        if (ImGui::Button("Atualizar arquivos")) {
            editorDocFiles = ListEditableDocuments(ws);
            editorStatus = "file list refreshed";
        }
        ImGui::SameLine();
        ImGui::Text("Arquivos elegiveis: %zu", editorDocFiles.size());
        ImGui::SameLine();
        const std::string quickCheckCommand = BuildDocTypeCommand(ws, loadedEditorDocRelativePath, false);
        const std::string quickBuildCommand = BuildDocTypeCommand(ws, loadedEditorDocRelativePath, true);
        const bool canRunQuickDocAction = workspaceOk && app.services.taskManager && !running.load();
        if (!canRunQuickDocAction || quickCheckCommand.empty()) ImGui::BeginDisabled();
        if (ImGui::Button("Doc Check", ImVec2(110, 0))) {
            runCommandAsync(quickCheckCommand);
        }
        if (!canRunQuickDocAction || quickCheckCommand.empty()) ImGui::EndDisabled();
        ImGui::SameLine();
        if (!canRunQuickDocAction || quickBuildCommand.empty()) ImGui::BeginDisabled();
        if (ImGui::Button("Doc Build", ImVec2(110, 0))) {
            runCommandAsync(quickBuildCommand);
        }
        if (!canRunQuickDocAction || quickBuildCommand.empty()) ImGui::EndDisabled();
    }

    const float editorPanelHeight = textFocusMode ? 340.0f : 320.0f;
    if (workspaceOk && ImGui::BeginTable("DocOpsEditorTable", 2,
                                         ImGuiTableFlags_Resizable | ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_SizingStretchProp,
                                         ImVec2(0, editorPanelHeight))) {
        ImGui::TableSetupColumn("Arquivos", ImGuiTableColumnFlags_WidthStretch, 0.32f);
        ImGui::TableSetupColumn("Editor", ImGuiTableColumnFlags_WidthStretch, 0.68f);

        ImGui::TableNextColumn();
        if (ImGui::BeginChild("DocOpsEditorFiles", ImVec2(0, 0), true)) {
            for (int index = 0; index < static_cast<int>(editorDocFiles.size()); ++index) {
                const std::string label = editorDocFiles[index].string();
                const bool isSelected = (index == selectedEditorDocIndex);
                if (ImGui::Selectable(label.c_str(), isSelected)) {
                    selectedEditorDocIndex = index;
                    const std::filesystem::path absPath = ws / editorDocFiles[index];
                    std::string content;
                    if (ReadTextFile(absPath, content)) {
                        loadedEditorDocPath = absPath;
                        loadedEditorDocRelativePath = editorDocFiles[index];
                        editorBuffer = content;
                        editorDirty = false;
                        editorStatus = "loaded " + loadedEditorDocRelativePath.string();
                    } else {
                        editorStatus = "failed loading " + label;
                    }
                }
            }
        }
        ImGui::EndChild();

        ImGui::TableNextColumn();
        if (ImGui::BeginChild("DocOpsEditorPane", ImVec2(0, 0), true)) {
            if (loadedEditorDocPath.empty()) {
                ImGui::TextDisabled("Selecione um arquivo .tex/.md/.txt para editar.");
            } else {
                ImGui::Text("Arquivo: %s", loadedEditorDocRelativePath.string().c_str());
                const bool canSaveEditor = !loadedEditorDocPath.empty() && editorDirty;
                if (!canSaveEditor) ImGui::BeginDisabled();
                if (ImGui::Button("Salvar arquivo", ImVec2(120, 0))) {
                    if (WriteTextFile(loadedEditorDocPath, editorBuffer)) {
                        editorDirty = false;
                        editorStatus = "saved " + loadedEditorDocRelativePath.string();
                        app.AppendLog("[DocOps] File saved: " + loadedEditorDocPath.string() + "\n");
                    } else {
                        editorStatus = "save failed " + loadedEditorDocRelativePath.string();
                        app.AppendLog("[DocOps] Save failed: " + loadedEditorDocPath.string() + "\n");
                    }
                }
                if (!canSaveEditor) ImGui::EndDisabled();
                ImGui::SameLine();
                if (ImGui::Button("Recarregar", ImVec2(120, 0))) {
                    std::string freshContent;
                    if (ReadTextFile(loadedEditorDocPath, freshContent)) {
                        editorBuffer = freshContent;
                        editorDirty = false;
                        editorStatus = "reloaded " + loadedEditorDocRelativePath.string();
                    } else {
                        editorStatus = "reload failed " + loadedEditorDocRelativePath.string();
                    }
                }
                ImGui::SameLine();
                if (ImGui::Button("Aplicar ultima IA", ImVec2(140, 0))) {
                    const std::string lastAssistantResponse = GetLastAssistantResponse(app.services.conversationService.get());
                    if (!lastAssistantResponse.empty()) {
                        editorBuffer = ExtractAssistantPayloadForDocument(lastAssistantResponse);
                        editorDirty = true;
                        editorStatus = "assistant response applied";
                    } else {
                        editorStatus = "no assistant response available";
                    }
                }
                ImGui::SameLine();
                if (ImGui::Button("Aplicar + Salvar", ImVec2(140, 0))) {
                    const std::string lastAssistantResponse = GetLastAssistantResponse(app.services.conversationService.get());
                    if (!lastAssistantResponse.empty()) {
                        editorBuffer = ExtractAssistantPayloadForDocument(lastAssistantResponse);
                        if (WriteTextFile(loadedEditorDocPath, editorBuffer)) {
                            editorDirty = false;
                            editorStatus = "assistant applied and saved";
                            app.AppendLog("[DocOps] Assistant response applied and saved: " + loadedEditorDocPath.string() + "\n");
                        } else {
                            editorDirty = true;
                            editorStatus = "assistant apply save failed";
                            app.AppendLog("[DocOps] Assistant apply save failed: " + loadedEditorDocPath.string() + "\n");
                        }
                    } else {
                        editorStatus = "no assistant response available";
                    }
                }
                ImGui::SameLine();
                if (ImGui::Button("Aplicar patch IA (seguro)", ImVec2(190, 0))) {
                    const std::string lastAssistantResponse = GetLastAssistantResponse(app.services.conversationService.get());
                    if (lastAssistantResponse.empty()) {
                        editorStatus = "no assistant response available";
                    } else {
                        const std::string patchCandidate = ExtractUnifiedDiffCandidate(lastAssistantResponse);
                        if (patchCandidate.empty()) {
                            editorStatus = "assistant response has no unified diff patch";
                            app.AppendLog("[DocOps] Patch apply skipped: no unified diff found in assistant response.\n");
                        } else {
                            CommandResult patchResult;
                            if (ApplyUnifiedDiffPatch(ws, patchCandidate, patchResult)) {
                                std::string freshContent;
                                if (ReadTextFile(loadedEditorDocPath, freshContent)) {
                                    editorBuffer = freshContent;
                                    editorDirty = false;
                                }
                                editorStatus = "assistant patch applied";
                                app.AppendLog("[DocOps] Assistant patch applied successfully.\n");
                            } else {
                                editorStatus = "assistant patch apply failed";
                                app.AppendLog("[DocOps] Assistant patch failed (exit " + std::to_string(patchResult.exitCode) + ").\n");
                            }
                            if (!patchResult.output.empty()) {
                                std::lock_guard<std::mutex> lock(outputMutex);
                                output += "\n[DocOps] Patch apply output:\n" + patchResult.output + "\n";
                            }
                        }
                    }
                }
                ImGui::SameLine();
                const std::string compactEditorStatus = ToSingleLineCompact(editorStatus, 84);
                ImGui::TextColored(editorDirty ? ImVec4(1.0f, 0.8f, 0.2f, 1.0f) : ImVec4(0.6f, 0.9f, 0.6f, 1.0f),
                                   "Status: %s", compactEditorStatus.c_str());

                ImGui::InputText("Autor avaliacao##docops_eval_author", evaluationAuthor, sizeof(evaluationAuthor));
                ImGui::SameLine();
                if (ImGui::Button("Submeter avaliacao de conformidade", ImVec2(260, 0))) {
                    const std::filesystem::path evalDir = ws / "docs" / "avaliacoes";
                    std::error_code ec;
                    std::filesystem::create_directories(evalDir, ec);
                    if (ec) {
                        editorStatus = "failed creating docs/avaliacoes";
                        app.AppendLog("[DocOps] Failed creating docs/avaliacoes.\n");
                    } else {
                        const std::string timestamp = CurrentTimestampCompact();
                        const std::string dateIso = CurrentDateIso();
                        const std::filesystem::path evalPath = evalDir / ("AVALIACAO-CONFORMIDADE-" + timestamp + ".md");
                        const std::string baseRef = loadedEditorDocRelativePath.empty()
                            ? "Editor DocOps"
                            : ("Revisao de " + loadedEditorDocRelativePath.string());
                        const std::string scopeRef = loadedEditorDocRelativePath.empty()
                            ? "texto ativo"
                            : loadedEditorDocRelativePath.string();
                        const std::string draft = BuildConformityEvaluationDraft(
                            dateIso,
                            std::string(evaluationAuthor),
                            baseRef,
                            scopeRef
                        );
                        if (WriteTextFile(evalPath, draft)) {
                            editorStatus = "conformity evaluation submitted";
                            app.AppendLog("[DocOps] Conformity evaluation created: " + evalPath.string() + "\n");
                        } else {
                            editorStatus = "conformity evaluation write failed";
                            app.AppendLog("[DocOps] Failed writing conformity evaluation: " + evalPath.string() + "\n");
                        }
                    }
                }
                ImGui::SameLine();
                if (ImGui::Button("Rodar auditoria agora", ImVec2(190, 0))) {
                    const std::filesystem::path auditScript = ws / "scripts" / "audit_evaluations.sh";
                    if (!std::filesystem::exists(auditScript)) {
                        editorStatus = "audit script not found";
                        app.AppendLog("[DocOps] Missing script: " + auditScript.string() + "\n");
                    } else {
                        CommandResult auditResult = RunShellCommandInWorkspace(ws, "bash scripts/audit_evaluations.sh");
                        editorStatus = auditResult.exitCode == 0
                            ? "evaluation audit passed"
                            : "evaluation audit failed";
                        app.AppendLog("[DocOps] Evaluation audit finished with exit code: " + std::to_string(auditResult.exitCode) + "\n");
                        std::lock_guard<std::mutex> lock(outputMutex);
                        output += "\n[DocOps] Evaluation audit output:\n";
                        output += auditResult.output;
                        output += "\n[DocOps] Evaluation audit exit code: " + std::to_string(auditResult.exitCode) + "\n";
                    }
                }

                if (directEditInstruction.empty()) {
                    directEditInstruction = "Refine o texto mantendo consistencia com ADR/DDD e sem mudar compromisso normativo.";
                }
                ImGui::TextDisabled("Instrucao direta para IA (arquivo atual):");
                InputTextMultilineString("##docops_direct_instruction", &directEditInstruction, ImVec2(-FLT_MIN, 62));

                const bool canSendDirectInstruction =
                    app.services.conversationService &&
                    app.services.conversationService->isSessionActive() &&
                    !app.services.conversationService->isThinking() &&
                    !directEditInstruction.empty();
                if (!canSendDirectInstruction) ImGui::BeginDisabled();
                if (ImGui::Button("Enviar instrucao IA", ImVec2(170, 0))) {
                    pendingAssistantBaseline = GetLastAssistantResponse(app.services.conversationService.get());
                    applyPendingFromChat = autoApplyLastAiToEditor;
                    const std::string prompt = BuildDirectEditPrompt(
                        editProfiles[editProfileIndex],
                        loadedEditorDocRelativePath.string(),
                        editReason,
                        editAdrRef,
                        directEditInstruction,
                        editorBuffer
                    );
                    app.services.conversationService->sendMessage(prompt);
                    editorStatus = "direct instruction sent to assistant";
                    app.AppendLog("[DocOps] Direct editor instruction sent to assistant.\n");
                }
                if (!canSendDirectInstruction) ImGui::EndDisabled();
                ImGui::SameLine();
                ImGui::Checkbox("Auto-aplicar", &autoApplyLastAiToEditor);
                ImGui::SameLine();
                ImGui::Checkbox("Auto-salvar", &autoSaveAfterAiApply);
                if (!canSendDirectInstruction) {
                    ImGui::TextDisabled("Para envio direto, inicie sessao DocOps e aguarde IA ficar ociosa.");
                }

                const float minEditorHeight = 150.0f;
                const float maxEditorHeight = textFocusMode ? 230.0f : 260.0f;
                const float editorInputHeight = std::clamp(
                    ImGui::GetContentRegionAvail().y - ImGui::GetTextLineHeightWithSpacing(),
                    minEditorHeight,
                    maxEditorHeight
                );
                if (InputTextMultilineString("##docops_editor_buffer", &editorBuffer, ImVec2(-FLT_MIN, editorInputHeight))) {
                    editorDirty = true;
                    editorStatus = "modified";
                }
            }
        }
        ImGui::EndChild();
        ImGui::EndTable();
    }

    ImGui::Separator();
    ImGui::Text("Ações (presets):");

    const char* presets[] = {
        "doc check (arquivo selecionado)",
        "doc build (arquivo selecionado)",
        "make check",
        "make release-pdf",
        "make release-ort-pdf",
        "make release-rtv-pdf",
        "bash scripts/audit_docops_contract.sh",
        "bash scripts/audit_evaluations.sh",
        "git status --porcelain",
        "Custom command",
    };
    ImGui::Combo("##docops_preset", &presetIndex, presets, IM_ARRAYSIZE(presets));

    if (presetIndex == 9) {
        ImGui::Text("Comando customizado:");
        ImGui::PushItemWidth(-1);
        ImGui::InputText("##docops_custom", customCommand, sizeof(customCommand));
        ImGui::PopItemWidth();
    }

    const std::string selectedCommand = [&]() -> std::string {
        if (presetIndex == 9) return std::string(customCommand);
        if (presetIndex == 0) return BuildDocTypeCommand(ws, loadedEditorDocRelativePath, false);
        if (presetIndex == 1) return BuildDocTypeCommand(ws, loadedEditorDocRelativePath, true);
        return std::string(presets[presetIndex]);
    }();
    if ((presetIndex == 0 || presetIndex == 1) && selectedCommand.empty()) {
        ImGui::TextDisabled("Selecione um arquivo .tex/.md/.txt no editor para usar este preset.");
    }

    const bool canRun = workspaceOk && app.services.taskManager && !running.load() && !selectedCommand.empty();
    if (!canRun) ImGui::BeginDisabled();
    if (ImGui::Button(label("▶️ Executar", "Run"), ImVec2(140, 0))) {
        running.store(true);

        const std::string wsStr = ws.string();
        const std::string cmdStr = selectedCommand;

        {
            std::lock_guard<std::mutex> lock(outputMutex);
            output.clear();
            output += "[DocOps] Workspace: " + wsStr + "\n";
            output += "[DocOps] Command:   " + cmdStr + "\n\n";
        }
        app.AppendLog("[DocOps] Running: " + cmdStr + " (in " + wsStr + ")\n");
        app.SetProcessingStatus("DocOps: running command...");

        app.services.taskManager->SubmitTask(
            application::TaskType::Indexing,
            "DocOps",
            [wsStr, cmdStr, &app](std::shared_ptr<application::TaskStatus> status) {
                status->progress.store(0.1f);

                const std::string cd = "cd \"" + EscapeForDoubleQuotes(wsStr) + "\"";
                const std::string bash = "bash -lc \"" + cd + " && " + cmdStr + " 2>&1\"";

                FILE* pipe = popen(bash.c_str(), "r");
                if (!pipe) {
                    std::lock_guard<std::mutex> lock(outputMutex);
                    output += "ERROR: popen() falhou ao executar comando.\n";
                    lastExitCode = 127;
                    running.store(false);
                    return;
                }

                char buffer[512];
                while (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
                    std::lock_guard<std::mutex> lock(outputMutex);
                    output += buffer;
                }

                int rc = pclose(pipe);
                int exitCode = ExtractExitCode(rc);

                {
                    std::lock_guard<std::mutex> lock(outputMutex);
                    output += "\n[DocOps] Exit code: " + std::to_string(exitCode) + "\n";
                    lastExitCode = exitCode;
                }

                app.AppendLog("[DocOps] Completed with exit code: " + std::to_string(exitCode) + "\n");
                app.SetProcessingStatus("DocOps: done");
                status->progress.store(1.0f);
                running.store(false);
            }
        );
    }
    if (!canRun) ImGui::EndDisabled();

    ImGui::SameLine();
    if (running.load()) {
        ImGui::TextColored(ImVec4(1, 1, 0, 1), "⏳ Em execução...");
    } else {
        ImGui::Text("Exit code: %d", lastExitCode);
    }

    ImGui::Separator();
    ImGui::Text("Saída:");

    std::string snapshot;
    {
        std::lock_guard<std::mutex> lock(outputMutex);
        snapshot = output;
    }

    ImGui::BeginChild("DocOpsOutput", ImVec2(0, 0), true);
    ImGui::TextUnformatted(snapshot.c_str());
    ImGui::EndChild();

    ImGui::EndTabItem();
}

} // namespace ideawalker::ui
