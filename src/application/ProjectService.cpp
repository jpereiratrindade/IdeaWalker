#include "application/ProjectService.hpp"
#include <system_error>

namespace ideawalker::application {

bool ProjectService::EnsureProjectFolders(const std::filesystem::path& root) {
    try {
        std::filesystem::create_directories(root / "inbox");
        std::filesystem::create_directories(root / "notas");
        std::filesystem::create_directories(root / ".history");
        return true;
    } catch (...) {
        return false;
    }
}

bool ProjectService::CopyProjectData(const std::filesystem::path& fromRoot, const std::filesystem::path& toRoot) {
    try {
        std::filesystem::create_directories(toRoot / "inbox");
        std::filesystem::create_directories(toRoot / "notas");
        std::filesystem::create_directories(toRoot / ".history");
        
        std::filesystem::copy_options options = std::filesystem::copy_options::recursive | std::filesystem::copy_options::overwrite_existing;
        
        std::filesystem::copy(fromRoot / "inbox", toRoot / "inbox", options);
        std::filesystem::copy(fromRoot / "notas", toRoot / "notas", options);
        
        if (std::filesystem::exists(fromRoot / ".history")) {
             std::filesystem::copy(fromRoot / ".history", toRoot / ".history", options);
        }
        return true;
    } catch (...) {
        return false;
    }
}

bool ProjectService::IsValidProject(const std::string& rootPath) {
    if (rootPath.empty()) return false;
    std::filesystem::path root(rootPath);
    return std::filesystem::exists(root / "notas") || std::filesystem::exists(root / "inbox");
}

} // namespace ideawalker::application
