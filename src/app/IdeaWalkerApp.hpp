/**
 * @file IdeaWalkerApp.hpp
 * @brief Main application class for IdeaWalker.
 * @author IdeaWalker Team
 * @date 2026-01-07
 */

#pragma once

#include "ui/AppState.hpp"

struct SDL_Window;

namespace ideawalker::app {

/**
 * @class IdeaWalkerApp
 * @brief Orchestrates the application lifecycle, including initialization, the main loop, and shutdown.
 */
class IdeaWalkerApp {
public:
    /**
     * @brief Starts the application main loop.
     * @return Exit code (0 for success).
     */
    int Run();

private:
    /**
     * @brief Initializes SDL, OpenGL, ImGui and the application state.
     * @return True if initialization succeeded.
     */
    bool Init();

    /**
     * @brief Cleans up all resources before exiting.
     */
    void Shutdown();

    ui::AppState m_state; ///< Core application state and project management.
    SDL_Window* m_window = nullptr; ///< SDL window handle.
    void* m_glContext = nullptr; ///< OpenGL context.
    bool m_sdlInitialized = false; ///< Flag indicating SDL initialization status.
    bool m_imguiInitialized = false; ///< Flag indicating ImGui initialization status.
};

} // namespace ideawalker::app
