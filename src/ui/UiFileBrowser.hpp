#pragma once

#include <string>
#include <vector>
#include <filesystem>

namespace ideawalker::ui {

/**
 * @brief Resolves a browsing starting path from a buffer or fallback.
 */
std::filesystem::path ResolveBrowsePath(const char* buffer, const std::string& fallbackRoot);

/**
 * @brief Safely sets a path to a character buffer.
 */
void SetPathBuffer(char* buffer, size_t bufferSize, const std::filesystem::path& path);

/**
 * @brief Draws an interactive file browser.
 * @return True if a file was selected.
 */
bool DrawFileBrowser(const char* id, char* pathBuffer, size_t bufferSize, const std::string& fallbackRoot);

/**
 * @brief Draws an interactive folder browser.
 * @return True if a folder was selected.
 */
bool DrawFolderBrowser(const char* id, char* pathBuffer, size_t bufferSize, const std::string& fallbackRoot);

} // namespace ideawalker::ui
