/**
 * @file ProjectService.hpp
 * @brief Service to manage project lifecycle and filesystem structure.
 */

#pragma once

#include <string>
#include <filesystem>

namespace ideawalker::application {

class ProjectService {
public:
    /**
     * @brief Ensures the required folder structure exists at the given root.
     */
    bool EnsureProjectFolders(const std::filesystem::path& root);

    /**
     * @brief Copies all project data from one location to another.
     */
    bool CopyProjectData(const std::filesystem::path& fromRoot, const std::filesystem::path& toRoot);

    /**
     * @brief Validates if a path is a valid project directory.
     */
    bool IsValidProject(const std::string& rootPath);
};

} // namespace ideawalker::application
