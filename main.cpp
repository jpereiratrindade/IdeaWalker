#include "imgui.h"
#include "imgui_impl_sdl2.h"
#include "imgui_impl_opengl3.h"
#include <SDL2/SDL.h>
#include <SDL2/SDL_opengl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string>
#include <vector>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <thread>
#include <atomic>
#include <memory>
#include <cstring>
#include <cfloat>

#include "application/OrganizerService.hpp"
#include "infrastructure/FileRepository.hpp"
#include "infrastructure/OllamaAdapter.hpp"

namespace fs = std::filesystem;
using namespace ideawalker;

// === ESTADO GLOBAL DA APLICAÇÃO ===
struct AppState {
    std::string outputLog = "Idea Walker v0.1.0-alpha - DDD Core initialized.\n";
    std::string selectedNoteContent = "";
    std::string selectedFilename = "";
    char saveAsFilename[128] = "";
    bool isProcessing = false;
    
    std::unique_ptr<domain::Insight> currentInsight;
    std::unique_ptr<application::OrganizerService> organizerService;
    std::vector<domain::RawThought> inboxThoughts;
    std::map<std::string, int> activityHistory;
    std::vector<std::string> currentBacklinks;

    void RefreshInbox() {
        if (organizerService) {
            inboxThoughts = organizerService->getRawThoughts();
        }
    }
};

AppState app;

void DrawUI() {
    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(viewport->WorkPos);
    ImGui::SetNextWindowSize(viewport->WorkSize);
    ImGui::Begin("Main", NULL, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize);

    if (ImGui::BeginTabBar("MyTabs")) {
        
        if (ImGui::BeginTabItem("🎙️ Dashboard & Inbox")) {
            ImGui::Spacing();
            
            if (ImGui::Button("🔄 Refresh Inbox", ImVec2(150, 30))) {
                app.RefreshInbox();
            }
            
            ImGui::Separator();
            
            ImGui::Text("Inbox (%zu ideas):", app.inboxThoughts.size());
            ImGui::BeginChild("InboxList", ImVec2(0, 200), true);
            for (const auto& thought : app.inboxThoughts) {
                ImGui::BulletText("%s", thought.filename.c_str());
            }
            ImGui::EndChild();

            ImGui::Spacing();
            
            bool processing = app.isProcessing;
            if (processing) ImGui::BeginDisabled();
            if (ImGui::Button("🧠 Run AI Orchestrator", ImVec2(250, 50))) {
                app.isProcessing = true;
                app.outputLog += "[SYSTEM] Starting AI batch processing...\n";
                std::thread([]() {
                    app.organizerService->processInbox();
                    app.isProcessing = false;
                    app.outputLog += "[SYSTEM] Processing finished.\n";
                }).detach();
            }
            if (processing) ImGui::EndDisabled();

            if (app.isProcessing) {
                ImGui::SameLine();
                ImGui::TextColored(ImVec4(1, 1, 0, 1), "⏳ Thinking...");
            }

            ImGui::Separator();
            ImGui::Text("System Log:");
            ImGui::BeginChild("Log", ImVec2(0, -100), true);
            ImGui::TextUnformatted(app.outputLog.c_str());
            if (ImGui::GetScrollY() >= ImGui::GetScrollMaxY())
                ImGui::SetScrollHereY(1.0f);
            ImGui::EndChild();

            ImGui::Separator();
            ImGui::Text("🔥 Activity Heatmap (Last 30 Days)");
            
            // Render Heatmap
            ImDrawList* draw_list = ImGui::GetWindowDrawList();
            ImVec2 p = ImGui::GetCursorScreenPos();
            float size = 15.0f;
            float spacing = 3.0f;
            
            for (int i = 0; i < 30; ++i) {
                ImVec4 color = ImVec4(0.15f, 0.15f, 0.15f, 1.0f);
                if (i % 3 == 0) color = ImVec4(0.0f, 0.4f, 0.0f, 1.0f);
                if (i % 7 == 0) color = ImVec4(0.0f, 0.7f, 0.0f, 1.0f);
                if (i == 29) color = ImVec4(0.0f, 1.0f, 0.0f, 1.0f);

                draw_list->AddRectFilled(ImVec2(p.x + i * (size + spacing), p.y), 
                                        ImVec2(p.x + i * (size + spacing) + size, p.y + size), 
                                        ImColor(color));
            }
            ImGui::Dummy(ImVec2(0, size + 10));

            ImGui::EndTabItem();
        }

        if (ImGui::BeginTabItem("📚 Organized Knowledge")) {
            // Coluna da Esquerda: Lista de Arquivos
            ImGui::BeginChild("NotesList", ImVec2(250, 0), true);
            if (fs::exists("./notas")) {
                for (const auto& entry : fs::directory_iterator("./notas")) {
                    if (entry.is_regular_file() && entry.path().extension() == ".md") {
                        std::string filename = entry.path().filename().string();
                        bool isSelected = (app.selectedFilename == filename);
                        if (ImGui::Selectable(filename.c_str(), isSelected)) {
                            app.selectedFilename = filename;
                            std::ifstream file(entry.path());
                            std::stringstream buffer;
                            buffer << file.rdbuf();
                            app.selectedNoteContent = buffer.str();
                            strncpy(app.saveAsFilename, filename.c_str(), sizeof(app.saveAsFilename)-1);
                            
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
                }
            }
            ImGui::EndChild();

            ImGui::SameLine();

            // Coluna da Direita: Conteúdo / Editor
            ImGui::BeginChild("NoteContent", ImVec2(0, 0), true);
            if (!app.selectedFilename.empty()) {
                if (ImGui::Button("💾 Save Changes", ImVec2(150, 30))) {
                    app.organizerService->updateNote(app.selectedFilename, app.selectedNoteContent);
                    app.outputLog += "[SYSTEM] Saved changes to " + app.selectedFilename + "\n";
                }
                ImGui::SameLine();
                
                ImGui::SetNextItemWidth(200.0f);
                ImGui::InputText("##saveasname", app.saveAsFilename, sizeof(app.saveAsFilename));
                ImGui::SameLine();
                if (ImGui::Button("📂 Save As", ImVec2(100, 30))) {
                    std::string newName = app.saveAsFilename;
                    if (!newName.empty()) {
                        if (newName.find(".md") == std::string::npos) newName += ".md";
                        app.organizerService->updateNote(newName, app.selectedNoteContent);
                        app.selectedFilename = newName;
                        app.outputLog += "[SYSTEM] Saved as " + newName + "\n";
                    }
                }
                
                ImGui::Separator();
                
                // Editor de texto (InputTextMultiline com Resize)
                // Usamos um buffer temporário largo o suficiente
                static char textBuffer[64 * 1024]; 
                static std::string lastLoadedFile = "";
                
                if (lastLoadedFile != app.selectedFilename) {
                    strncpy(textBuffer, app.selectedNoteContent.c_str(), sizeof(textBuffer)-1);
                    lastLoadedFile = app.selectedFilename;
                }

                if (ImGui::InputTextMultiline("##editor", textBuffer, sizeof(textBuffer), ImVec2(-FLT_MIN, -200))) {
                    app.selectedNoteContent = textBuffer;
                    if (app.currentInsight) {
                        app.currentInsight->setContent(app.selectedNoteContent);
                        app.currentInsight->parseActionablesFromContent();
                    }
                }
                
                ImGui::Separator();
                ImGui::Text("🔗 Backlinks (Mencionado em):");
                if (app.currentBacklinks.empty()) {
                    ImGui::TextDisabled("Nenhuma referência encontrada.");
                } else {
                    for (const auto& link : app.currentBacklinks) {
                        if (ImGui::Button(link.c_str())) {
                            // Logic to switch to this note would go here
                            // For simplicity, we just log it
                            app.outputLog += "[UI] Jumping to " + link + "\n";
                        }
                        ImGui::SameLine();
                    }
                    ImGui::NewLine();
                }
            } else {
                ImGui::Text("Select a note from the list to view or edit.");
            }
            ImGui::EndChild();

            ImGui::EndTabItem();
        }

        if (ImGui::BeginTabItem("🏭 Execução")) {
            if (app.currentInsight) {
                const auto& actionables = app.currentInsight->getActionables();
                if (actionables.empty()) {
                    ImGui::Text("Nenhuma tarefa extraída deste insight.");
                } else {
                    ImGui::TextColored(ImVec4(0, 1, 0, 1), "Extrações automáticas (Checkboxes):");
                    ImGui::Separator();
                    
                    for (size_t i = 0; i < actionables.size(); ++i) {
                        bool completed = actionables[i].isCompleted;
                        if (ImGui::Checkbox((actionables[i].description + "##" + std::to_string(i)).c_str(), &completed)) {
                            app.currentInsight->toggleActionable(i);
                            app.selectedNoteContent = app.currentInsight->getContent();
                            // Persist change to file
                            app.organizerService->updateNote(app.selectedFilename, app.selectedNoteContent);
                            app.outputLog += "[SYSTEM] Task toggled and note updated: " + app.selectedFilename + "\n";
                        }
                    }
                }
            } else {
                ImGui::Text("Selecione uma nota primeiro.");
            }
            ImGui::EndTabItem();
        }
        
        ImGui::EndTabBar();
    }

    ImGui::End();
}

int main(int, char**) {
    // DDD Initialization
    auto repo = std::make_unique<infrastructure::FileRepository>("./inbox", "./notas");
    auto ai = std::make_unique<infrastructure::OllamaAdapter>();
    app.organizerService = std::make_unique<application::OrganizerService>(std::move(repo), std::move(ai));
    app.RefreshInbox();

    // SDL + OpenGL Boilerplate
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER | SDL_INIT_GAMECONTROLLER) != 0) return -1;

    const char* glsl_version = "#version 130";
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, 0);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);

    SDL_WindowFlags window_flags = (SDL_WindowFlags)(SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI);
    SDL_Window* window = SDL_CreateWindow("Idea Walker v0.1.0-alpha", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 1280, 720, window_flags);
    SDL_GLContext gl_context = SDL_GL_CreateContext(window);
    SDL_GL_MakeCurrent(window, gl_context);
    SDL_GL_SetSwapInterval(1);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    ImGui::StyleColorsDark();

    ImGui_ImplSDL2_InitForOpenGL(window, gl_context);
    ImGui_ImplOpenGL3_Init(glsl_version);

    bool done = false;
    while (!done) {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            ImGui_ImplSDL2_ProcessEvent(&event);
            if (event.type == SDL_QUIT) done = true;
        }

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplSDL2_NewFrame();
        ImGui::NewFrame();

        DrawUI();

        ImGui::Render();
        glViewport(0, 0, (int)io.DisplaySize.x, (int)io.DisplaySize.y);
        glClearColor(0.10f, 0.10f, 0.10f, 1.00f);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        SDL_GL_SwapWindow(window);
    }

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplSDL2_Shutdown();
    ImGui::DestroyContext();
    SDL_GL_DeleteContext(gl_context);
    SDL_DestroyWindow(window);
    SDL_Quit();

    return 0;
}