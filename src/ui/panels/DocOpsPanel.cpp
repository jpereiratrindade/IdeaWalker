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

#include "ui/panels/DocOpsUtils.hpp"

namespace ideawalker::ui {

namespace {
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
    static std::string lastCommandState = "idle";
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
            lastCommandState = "running: " + cmdStr;
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
                    lastCommandState = "fail(127): " + cmdStr;
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
                    lastCommandState = (exitCode == 0 ? "ok(" : "fail(")
                        + std::to_string(exitCode) + "): " + cmdStr;
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
            if (runCommandAsync(quickCheckCommand)) {
                editorStatus = "doc check running";
            } else {
                editorStatus = "doc check unavailable";
            }
        }
        if (!canRunQuickDocAction || quickCheckCommand.empty()) ImGui::EndDisabled();
        ImGui::SameLine();
        if (!canRunQuickDocAction || quickBuildCommand.empty()) ImGui::BeginDisabled();
        if (ImGui::Button("Doc Build", ImVec2(110, 0))) {
            if (runCommandAsync(quickBuildCommand)) {
                editorStatus = "doc build running";
            } else {
                editorStatus = "doc build unavailable";
            }
        }
        if (!canRunQuickDocAction || quickBuildCommand.empty()) ImGui::EndDisabled();
        ImGui::SameLine();
        std::string commandStateSnapshot;
        {
            std::lock_guard<std::mutex> lock(outputMutex);
            commandStateSnapshot = lastCommandState;
        }
        if (!running.load() && (editorStatus == "doc check running" || editorStatus == "doc build running")) {
            editorStatus = commandStateSnapshot;
        }
        if (running.load()) {
            ImGui::TextColored(ImVec4(1, 1, 0, 1), "Executando...");
        } else {
            const std::string compactCommandState = ToSingleLineCompact(commandStateSnapshot, 76);
            ImGui::TextDisabled("%s", compactCommandState.c_str());
        }
    }

    const float docopsAreaHeight = ImGui::GetContentRegionAvail().y;
    const float editorPanelHeight = std::max(
        textFocusMode ? 300.0f : 260.0f,
        docopsAreaHeight * (textFocusMode ? 0.68f : 0.52f)
    );
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

                const float minEditorHeight = textFocusMode ? 220.0f : 160.0f;
                const float editorInputHeight = std::max(
                    minEditorHeight,
                    ImGui::GetContentRegionAvail().y - ImGui::GetTextLineHeightWithSpacing()
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
        "make release-dimension-pdf",
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
