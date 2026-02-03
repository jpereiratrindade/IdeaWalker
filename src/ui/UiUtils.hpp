#pragma once

#include "ui/AppState.hpp"
#include "imgui.h"
#include <string>
#include <ctime>

namespace ideawalker::ui {

/**
 * @brief Converts time_t to tm using platform-specific safe functions.
 */
std::tm ToLocalTime(std::time_t tt);

/**
 * @brief Helper for InputTextMultiline with std::string and auto-resize.
 */
bool InputTextMultilineString(const char* label, std::string* str, const ImVec2& size, ImGuiInputTextFlags flags = 0);

/**
 * @brief Renders a stylized task card.
 */
bool TaskCard(const char* id, const std::string& text, float width);

} // namespace ideawalker::ui
