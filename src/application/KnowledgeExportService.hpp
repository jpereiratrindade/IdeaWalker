/**
 * @file KnowledgeExportService.hpp
 * @brief Service to export the entire knowledge base (insights and graph).
 */

#pragma once

#include <string>
#include <vector>
#include "domain/Insight.hpp"
#include "domain/writing/MermaidGraph.hpp"

namespace ideawalker::application {

class KnowledgeExportService {
public:
    /**
     * @brief Exports the knowledge base to a Mermaid mindmap format.
     */
    static std::string ToMermaidMindmap(const std::vector<domain::writing::GraphNode>& nodes, 
                                        const std::vector<domain::writing::GraphLink>& links);

    /**
     * @brief Exports the entire knowledge base to a comprehensive Markdown document.
     */
    static std::string ToFullMarkdown(const std::vector<domain::Insight>& insights,
                                      const std::vector<domain::writing::GraphNode>& nodes, 
                                      const std::vector<domain::writing::GraphLink>& links);
};

} // namespace ideawalker::application
