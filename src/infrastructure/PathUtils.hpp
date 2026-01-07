// PathUtils Header
#pragma once
#include <string>
#include <filesystem>

namespace ideawalker::infrastructure {

class PathUtils {
public:
    static std::filesystem::path GetDataHome();
    static std::filesystem::path GetConfigHome();
    static std::filesystem::path GetCacheHome();
    static std::filesystem::path GetModelsDir();
};

} // namespace ideawalker::infrastructure
