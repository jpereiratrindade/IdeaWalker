/**
 * @file ConversationPanel.hpp
 * @brief UI Panel for the Cognitive Dialogue Context.
 */

#pragma once
#include "ui/AppState.hpp"

namespace ideawalker::ui {

/**
 * @class ConversationPanel
 * @brief Static helper to draw the conversation panel in the ImGui loop.
 */
class ConversationPanel {
public:
    /**
     * @brief Draws the panel contents (must be called inside an ImGui container).
     * @param app The application state.
     */
    static void DrawContent(AppState& app);
};

} // namespace ideawalker::ui
