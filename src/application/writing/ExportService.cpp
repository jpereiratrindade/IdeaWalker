/**
 * @file ExportService.cpp
 * @brief Implementation of ExportService.
 */

#include "ExportService.hpp"
#include <sstream>

namespace ideawalker::application::writing {

std::string ExportService::toMarkdown(const WritingTrajectory& trajectory) {
    std::stringstream ss;
    ss << "# " << trajectory.getIntent().purpose << "\n\n"; // Using purpose as provisional title
    ss << "**Audience:** " << trajectory.getIntent().audience << "\n";
    ss << "**Core Claim:** " << trajectory.getIntent().coreClaim << "\n\n";
    ss << "---\n\n";

    for (const auto& [id, segment] : trajectory.getSegments()) {
        ss << "## " << segment.title << "\n\n";
        ss << segment.content << "\n\n";
    }
    return ss.str();
}

std::string ExportService::toLatex(const WritingTrajectory& trajectory) {
    std::stringstream ss;
    ss << "\\documentclass{article}\n";
    ss << "\\title{" << trajectory.getIntent().purpose << "}\n";
    ss << "\\author{IdeaWalker User}\n";
    ss << "\\begin{document}\n";
    ss << "\\maketitle\n\n";
    
    ss << "\\begin{abstract}\n";
    ss << trajectory.getIntent().coreClaim << "\n";
    ss << "\\end{abstract}\n\n";

    for (const auto& [id, segment] : trajectory.getSegments()) {
        ss << "\\section{" << segment.title << "}\n";
        ss << segment.content << "\n\n";
    }

    ss << "\\end{document}\n";
    return ss.str();
}

} // namespace ideawalker::application::writing
