#include "infrastructure/PathUtils.hpp"
#include <cstdlib>
#include <filesystem>

namespace ideawalker::infrastructure {

namespace fs = std::filesystem;

fs::path PathUtils::GetDataHome() {
    const char* xdgDataHome = std::getenv("XDG_DATA_HOME");
    if (xdgDataHome && *xdgDataHome) {
        return fs::path(xdgDataHome);
    }
    const char* home = std::getenv("HOME");
    if (home && *home) {
        return fs::path(home) / ".local" / "share";
    }
    return fs::current_path(); // Fallback
}

fs::path PathUtils::GetConfigHome() {
    const char* xdgConfigHome = std::getenv("XDG_CONFIG_HOME");
    if (xdgConfigHome && *xdgConfigHome) {
        return fs::path(xdgConfigHome);
    }
    const char* home = std::getenv("HOME");
    if (home && *home) {
        return fs::path(home) / ".config";
    }
    return fs::current_path();
}

fs::path PathUtils::GetCacheHome() {
    const char* xdgCacheHome = std::getenv("XDG_CACHE_HOME");
    if (xdgCacheHome && *xdgCacheHome) {
        return fs::path(xdgCacheHome);
    }
    const char* home = std::getenv("HOME");
    if (home && *home) {
        return fs::path(home) / ".cache";
    }
    return fs::current_path();
}

fs::path PathUtils::GetModelsDir() {
    fs::path base = GetDataHome() / "IdeaWalker" / "models";
    // Ensure directory exists
    try {
        if (!fs::exists(base)) {
            fs::create_directories(base);
        }
    } catch (...) {
        // Ignore errors, caller might handle or just fail to find models
    }
    return base;
}

fs::path PathUtils::GetProjectsDir() {
    fs::path base = GetDataHome() / "IdeaWalker" / "projects";
    try {
        if (!fs::exists(base)) {
            fs::create_directories(base);
        }
    } catch (...) {
        // Fallback or log error if logging system exists
    }
    return base;
}

fs::path PathUtils::GetEmbeddingsDir() {
    fs::path base = GetDataHome() / "IdeaWalker" / "embeddings";
    try {
        if (!fs::exists(base)) {
            fs::create_directories(base);
        }
    } catch (...) {
        // Fallback or log error
    }
    return base;
}

} // namespace ideawalker::infrastructure
