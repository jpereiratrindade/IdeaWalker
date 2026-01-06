#pragma once

#include "ui/AppState.hpp"

struct SDL_Window;

namespace ideawalker::app {

class IdeaWalkerApp {
public:
    int Run();

private:
    bool Init();
    void Shutdown();

    ui::AppState m_state;
    SDL_Window* m_window = nullptr;
    void* m_glContext = nullptr;
    bool m_sdlInitialized = false;
    bool m_imguiInitialized = false;
};

} // namespace ideawalker::app
