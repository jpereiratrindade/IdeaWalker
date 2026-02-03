/**
 * @file IdeaWalkerApp.cpp
 * @brief Implementation of the IdeaWalkerApp class.
 */
#include "app/IdeaWalkerApp.hpp"

#include "ui/UiRenderer.hpp"

#include "imnodes.h"
#include "imgui.h"
#include "imgui_impl_opengl3.h"
#include "imgui_impl_sdl2.h"

#include <SDL2/SDL.h>
#include <SDL2/SDL_opengl.h>

#include <cstdio>
#include <iostream>
#include <filesystem>
#include <string>
#include <vector>
#include "infrastructure/ConfigLoader.hpp"
#include "infrastructure/FileRepository.hpp"
#include "infrastructure/OllamaAdapter.hpp"
#include "infrastructure/WhisperCppAdapter.hpp"
#include "infrastructure/PathUtils.hpp"
#include "infrastructure/PersistenceService.hpp"
#include "infrastructure/FileSystemArtifactScanner.hpp"
#include "infrastructure/writing/WritingEventStoreFs.hpp"
#include "infrastructure/writing/WritingTrajectoryRepositoryFs.hpp"
#include "application/AppServices.hpp"

namespace ideawalker::app {

namespace {

std::string FindFontPath(const std::vector<const char*>& candidates) {
    for (const char* path : candidates) {
        if (std::filesystem::exists(path)) {
            return path;
        }
    }
    return {};
}

bool LoadFonts(ImGuiIO& io) {
    const float baseFontSize = 16.0f;
    bool emojiLoaded = false;

#if defined(_WIN32)
    const std::vector<const char*> baseCandidates = {
        "assets/fonts/NotoEmoji-Regular.ttf", // Prioritize local asset
        "C:\\\\Windows\\\\Fonts\\\\segoeui.ttf",
    };
    const std::vector<const char*> emojiCandidates = {
        "assets/fonts/NotoEmoji-Regular.ttf", // Prioritize local asset
        "C:\\\\Windows\\\\Fonts\\\\seguiemj.ttf",
    };
#elif defined(__APPLE__)
    const std::vector<const char*> baseCandidates = {
        "assets/fonts/NotoEmoji-Regular.ttf",
        "/System/Library/Fonts/Supplemental/Arial.ttf",
    };
    const std::vector<const char*> emojiCandidates = {
        "assets/fonts/NotoEmoji-Regular.ttf",
        "/System/Library/Fonts/Apple Color Emoji.ttc",
    };
#else
    const std::vector<const char*> baseCandidates = {
        "assets/fonts/NotoEmoji-Regular.ttf",
        "/usr/share/fonts/truetype/noto/NotoSans-Regular.ttf",
        "/usr/share/fonts/TTF/NotoSans-Regular.ttf",
        "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",
    };
    const std::vector<const char*> emojiCandidates = {
        "assets/fonts/NotoEmoji-Regular.ttf",
        "/usr/share/fonts/google-noto-emoji-fonts/NotoEmoji-Regular.ttf",
        "/usr/share/fonts/truetype/noto/NotoEmoji-Regular.ttf",
        "/usr/share/fonts/TTF/NotoEmoji-Regular.ttf",
        "/usr/share/fonts/google-noto-color-emoji-fonts/Noto-COLRv1.ttf",
        "/usr/share/fonts/truetype/noto/NotoColorEmoji.ttf",
    };
#endif

    std::string basePath = FindFontPath(baseCandidates);
    ImFont* baseFont = nullptr;
    if (!basePath.empty()) {
        baseFont = io.Fonts->AddFontFromFileTTF(basePath.c_str(), baseFontSize);
    }
    if (!baseFont) {
        baseFont = io.Fonts->AddFontDefault();
    }
    io.FontDefault = baseFont;

    std::string emojiPath = FindFontPath(emojiCandidates);
    if (!emojiPath.empty()) {
        std::cout << "[IdeaWalkerApp] Found Emoji Font: " << emojiPath << std::endl;
        ImFontConfig config;
        config.MergeMode = true;
        config.PixelSnapH = true;

        static const ImWchar emojiRanges[] = {
            0x00A0, 0x00FF,   // Latin-1 Supplement (Accents)
            0x2000, 0x3000,   // General Punctuation, Symbols, Dingbats, Technical (includes Gear 2699)
            0x1F300, 0x1FAFF, // Emoji ranges
            0
        };

        ImFontGlyphRangesBuilder builder;
        builder.AddRanges(io.Fonts->GetGlyphRangesDefault());
        builder.AddRanges(emojiRanges);
        static ImVector<ImWchar> ranges; // Static to persist until atlas build
        ranges.clear();
        builder.BuildRanges(&ranges);

        ImFont* emojiFont = io.Fonts->AddFontFromFileTTF(emojiPath.c_str(), baseFontSize, &config, ranges.Data);
        emojiLoaded = (emojiFont != nullptr);
    } else {
        std::cerr << "[IdeaWalkerApp] WARNING: No Emoji font found in system paths. Icons will be disabled." << std::endl;
    }

    return emojiLoaded;
}

} // namespace

bool IdeaWalkerApp::Init() {
    std::string defaultRoot = std::filesystem::current_path().string();
    if (!m_state.OpenProject(defaultRoot)) {
        std::fprintf(stderr, "Failed to initialize folder structure at %s\n", defaultRoot.c_str());
        return false;
    }

    // Dependency Injection / Composition Root
    auto root = std::filesystem::path(defaultRoot);
    auto repo = std::make_unique<infrastructure::FileRepository>(
        (root / "inbox").string(), 
        (root / "notas").string(),
        (root / ".history").string()
    );
    auto sharedAi = std::make_shared<infrastructure::OllamaAdapter>();
    sharedAi->initialize();

    auto taskManager = std::make_shared<application::AsyncTaskManager>();
    
    auto modelsDir = infrastructure::PathUtils::GetModelsDir();
    std::string modelPath = (modelsDir / "ggml-base.bin").string();
    if (!std::filesystem::exists(modelPath)) {
         if (std::filesystem::exists(root / "ggml-base.bin")) {
             modelPath = (root / "ggml-base.bin").string();
         }
    }
    std::string inboxPath = (root / "inbox").string();
    auto transcriber = std::make_unique<infrastructure::WhisperCppAdapter>(modelPath, inboxPath);

    application::AppServices services;
    auto knowledge = std::make_unique<application::KnowledgeService>(std::move(repo));
    auto processing = std::make_unique<application::AIProcessingService>(*knowledge, sharedAi, taskManager, std::move(transcriber));
    
    services.knowledgeService = std::move(knowledge);
    services.aiProcessingService = std::move(processing);
    services.persistenceService = std::make_shared<infrastructure::PersistenceService>();
    services.conversationService = std::make_unique<application::ConversationService>(sharedAi, services.persistenceService, root.string());

    auto scanPath = (root / "inbox").string();
    auto obsPath = (root / "observations").string();
    auto scanner = std::make_unique<infrastructure::FileSystemArtifactScanner>(scanPath);
    services.ingestionService = std::make_unique<application::DocumentIngestionService>(std::move(scanner), sharedAi, obsPath);

    services.contextAssembler = std::make_unique<application::ContextAssembler>(*services.knowledgeService, *services.ingestionService);
    services.suggestionService = std::make_unique<application::SuggestionService>(sharedAi, root.string());

    auto eventStore = std::make_unique<infrastructure::writing::WritingEventStoreFs>(root.string(), services.persistenceService);
    auto trajRepo = std::make_shared<infrastructure::writing::WritingTrajectoryRepositoryFs>(std::move(eventStore));
    services.writingTrajectoryService = std::make_unique<application::writing::WritingTrajectoryService>(std::move(trajRepo));
    services.graphService = std::make_unique<application::GraphService>();
    services.projectService = std::make_unique<application::ProjectService>();
    services.exportService = std::make_unique<application::KnowledgeExportService>();
    services.taskManager = taskManager;

    m_state.InjectServices(std::move(services));

    // Unified Config Loader
    auto videoDriver = infrastructure::ConfigLoader::GetVideoDriverPreference(defaultRoot);
    if (videoDriver) {
        if (*videoDriver == "x11") {
            std::cout << "[IdeaWalkerApp] Enforcing X11 Video Driver via settings.json" << std::endl;
            SDL_SetHint(SDL_HINT_VIDEODRIVER, "x11");
        } else if (*videoDriver == "wayland") {
            std::cout << "[IdeaWalkerApp] Enforcing Wayland Video Driver via settings.json" << std::endl;
            SDL_SetHint(SDL_HINT_VIDEODRIVER, "wayland");
        }
    }

    // Proactive stability fix for Wayland (Client-side decorations) - REVERTED due to crashes
    // SDL_SetHint(SDL_HINT_VIDEO_WAYLAND_ALLOW_LIBDECOR, "1");

    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER | SDL_INIT_GAMECONTROLLER) != 0) {
        std::fprintf(stderr, "SDL_Init failed: %s\n", SDL_GetError());
        return false;
    }
    m_sdlInitialized = true;

    const char* glsl_version = "#version 130";
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, 0);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);

    SDL_WindowFlags window_flags = (SDL_WindowFlags)(SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI);
    m_window = SDL_CreateWindow("Idea Walker v0.1.5-beta", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 1280, 720, window_flags);
    if (!m_window) {
        std::fprintf(stderr, "SDL_CreateWindow failed: %s\n", SDL_GetError());
        return false;
    }

    m_glContext = SDL_GL_CreateContext(m_window);
    if (!m_glContext) {
        std::fprintf(stderr, "SDL_GL_CreateContext failed: %s\n", SDL_GetError());
        return false;
    }
    SDL_GL_MakeCurrent(m_window, m_glContext);
    SDL_GL_SetSwapInterval(1);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    m_state.ui.emojiEnabled = LoadFonts(io);
    ImGui::StyleColorsDark();

    ImNodes::CreateContext();
    ImNodes::StyleColorsDark();
    m_state.InitImNodes();

    if (!ImGui_ImplSDL2_InitForOpenGL(m_window, m_glContext)) {
        std::fprintf(stderr, "ImGui_ImplSDL2_InitForOpenGL failed.\n");
        return false;
    }
    if (!ImGui_ImplOpenGL3_Init(glsl_version)) {
        std::fprintf(stderr, "ImGui_ImplOpenGL3_Init failed.\n");
        return false;
    }
    m_imguiInitialized = true;

    return true;
}

void IdeaWalkerApp::Shutdown() {
    if (m_imguiInitialized) {
        ImGui_ImplOpenGL3_Shutdown();
        ImGui_ImplSDL2_Shutdown();
        m_state.ShutdownImNodes();
        ImNodes::DestroyContext();
        ImGui::DestroyContext();
        m_imguiInitialized = false;
    }

    if (m_glContext) {
        SDL_GL_DeleteContext(m_glContext);
        m_glContext = nullptr;
    }
    if (m_window) {
        SDL_DestroyWindow(m_window);
        m_window = nullptr;
    }
    if (m_sdlInitialized) {
        SDL_Quit();
        m_sdlInitialized = false;
    }
}

int IdeaWalkerApp::Run() {
    if (!Init()) {
        Shutdown();
        return -1;
    }

    bool done = false;
    while (!done) {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            ImGui_ImplSDL2_ProcessEvent(&event);
            if (event.type == SDL_QUIT) done = true;
            if (event.type == SDL_DROPFILE) {
                char* dropped_file = event.drop.file;
                m_state.HandleFileDrop(dropped_file);
                SDL_free(dropped_file);
            }
        }

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplSDL2_NewFrame();
        ImGui::NewFrame();

        ui::DrawUI(m_state);
        if (m_state.ui.requestExit) {
            done = true;
        }

        ImGui::Render();
        ImGuiIO& io = ImGui::GetIO();
        glViewport(0, 0, static_cast<int>(io.DisplaySize.x), static_cast<int>(io.DisplaySize.y));
        glClearColor(0.10f, 0.10f, 0.10f, 1.00f);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        SDL_GL_SwapWindow(m_window);
    }

    Shutdown();
    return 0;
}

} // namespace ideawalker::app
