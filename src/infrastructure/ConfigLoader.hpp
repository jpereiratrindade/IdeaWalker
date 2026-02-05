/**
 * @file ConfigLoader.hpp
 * @brief Static utility for loading/saving application configuration (settings.json).
 * 
 * Provides a unified way to access configuration like video driver preference
 * without scattering JSON parsing logic throughout the codebase.
 */

#pragma once

#include <string>
#include <optional>

namespace ideawalker::infrastructure {

class ConfigLoader {
public:
    /**
     * @brief Reads the 'video_driver' key from settings.json.
     * @param projectRoot Absolute path to the project root.
     * @return std::optional<std::string> The driver name ("x11", "wayland") or nullopt.
     */
    static std::optional<std::string> GetVideoDriverPreference(const std::string& projectRoot);
    static void SaveVideoDriverPreference(const std::string& projectRoot, const std::string& driver);

    /**
     * @brief Reads the 'ai_model' key from settings.json.
     */
    static std::optional<std::string> GetAIModelPreference(const std::string& projectRoot);
    
    /**
     * @brief Saves the 'ai_model' key to settings.json.
     */
    static void SaveAIModelPreference(const std::string& projectRoot, const std::string& modelName);
};

} // namespace ideawalker::infrastructure
