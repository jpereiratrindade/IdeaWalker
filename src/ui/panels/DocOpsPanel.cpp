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
            "  write_mode: \"proposal_only\"\n";
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

    static std::mutex outputMutex;
    static std::string output;
    static std::atomic<bool> running{false};
    static int lastExitCode = 0;

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

    ImGui::Separator();
    ImGui::Text("Assistencia de edicao (MVP):");
    const char* editProfiles[] = {"architect", "author", "editor", "reviewer"};
    ImGui::Combo("Perfil##docops_edit_profile", &editProfileIndex, editProfiles, IM_ARRAYSIZE(editProfiles));
    ImGui::InputText("Arquivo alvo##docops_edit_target", editTargetFile, sizeof(editTargetFile));
    ImGui::InputText("Motivo##docops_edit_reason", editReason, sizeof(editReason));
    ImGui::InputText("ADR referencia##docops_edit_adr", editAdrRef, sizeof(editAdrRef));

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

    ImGui::Separator();
    ImGui::Text("ADR helper:");
    std::vector<std::string> adrIds = ListAdrIds(ws);
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

    ImGui::Separator();
    ImGui::Text("LLM bridge (chat):");
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
        app.services.conversationService->sendMessage(llmPromptDraft);
        app.AppendLog("[DocOps] Prompt sent to Conversation chat.\n");
    }
    if (!canSendChat) ImGui::EndDisabled();
    if (!canSendChat) {
        ImGui::TextDisabled("Para enviar ao LLM, inicie uma sessao no chat (painel inferior).");
    }

    ImGui::Separator();
    ImGui::Text("Ações (presets):");

    const char* presets[] = {
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

    if (presetIndex == 7) {
        ImGui::Text("Comando customizado:");
        ImGui::PushItemWidth(-1);
        ImGui::InputText("##docops_custom", customCommand, sizeof(customCommand));
        ImGui::PopItemWidth();
    }

    const std::string selectedCommand = [&]() -> std::string {
        if (presetIndex == 7) return std::string(customCommand);
        return std::string(presets[presetIndex]);
    }();

    const bool canRun = workspaceOk && app.services.taskManager && !running.load();
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
