#include "ui/panels/MainPanels.hpp"
#include "imgui.h"

#include <atomic>
#include <cstring>
#include <cstdio>
#include <filesystem>
#include <mutex>
#include <string>

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
    if (!workspaceOk) {
        ImGui::TextColored(ImVec4(1, 0.5f, 0.0f, 1), "Workspace inválido ou inexistente.");
    }

    ImGui::Separator();
    ImGui::Text("Ações (presets):");

    const char* presets[] = {
        "make check",
        "make release-pdf",
        "make release-ort-pdf",
        "make release-rtv-pdf",
        "git status --porcelain",
        "Custom command",
    };
    ImGui::Combo("##docops_preset", &presetIndex, presets, IM_ARRAYSIZE(presets));

    if (presetIndex == 5) {
        ImGui::Text("Comando customizado:");
        ImGui::PushItemWidth(-1);
        ImGui::InputText("##docops_custom", customCommand, sizeof(customCommand));
        ImGui::PopItemWidth();
    }

    const std::string selectedCommand = [&]() -> std::string {
        if (presetIndex == 5) return std::string(customCommand);
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
