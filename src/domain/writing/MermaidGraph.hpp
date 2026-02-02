#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <map>

namespace ideawalker::domain::writing {

    /**
     * @enum NodeType
     * @brief Types of nodes available in the interactive graph.
     */
    enum class NodeType {
        INSIGHT,    ///< A structured note or insight.
        TASK_TODO,  ///< A task that hasn't been started.
        TASK_DONE,  ///< A completed task.
        NOTE_LINK,  ///< A wiki-style link to another note.
        TASK,       ///< General task node.
        CONCEPT,    ///< A referenced concept that doesn't exist as a file yet.
        HYPOTHESIS  ///< A temporary anchor for integration hypotheses (Cyan).
    };

    /**
     * @enum NodeShape
     * @brief Mermaid-style shapes for graph rendering.
     */
    enum class NodeShape {
        BOX,            ///< [ ] Square box.
        ROUNDED_BOX,    ///< ( ) Rounded box.
        CIRCLE,         ///< (( )) Circle.
        STADIUM,        ///< ([ ]) Stadium shape.
        SUBROUTINE,     ///< [[ ]] Double-bordered box.
        CYLINDER,       ///< [( )] Cylinder/Database.
        HEXAGON,        ///< {{ }} Hexagon.
        RHOMBUS,        ///< { } Diamond.
        ASYMMETRIC,     ///< > ] Flag shape.
        BANG,           ///< )) (( Bang shape.
        CLOUD           ///< ) ( Cloud shape.
    };

    /**
     * @enum LayoutOrientation
     * @brief Orientation for tree-like diagrams.
     */
    enum class LayoutOrientation {
        LeftRight,
        TopDown
    };

    /**
     * @struct GraphNode
     * @brief Represents a visible node in the graph (Neural Web or Mermaid Preview).
     */
    struct GraphNode {
        int id; ///< Unique node ID.
        std::string title; ///< Text label.
        float x, y; ///< Normalized or screen coordinates.
        float w, h; ///< Exact dimensions (width, height) calculated for rendering.
        float wrapW = 200.0f; ///< The text wrap width used during size calculation.
        float vx, vy; ///< Velocity vectors for physics simulation.
        NodeType type; ///< Categorical type.
        bool isCompleted = false; ///< Task status (if applicable).
        bool isInProgress = false; ///< Task status (if applicable).
        NodeShape shape = NodeShape::ROUNDED_BOX; ///< Visual shape style.
    };

    /**
     * @struct GraphLink
     * @brief Represents a connection between two nodes.
     */
    struct GraphLink {
        int id; ///< Unique link ID.
        int startNode; ///< Source node ID.
        int endNode; ///< Destination node ID.
    };

    /**
     * @struct PreviewGraphState
     * @brief Contains the parsed state of a Mermaid diagram.
     */
    struct PreviewGraphState {
        std::vector<GraphNode> nodes;
        std::vector<GraphLink> links;
        std::unordered_map<int, int> nodeById; ///< Mapping of node IDs to their indices (O(1) lookup).
        std::vector<int> roots; ///< List of root node IDs.
        std::unordered_map<int, std::vector<int>> childrenNodes; ///< Adjacency list.
        std::string lastContent;
        bool initialized = false;
        LayoutOrientation orientation = LayoutOrientation::LeftRight;
        bool isForest = false; // Multiple roots detected
    };

} // namespace ideawalker::domain::writing
