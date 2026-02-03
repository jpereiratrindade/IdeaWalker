/**
 * @file GraphService.hpp
 * @brief Service to manage the node graph lifecycle and physics.
 */

#pragma once

#include <vector>
#include <unordered_set>
#include "domain/Insight.hpp"
#include "domain/writing/MermaidGraph.hpp"

namespace ideawalker::application {

class GraphService {
public:
    /**
     * @brief Rebuilds the graph nodes and links based on the provided insights.
     */
    void RebuildGraph(const std::vector<domain::Insight>& insights, 
                      bool showTasks,
                      std::vector<domain::writing::GraphNode>& nodes, 
                      std::vector<domain::writing::GraphLink>& links);

    /**
     * @brief Updates the physics simulation for the graph.
     */
    void UpdatePhysics(std::vector<domain::writing::GraphNode>& nodes, 
                       const std::vector<domain::writing::GraphLink>& links,
                       const std::unordered_set<int>& selectedNodes);

    /**
     * @brief Resets node positions to the center.
     */
    void CenterGraph(std::vector<domain::writing::GraphNode>& nodes);
};

} // namespace ideawalker::application
