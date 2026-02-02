/**
 * @file ConfigLoader.cpp
 * @brief Implementation of ConfigLoader.
 */

#include "infrastructure/ConfigLoader.hpp"
#include <filesystem>
#include <fstream>
#include <nlohmann/json.hpp>
#include <iostream>

namespace ideawalker::infrastructure {

std::optional<std::string> ConfigLoader::GetVideoDriverPreference(const std::string& projectRoot) {
    std::filesystem::path configPath = std::filesystem::path(projectRoot) / "settings.json";
    if (!std::filesystem::exists(configPath)) {
        return std::nullopt;
    }

    try {
        std::ifstream f(configPath);
        nlohmann::json j;
        f >> j;

        if (j.contains("video_driver")) {
            return j["video_driver"].get<std::string>();
        }
    } catch (const std::exception& e) {
        std::cerr << "[ConfigLoader] Error reading settings.json: " << e.what() << std::endl;
    }

    return std::nullopt;
}

void ConfigLoader::SaveVideoDriverPreference(const std::string& projectRoot, const std::string& driver) {
    std::filesystem::path configPath = std::filesystem::path(projectRoot) / "settings.json";
    nlohmann::json j;

    // Try to load existing to preserve other settings
    if (std::filesystem::exists(configPath)) {
        try {
            std::ifstream f(configPath);
            f >> j;
        } catch (...) {
            // If failed to read, we start fresh (or could backup, but for now simple overwrite of corrupted is acceptable)
        }
    }

    j["video_driver"] = driver;

    try {
        std::ofstream f(configPath);
        f << j.dump(4);
    } catch (const std::exception& e) {
        std::cerr << "[ConfigLoader] Error writing settings.json: " << e.what() << std::endl;
    }
}

} // namespace ideawalker::infrastructure
