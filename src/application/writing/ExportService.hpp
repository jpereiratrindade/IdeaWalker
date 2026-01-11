/**
 * @file ExportService.hpp
 * @brief Service for exporting writing trajectories to various formats.
 */

#pragma once

#include <string>
#include "domain/writing/WritingTrajectory.hpp"

namespace ideawalker::application::writing {

using namespace ideawalker::domain::writing;

class ExportService { // Simplified for MVP, likely just a static helper or simple class
public:
    static std::string toMarkdown(const WritingTrajectory& trajectory);
    static std::string toLatex(const WritingTrajectory& trajectory);
};

} // namespace ideawalker::application::writing
