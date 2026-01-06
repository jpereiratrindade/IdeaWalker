#include "ui/UiRenderer.hpp"

#include "application/OrganizerService.hpp"
#include "imgui.h"
#include "imnodes.h"

#include <algorithm>
#include <chrono>
#include <cfloat>
#include <cstdio>
#include <ctime>
#include <filesystem>
#include <string>
#include <thread>
#include <vector>

namespace ideawalker::ui {

namespace {

std::tm ToLocalTime(std::time_t tt) {
    std::tm tm = {};
#if defined(_WIN32)
    localtime_s(&tm, &tt);
#else
    localtime_r(&tt, &tm);
#endif
    return tm;
}

int TextEditCallback(ImGuiInputTextCallbackData* data) {
    if (data->EventFlag == ImGuiInputTextFlags_CallbackResize) {
        auto* str = static_cast<std::string*>(data->UserData);
        str->resize(data->BufTextLen);
        data->Buf = str->data();
    }
    return 0;
}

bool InputTextMultilineString(const char* label, std::string* str, const ImVec2& size, ImGuiInputTextFlags flags = 0) {
    flags |= ImGuiInputTextFlags_CallbackResize;
    if (str->capacity() == 0) {
        str->reserve(1024);
    }
    return ImGui::InputTextMultiline(label, str->data(), str->capacity() + 1, size, flags, TextEditCallback, str);
}

bool TaskCard(const char* id, const std::string& text, float width) {
    ImGuiStyle& style = ImGui::GetStyle();
    float paddingX = style.FramePadding.x;
    float paddingY = style.FramePadding.y;
    float wrapWidth = width - paddingX * 2.0f;
    if (wrapWidth < 1.0f) {
        wrapWidth = 1.0f;
    }

    ImVec2 textSize = ImGui::CalcTextSize(text.c_str(), nullptr, false, wrapWidth);
    float height = textSize.y + paddingY * 2.0f;

    ImGui::PushID(id);
    ImGui::InvisibleButton("##task", ImVec2(width, height));
    bool hovered = ImGui::IsItemHovered();
    bool active = ImGui::IsItemActive();
    bool clicked = ImGui::IsItemClicked();

    ImU32 bg = ImGui::GetColorU32(active ? ImGuiCol_ButtonActive : (hovered ? ImGuiCol_ButtonHovered : ImGuiCol_Button));
    ImU32 border = ImGui::GetColorU32(ImGuiCol_Border);
    ImVec2 min = ImGui::GetItemRectMin();
    ImVec2 max = ImGui::GetItemRectMax();
    ImDrawList* drawList = ImGui::GetWindowDrawList();
    drawList->AddRectFilled(min, max, bg, 4.0f);
    drawList->AddRect(min, max, border, 4.0f);

    ImVec2 textPos(min.x + paddingX, min.y + paddingY);
    drawList->AddText(ImGui::GetFont(), ImGui::GetFontSize(), textPos,
                      ImGui::GetColorU32(ImGuiCol_Text), text.c_str(), nullptr, wrapWidth);

    ImGui::PopID();
    return clicked;
}

std::filesystem::path ResolveBrowsePath(const char* buffer, const std::string& fallbackRoot) {
    namespace fs = std::filesystem;
    std::error_code ec;
    fs::path current;
    if (buffer && buffer[0] != '\0') {
        current = fs::path(buffer);
    } else if (!fallbackRoot.empty()) {
        current = fs::path(fallbackRoot);
    } else {
        current = fs::current_path(ec);
    }

    if (current.empty()) {
        current = fs::current_path(ec);
    }

    if (!fs::exists(current, ec) || !fs::is_directory(current, ec)) {
        fs::path parent = current.parent_path();
        if (!parent.empty() && fs::exists(parent, ec) && fs::is_directory(parent, ec)) {
            current = parent;
        }
    }

    if (current.empty()) {
        current = fs::current_path(ec);
    }

    return current;
}

void SetPathBuffer(char* buffer, size_t bufferSize, const std::filesystem::path& path) {
    if (!buffer || bufferSize == 0) {
        return;
    }
    std::string value = path.string();
    std::snprintf(buffer, bufferSize, "%s", value.c_str());
}

std::vector<std::filesystem::path> GetRootShortcuts() {
    namespace fs = std::filesystem;
    std::vector<fs::path> roots;
    std::error_code ec;

#if defined(_WIN32)
    for (char drive = 'A'; drive <= 'Z'; ++drive) {
        std::string root = std::string(1, drive) + ":\\";
        if (fs::exists(root, ec)) {
            roots.emplace_back(root);
        }
    }
#elif defined(__APPLE__)
    const char* candidates[] = { "/", "/Volumes", "/Users" };
    for (const char* path : candidates) {
        if (fs::exists(path, ec)) {
            roots.emplace_back(path);
        }
    }
#else
    const char* candidates[] = { "/", "/mnt", "/media", "/run/media", "/home" };
    for (const char* path : candidates) {
        if (fs::exists(path, ec)) {
            roots.emplace_back(path);
        }
    }
#endif

    if (roots.empty()) {
        roots.emplace_back("/");
    }
    return roots;
}

void DrawFolderBrowser(const char* id,
                       char* pathBuffer,
                       size_t bufferSize,
                       const std::string& fallbackRoot) {
    namespace fs = std::filesystem;
    ImGui::PushID(id);

    fs::path current = ResolveBrowsePath(pathBuffer, fallbackRoot);
    bool updated = false;

    ImGui::Text("Browse:");
    ImGui::SameLine();
    ImGui::TextUnformatted(current.string().c_str());

    if (ImGui::Button("Up")) {
        if (current.has_parent_path()) {
            current = current.parent_path();
            updated = true;
        }
    }
    ImGui::SameLine();
    if (ImGui::Button("Use Current")) {
        updated = true;
    }

    ImGui::Separator();
    ImGui::Text("Roots:");
    if (ImGui::BeginChild("Roots", ImVec2(0, 70), true)) {
        auto roots = GetRootShortcuts();
        for (const auto& root : roots) {
            std::string label = root.string();
            if (ImGui::Selectable(label.c_str())) {
                current = root;
                updated = true;
            }
        }
    }
    ImGui::EndChild();

    ImGui::Text("Folders:");
    if (ImGui::BeginChild("FolderList", ImVec2(0, 200), true)) {
        std::error_code ec;
        if (!fs::exists(current, ec) || !fs::is_directory(current, ec)) {
            ImGui::TextDisabled("Folder not available.");
        } else {
            std::vector<fs::directory_entry> entries;
            for (const auto& entry : fs::directory_iterator(current, ec)) {
                if (entry.is_directory(ec)) {
                    entries.push_back(entry);
                }
            }
            if (ec) {
                ImGui::TextDisabled("Cannot read folder.");
            } else {
                std::sort(entries.begin(), entries.end(),
                          [](const fs::directory_entry& a, const fs::directory_entry& b) {
                              return a.path().filename().string() < b.path().filename().string();
                          });
                for (const auto& entry : entries) {
                    std::string name = entry.path().filename().string();
                    if (name.empty()) {
                        name = entry.path().string();
                    }
                    if (ImGui::Selectable(name.c_str())) {
                        current = entry.path();
                        updated = true;
                    }
                }
            }
        }
    }
    ImGui::EndChild();

    if (updated) {
        SetPathBuffer(pathBuffer, bufferSize, current);
    }

    ImGui::PopID();
}


void DrawNodeGraph(AppState& app) {
    ImNodes::BeginNodeEditor();

    // 1. Draw Nodes
    for (auto& node : app.graphNodes) {
        ImNodes::SetNodeGridSpacePos(node.id, ImVec2(node.x, node.y));

        ImNodes::BeginNode(node.id);
        
        ImNodes::BeginNodeTitleBar();
        ImGui::TextUnformatted(node.title.c_str());
        ImNodes::EndNodeTitleBar();

        ImNodes::BeginOutputAttribute(node.id << 8); 
        ImGui::Dummy(ImVec2(10, 0));
        ImNodes::EndOutputAttribute();

        ImNodes::BeginInputAttribute((node.id << 8) + 1);
        ImGui::Dummy(ImVec2(10, 0));
        ImNodes::EndInputAttribute();

        ImNodes::EndNode();
    }

    // 2. Draw Links
    for (const auto& link : app.graphLinks) {
        int startAttr = (link.startNode << 8);
        int endAttr = (link.endNode << 8) + 1;
        ImNodes::Link(link.id, startAttr, endAttr);
    }

    ImNodes::EndNodeEditor();
    
    // 3. Update Physics
    if (!app.graphNodes.empty()) {
        app.UpdateGraphPhysics();
    }
}

} // namespace

void DrawUI(AppState& app) {
    bool needRefresh = false;
    if (app.pendingRefresh.exchange(false)) {
        app.RefreshInbox();
        app.RefreshAllInsights();
    }
    auto label = [&app](const char* withEmoji, const char* plain) {
        return app.emojiEnabled ? withEmoji : plain;
    };
    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(viewport->WorkPos);
    ImGui::SetNextWindowSize(viewport->WorkSize);
    ImGui::Begin("Main", NULL, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_MenuBar);
    const bool hasProject = (app.organizerService != nullptr);
    const bool canChangeProject = !app.isProcessing.load();

    if (ImGui::BeginMenuBar()) {
        if (ImGui::BeginMenu("File")) {
            if (ImGui::MenuItem("New Project...", nullptr, false, canChangeProject)) {
                app.showNewProjectModal = true;
            }
            if (ImGui::MenuItem("Open Project...", nullptr, false, canChangeProject)) {
                app.showOpenProjectModal = true;
            }
            if (ImGui::MenuItem("Save Project", nullptr, false, hasProject && canChangeProject)) {
                if (!app.SaveProject()) {
                    app.AppendLog("[SYSTEM] Failed to save project.\n");
                }
            }
            if (ImGui::MenuItem("Save Project As...", nullptr, false, canChangeProject)) {
                app.showSaveAsProjectModal = true;
            }
            if (ImGui::MenuItem("Close Project", nullptr, false, hasProject && canChangeProject)) {
                app.CloseProject();
            }
            ImGui::Separator();
            if (ImGui::MenuItem("Exit", nullptr, false, canChangeProject)) {
                app.requestExit = true;
            }
            ImGui::EndMenu();
        }
        ImGui::EndMenuBar();
    }

    if (app.showNewProjectModal) {
        std::filesystem::path defaultPath = ResolveBrowsePath(nullptr, app.projectRoot);
        SetPathBuffer(app.projectPathBuffer, sizeof(app.projectPathBuffer), defaultPath);
        ImGui::OpenPopup("New Project");
        app.showNewProjectModal = false;
    }
    if (ImGui::BeginPopupModal("New Project", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::Text("Project folder:");
        ImGui::InputText("##newproject", app.projectPathBuffer, sizeof(app.projectPathBuffer));
        DrawFolderBrowser("new_project_browser",
                          app.projectPathBuffer,
                          sizeof(app.projectPathBuffer),
                          app.projectRoot);
        if (ImGui::Button("Create", ImVec2(120, 0))) {
            if (!app.NewProject(app.projectPathBuffer)) {
                app.AppendLog("[SYSTEM] Failed to create project.\n");
            }
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel", ImVec2(120, 0))) {
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }

    if (app.showOpenProjectModal) {
        std::filesystem::path defaultPath = ResolveBrowsePath(nullptr, app.projectRoot);
        SetPathBuffer(app.projectPathBuffer, sizeof(app.projectPathBuffer), defaultPath);
        ImGui::OpenPopup("Open Project");
        app.showOpenProjectModal = false;
    }
    if (ImGui::BeginPopupModal("Open Project", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::Text("Project folder:");
        ImGui::InputText("##openproject", app.projectPathBuffer, sizeof(app.projectPathBuffer));
        DrawFolderBrowser("open_project_browser",
                          app.projectPathBuffer,
                          sizeof(app.projectPathBuffer),
                          app.projectRoot);
        if (ImGui::Button("Open", ImVec2(120, 0))) {
            if (!app.OpenProject(app.projectPathBuffer)) {
                app.AppendLog("[SYSTEM] Failed to open project.\n");
            }
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel", ImVec2(120, 0))) {
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }

    if (app.showSaveAsProjectModal) {
        std::filesystem::path defaultPath = ResolveBrowsePath(nullptr, app.projectRoot);
        SetPathBuffer(app.projectPathBuffer, sizeof(app.projectPathBuffer), defaultPath);
        ImGui::OpenPopup("Save Project As");
        app.showSaveAsProjectModal = false;
    }
    if (ImGui::BeginPopupModal("Save Project As", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::Text("Project folder:");
        ImGui::InputText("##saveprojectas", app.projectPathBuffer, sizeof(app.projectPathBuffer));
        DrawFolderBrowser("save_project_browser",
                          app.projectPathBuffer,
                          sizeof(app.projectPathBuffer),
                          app.projectRoot);
        if (ImGui::Button("Save", ImVec2(120, 0))) {
            if (!app.SaveProjectAs(app.projectPathBuffer)) {
                app.AppendLog("[SYSTEM] Failed to save project as.\n");
            }
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel", ImVec2(120, 0))) {
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }

    if (hasProject) {
        ImGui::TextDisabled("Project: %s", app.projectRoot.c_str());
    } else {
        ImGui::TextDisabled("Nenhum projeto aberto.");
    }
    ImGui::Separator();

    if (ImGui::BeginTabBar("MyTabs")) {
        
        ImGuiTabItemFlags flags0 = (app.requestedTab == 0) ? ImGuiTabItemFlags_SetSelected : 0;
        if (ImGui::BeginTabItem(label("🎙️ Dashboard & Inbox", "Dashboard & Inbox"), NULL, flags0)) {
            if (app.requestedTab == 0) app.requestedTab = -1;
            app.activeTab = 0;
            if (!hasProject) {
                ImGui::TextDisabled("Nenhum projeto aberto.");
                ImGui::TextDisabled("Use File > New Project ou File > Open Project para comecar.");
            } else {
                ImGui::Spacing();
                
                if (ImGui::Button(label("🔄 Refresh Inbox", "Refresh Inbox"), ImVec2(150, 30))) {
                    app.RefreshInbox();
                }
                
                ImGui::Separator();
                
                ImGui::Text("Inbox (%zu ideas):", app.inboxThoughts.size());
                ImGui::BeginChild("InboxList", ImVec2(0, 200), true);
                for (const auto& thought : app.inboxThoughts) {
                    bool isSelected = (app.selectedInboxFilename == thought.filename);
                    if (ImGui::Selectable(thought.filename.c_str(), isSelected)) {
                        app.selectedInboxFilename = thought.filename;
                    }
                }
                ImGui::EndChild();

                ImGui::Spacing();
                
                bool processing = app.isProcessing.load();
                if (processing) ImGui::BeginDisabled();
                auto startBatch = [&app](bool force) {
                    app.isProcessing.store(true);
                    app.AppendLog(force ? "[SYSTEM] Starting AI reprocess (batch)...\n" : "[SYSTEM] Starting AI batch processing...\n");
                    std::thread([&app, force]() {
                        app.organizerService->processInbox(force);
                        bool consolidated = app.organizerService->updateConsolidatedTasks();
                        app.isProcessing.store(false);
                        app.AppendLog("[SYSTEM] Processing finished.\n");
                        app.AppendLog(consolidated ? "[SYSTEM] Consolidated tasks updated.\n" : "[SYSTEM] Consolidated tasks failed.\n");
                        app.pendingRefresh.store(true);
                    }).detach();
                };

                auto startSingle = [&app](const std::string& filename, bool force) {
                    app.isProcessing.store(true);
                    app.AppendLog(force ? "[SYSTEM] Starting AI reprocess for " + filename + "...\n"
                                        : "[SYSTEM] Starting AI processing for " + filename + "...\n");
                    std::thread([&app, filename, force]() {
                        auto result = app.organizerService->processInboxItem(filename, force);
                        switch (result) {
                        case application::OrganizerService::ProcessResult::Processed:
                            app.AppendLog("[SYSTEM] Processing finished for " + filename + ".\n");
                            break;
                        case application::OrganizerService::ProcessResult::SkippedUpToDate:
                            app.AppendLog("[SYSTEM] Skipped (up-to-date): " + filename + ".\n");
                            break;
                        case application::OrganizerService::ProcessResult::NotFound:
                            app.AppendLog("[SYSTEM] Inbox file not found: " + filename + ".\n");
                            break;
                        case application::OrganizerService::ProcessResult::Failed:
                            app.AppendLog("[SYSTEM] Processing failed for " + filename + ".\n");
                            break;
                        }
                        if (result != application::OrganizerService::ProcessResult::NotFound
                            && result != application::OrganizerService::ProcessResult::Failed) {
                            bool consolidated = app.organizerService->updateConsolidatedTasks();
                            app.AppendLog(consolidated ? "[SYSTEM] Consolidated tasks updated.\n" : "[SYSTEM] Consolidated tasks failed.\n");
                        }
                        app.isProcessing.store(false);
                        app.pendingRefresh.store(true);
                    }).detach();
                };

                const bool hasSelection = !app.selectedInboxFilename.empty();
                const char* runLabel = hasSelection
                    ? label("🧠 Run Selected", "Run Selected")
                    : label("🧠 Run AI Orchestrator", "Run AI Orchestrator");
                if (ImGui::Button(runLabel, ImVec2(250, 50))) {
                    if (hasSelection) {
                        startSingle(app.selectedInboxFilename, false);
                    } else {
                        startBatch(false);
                    }
                }
                if (hasSelection) {
                    ImGui::SameLine();
                    if (ImGui::Button(label("🧠 Run All", "Run All"), ImVec2(120, 50))) {
                        startBatch(false);
                    }
                    ImGui::SameLine();
                    if (ImGui::Button(label("🔁 Reprocess Selected", "Reprocess Selected"), ImVec2(180, 50))) {
                        startSingle(app.selectedInboxFilename, true);
                    }
                } else {
                    ImGui::SameLine();
                    if (ImGui::Button(label("🔁 Reprocess All", "Reprocess All"), ImVec2(150, 50))) {
                        startBatch(true);
                    }
                }
                if (processing) ImGui::EndDisabled();

                if (app.isProcessing.load()) {
                    ImGui::SameLine();
                    ImGui::TextColored(ImVec4(1, 1, 0, 1), "⏳ Thinking...");
                }

                ImGui::Separator();
                ImGui::Text("System Log:");
                ImGui::BeginChild("Log", ImVec2(0, -100), true);
                std::string logSnapshot = app.GetLogSnapshot();
                ImGui::TextUnformatted(logSnapshot.c_str());
                if (ImGui::GetScrollY() >= ImGui::GetScrollMaxY())
                    ImGui::SetScrollHereY(1.0f);
                ImGui::EndChild();

                ImGui::Separator();
                ImGui::Text("%s", label("🔥 Activity Heatmap (Last 30 Days)", "Activity Heatmap (Last 30 Days)"));
                
                // Render Heatmap
                ImDrawList* draw_list = ImGui::GetWindowDrawList();
                ImVec2 p = ImGui::GetCursorScreenPos();
                float size = 15.0f;
                float spacing = 3.0f;
                int maxCount = 0;
                for (const auto& entry : app.activityHistory) {
                    if (entry.second > maxCount) maxCount = entry.second;
                }
                const auto now = std::chrono::system_clock::now();
                
                for (int i = 0; i < 30; ++i) {
                    ImVec4 color = ImVec4(0.15f, 0.15f, 0.15f, 1.0f);
                    auto day = now - std::chrono::hours(24 * (29 - i));
                    std::time_t tt = std::chrono::system_clock::to_time_t(day);
                    std::tm tm = ToLocalTime(tt);
                    char buffer[11];
                    std::strftime(buffer, sizeof(buffer), "%Y-%m-%d", &tm);
                    auto it = app.activityHistory.find(buffer);
                    int count = (it != app.activityHistory.end()) ? it->second : 0;
                    if (count > 0) {
                        float intensity = (maxCount > 0) ? (static_cast<float>(count) / maxCount) : 0.0f;
                        float green = 0.3f + (0.7f * intensity);
                        color = ImVec4(0.0f, green, 0.0f, 1.0f);
                    }

                    draw_list->AddRectFilled(ImVec2(p.x + i * (size + spacing), p.y), 
                                            ImVec2(p.x + i * (size + spacing) + size, p.y + size), 
                                            ImColor(color));
                }
                ImGui::Dummy(ImVec2(0, size + 10));
            }

            ImGui::EndTabItem();
        }

        ImGuiTabItemFlags flags1 = (app.requestedTab == 1) ? ImGuiTabItemFlags_SetSelected : 0;
        if (ImGui::BeginTabItem(label("📚 Organized Knowledge", "Organized Knowledge"), NULL, flags1)) {
            if (app.requestedTab == 1) app.requestedTab = -1;
            bool enteringKnowledge = (app.activeTab != 1);
            app.activeTab = 1;
            if (enteringKnowledge && hasProject && !app.isProcessing.load()) {
                app.RefreshAllInsights();
            }
            if (!hasProject) {
                ImGui::TextDisabled("Nenhum projeto aberto.");
                ImGui::TextDisabled("Use File > New Project ou File > Open Project para comecar.");
            } else {
                ImGui::Checkbox("Visao unificada", &app.unifiedKnowledgeView);
                ImGui::Separator();

                if (app.unifiedKnowledgeView) {
                    ImGui::BeginChild("UnifiedKnowledge", ImVec2(0, 0), true);
                    if (app.unifiedKnowledge.empty()) {
                        ImGui::TextDisabled("Nenhum insight disponivel.");
                    } else {
                        InputTextMultilineString("##unified", &app.unifiedKnowledge, ImVec2(-FLT_MIN, -FLT_MIN), ImGuiInputTextFlags_ReadOnly);
                    }
                    ImGui::EndChild();
                } else {
                    // Coluna da Esquerda: Lista de Arquivos
                    ImGui::BeginChild("NotesList", ImVec2(250, 0), true);
                    for (const auto& insight : app.allInsights) {
                        const std::string& filename = insight.getMetadata().id;
                        bool isSelected = (app.selectedFilename == filename);
                        if (ImGui::Selectable(filename.c_str(), isSelected)) {
                            app.selectedFilename = filename;
                            app.selectedNoteContent = insight.getContent();
                            std::snprintf(app.saveAsFilename, sizeof(app.saveAsFilename), "%s", filename.c_str());
                            
                            // Update Domain Insight
                            domain::Insight::Metadata meta;
                            meta.id = filename;
                            app.currentInsight = std::make_unique<domain::Insight>(meta, app.selectedNoteContent);
                            app.currentInsight->parseActionablesFromContent();

                            // Fetch Backlinks
                            if (app.organizerService) {
                                app.currentBacklinks = app.organizerService->getBacklinks(filename);
                            }
                        }
                    }
                    ImGui::EndChild();

                    ImGui::SameLine();

                    // Coluna da Direita: Conteudo / Editor
                    ImGui::BeginChild("NoteContent", ImVec2(0, 0), true);
                    if (!app.selectedFilename.empty()) {
                        if (ImGui::Button(label("💾 Save Changes", "Save Changes"), ImVec2(150, 30))) {
                            app.organizerService->updateNote(app.selectedFilename, app.selectedNoteContent);
                            app.AppendLog("[SYSTEM] Saved changes to " + app.selectedFilename + "\n");
                            app.RefreshAllInsights();
                        }
                        ImGui::SameLine();

                        ImGui::SetNextItemWidth(200.0f);
                        ImGui::InputText("##saveasname", app.saveAsFilename, sizeof(app.saveAsFilename));
                        ImGui::SameLine();
                        if (ImGui::Button(label("📂 Save As", "Save As"), ImVec2(100, 30))) {
                            std::string newName = app.saveAsFilename;
                            if (!newName.empty()) {
                                if (newName.find(".md") == std::string::npos) newName += ".md";
                                app.organizerService->updateNote(newName, app.selectedNoteContent);
                                app.selectedFilename = newName;
                                app.AppendLog("[SYSTEM] Saved as " + newName + "\n");
                                app.RefreshAllInsights();
                            }
                        }

                        ImGui::Separator();
                        
                        // Editor de texto (InputTextMultiline com Resize)
                        if (InputTextMultilineString("##editor", &app.selectedNoteContent, ImVec2(-FLT_MIN, -200))) {
                            if (app.currentInsight) {
                                app.currentInsight->setContent(app.selectedNoteContent);
                                app.currentInsight->parseActionablesFromContent();
                            }
                        }
                        
                        ImGui::Separator();
                        ImGui::Text("%s", label("🔗 Backlinks (Mencionado em):", "Backlinks (Mencionado em):"));
                        if (app.currentBacklinks.empty()) {
                            ImGui::TextDisabled("Nenhuma referencia encontrada.");
                        } else {
                            for (const auto& link : app.currentBacklinks) {
                                if (ImGui::Button(link.c_str())) {
                                    // Logic to switch to this note would go here
                                    // For simplicity, we just log it
                                    app.AppendLog("[UI] Jumping to " + link + "\n");
                                }
                                ImGui::SameLine();
                            }
                            ImGui::NewLine();
                        }
                    } else {
                        ImGui::Text("Select a note from the list to view or edit.");
                    }
                    ImGui::EndChild();
                }
            }

            ImGui::EndTabItem();
        }

        ImGuiTabItemFlags flags2 = (app.requestedTab == 2) ? ImGuiTabItemFlags_SetSelected : 0;
        if (ImGui::BeginTabItem(label("🏭 Execução", "Execução"), NULL, flags2)) {
            if (app.requestedTab == 2) app.requestedTab = -1;
            app.activeTab = 2;
            if (!hasProject) {
                ImGui::TextDisabled("Nenhum projeto aberto.");
                ImGui::TextDisabled("Use File > New Project ou File > Open Project para comecar.");
            } else {
                if (ImGui::Button(label("🔄 Refresh Tasks", "Refresh Tasks"), ImVec2(120, 30))) {
                    app.RefreshAllInsights();
                }
                ImGui::Separator();

                const ImGuiTableFlags tableFlags = ImGuiTableFlags_SizingStretchSame
                    | ImGuiTableFlags_NoBordersInBody
                    | ImGuiTableFlags_NoSavedSettings
                    | ImGuiTableFlags_PadOuterX;
                ImGui::PushStyleVar(ImGuiStyleVar_CellPadding, ImVec2(12.0f, 12.0f));
                ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(8.0f, 8.0f));

                const bool useConsolidatedTasks = app.consolidatedInsight && !app.consolidatedInsight->getActionables().empty();
                std::vector<const domain::Insight*> taskSources;
                if (useConsolidatedTasks) {
                    taskSources.push_back(app.consolidatedInsight.get());
                } else {
                    for (const auto& insight : app.allInsights) {
                        taskSources.push_back(&insight);
                    }
                }

                auto renderColumn = [&](const char* childId,
                                        const char* title,
                                        const ImVec4& titleColor,
                                        const ImVec4& backgroundColor,
                                        auto shouldShow,
                                        bool completed,
                                        bool inProgress) {
                    ImGui::TextColored(titleColor, "%s", title);
                    ImGui::Dummy(ImVec2(0.0f, 4.0f));
                    ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 6.0f);
                    ImGui::PushStyleVar(ImGuiStyleVar_ChildBorderSize, 1.0f);
                    ImGui::PushStyleColor(ImGuiCol_ChildBg, backgroundColor);
                    ImGui::BeginChild(childId, ImVec2(0, 0), true);
                    for (const auto* insight : taskSources) {
                        const auto& actionables = insight->getActionables();
                        for (size_t i = 0; i < actionables.size(); ++i) {
                            if (!shouldShow(actionables[i])) {
                                continue;
                            }
                            std::string itemId = std::string(childId) + insight->getMetadata().id + std::to_string(i);
                            float cardWidth = ImGui::GetContentRegionAvail().x;
                            if (cardWidth < 1.0f) {
                                cardWidth = 1.0f;
                            }
                            if (TaskCard(itemId.c_str(), actionables[i].description, cardWidth)) {
                                app.selectedFilename = insight->getMetadata().id;
                                app.selectedNoteContent = insight->getContent();

                                domain::Insight::Metadata meta;
                                meta.id = app.selectedFilename;
                                app.currentInsight = std::make_unique<domain::Insight>(meta, app.selectedNoteContent);
                                app.currentInsight->parseActionablesFromContent();
                            }
                            if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort)) {
                                ImGui::BeginTooltip();
                                if (useConsolidatedTasks) {
                                    ImGui::Text("Origem: consolidado");
                                } else {
                                    ImGui::Text("Origem: %s", insight->getMetadata().id.c_str());
                                }
                                ImGui::EndTooltip();
                            }

                            if (ImGui::BeginDragDropSource()) {
                                std::string payload = insight->getMetadata().id + "|" + std::to_string(i);
                                ImGui::SetDragDropPayload("DND_TASK", payload.c_str(), payload.size() + 1);
                                ImGui::Text("Movendo: %s", actionables[i].description.c_str());
                                ImGui::EndDragDropSource();
                            }
                        }
                    }
                    ImGui::EndChild();
                    if (app.organizerService && ImGui::BeginDragDropTarget()) {
                        if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("DND_TASK")) {
                            std::string data = static_cast<const char*>(payload->Data);
                            size_t sep = data.find('|');
                            std::string filename = data.substr(0, sep);
                            int index = std::stoi(data.substr(sep + 1));
                            app.organizerService->setTaskStatus(filename, index, completed, inProgress);
                            needRefresh = true;
                        }
                        ImGui::EndDragDropTarget();
                    }
                    ImGui::PopStyleColor();
                    ImGui::PopStyleVar(2);
                };

                if (ImGui::BeginTable("ExecutionColumns", 3, tableFlags)) {
                    ImGui::TableNextRow();
                    ImGui::TableNextColumn();
                    renderColumn(
                        "todo",
                        label("📋 A FAZER", "A FAZER"),
                        ImVec4(1, 0.8f, 0, 1),
                        ImVec4(0.08f, 0.09f, 0.10f, 1.0f),
                        [](const domain::Actionable& task) { return !task.isCompleted && !task.isInProgress; },
                        false,
                        false
                    );

                    ImGui::TableNextColumn();
                    renderColumn(
                        "progress",
                        label("⏳ EM ANDAMENTO", "EM ANDAMENTO"),
                        ImVec4(0, 0.7f, 1, 1),
                        ImVec4(0.07f, 0.09f, 0.11f, 1.0f),
                        [](const domain::Actionable& task) { return task.isInProgress; },
                        false,
                        true
                    );

                    ImGui::TableNextColumn();
                    renderColumn(
                        "done",
                        label("✅ FEITO", "FEITO"),
                        ImVec4(0, 1, 0, 1),
                        ImVec4(0.07f, 0.10f, 0.08f, 1.0f),
                        [](const domain::Actionable& task) { return task.isCompleted; },
                        true,
                        false
                    );

                    ImGui::EndTable();
                }

                ImGui::PopStyleVar(2);
            }
            ImGui::EndTabItem();
        }
        
        ImGuiTabItemFlags flags3 = (app.requestedTab == 3) ? ImGuiTabItemFlags_SetSelected : 0;
        if (ImGui::BeginTabItem(label("🕸️ Neural Web", "Neural Web"), NULL, flags3)) {
            if (app.requestedTab == 3) app.requestedTab = -1;
            bool enteringGraph = (app.activeTab != 3);
            app.activeTab = 3;

            if (enteringGraph && hasProject) {
                app.RebuildGraph();
            }

            if (!hasProject) {
                ImGui::TextDisabled("Nenhum projeto aberto.");
            } else {
                DrawNodeGraph(app);
            }
            ImGui::EndTabItem();
        }
        
        ImGui::EndTabBar();
    }

    ImGui::End();

    if (needRefresh) {
        app.RefreshAllInsights();
    }
}

} // namespace ideawalker::ui
