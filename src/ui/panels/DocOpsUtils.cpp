#include "ui/panels/DocOpsUtils.hpp"
#include <algorithm>
#include <cctype>
#include <chrono>
#include <cstring>
#include <fstream>
#include <iomanip>
#include <sstream>

#if !defined(_WIN32)
#include <sys/wait.h>
#endif

namespace ideawalker::ui {

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

std::string TrimWhitespace(std::string text) {
    auto isSpace = [](unsigned char c) { return std::isspace(c) != 0; };
    while (!text.empty() && isSpace(static_cast<unsigned char>(text.front()))) text.erase(text.begin());
    while (!text.empty() && isSpace(static_cast<unsigned char>(text.back()))) text.pop_back();
    return text;
}

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

} // namespace ideawalker::ui
