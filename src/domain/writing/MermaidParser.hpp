#pragma once

#include "domain/writing/MermaidGraph.hpp"
#include <string>
#include <functional>

namespace ideawalker::domain::writing {

/**
 * @brief Service to parse Mermaid syntax and generate graph layouts.
 * This service is stateless and independent of UI frameworks.
 */
class MermaidParser {
public:
    /**
     * @brief Result of a size calculation.
     */
    struct NodeSize {
        float width;
        float height;
        float wrapWidth;
    };

    /**
     * @brief Function type for calculating node sizes based on text content and wrapping.
     */
    using SizeCalculator = std::function<NodeSize(const std::string& text)>;

    /**
     * @brief Parses a Mermaid string and populates a PreviewGraphState.
     * @param content Raw Mermaid syntax.
     * @param graph State to populate.
     * @param calculator Callback to calculate node dimensions.
     * @param baseId Starting ID for nodes in this graph.
     * @return True if parsing was successful and content changed.
     */
    static bool Parse(const std::string& content, PreviewGraphState& graph, SizeCalculator calculator, int baseId = 10000);
};

} // namespace ideawalker::domain::writing
