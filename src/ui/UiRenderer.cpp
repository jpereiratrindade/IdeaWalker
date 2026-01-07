/**
 * @file UiRenderer.cpp
 * @brief Implementation of the UI rendering system using Dear ImGui and ImNodes.
 */
#include "ui/UiRenderer.hpp"

#include "application/OrganizerService.hpp"
#include "imgui.h"
#include "imnodes.h"

#include <algorithm>
#include <chrono>
#include <cfloat>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <filesystem>
#include <functional>
#include <sstream>
#include <string>
#include <thread>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace ideawalker::ui {

namespace {
    constexpr float NODE_MAX_WIDTH = 250.0f;

bool StartsWith(const std::string& text, const char* prefix) {
    return text.rfind(prefix, 0) == 0;
}

/**
 * @brief Heuristic to find a balanced node size (width vs height).
 * Iterates through wrap widths to avoid extremely tall "post" nodes.
 */
/**
 * @struct NodeSizeResult
 * @brief Simple container for adaptive node dimensions and wrap width.
 */
struct NodeSizeResult { float w, h, wrap; };

/**
 * @brief Heuristic to find a balanced node size (width vs height).
 * Iterates through wrap widths to avoid extremely tall "post" nodes.
 * @param text The node title.
 * @param minWrap Minimum allowed wrap width.
 * @param maxWrap Maximum allowed wrap width.
 * @param step Step size for iteration.
 * @param padX Horizontal padding.
 * @param padY Vertical padding.
 * @return Best fit dimensions and the corresponding wrap width.
 */
NodeSizeResult EstimateNodeSizeAdaptive(const std::string& text, float minWrap, float maxWrap, float step, float padX, float padY) {
    float bestWrap = minWrap;
    float bestCost = FLT_MAX;
    ImVec2 bestSize = ImVec2(0, 0);

    for (float wrap = minWrap; wrap <= maxWrap; wrap += step) {
        ImVec2 sz = ImGui::CalcTextSize(text.c_str(), nullptr, false, wrap);
        
        // Cost: Area + penalty for extremely tall nodes
        float area = (sz.x + padX) * (sz.y + padY);
        float tallPenalty = (sz.y > 150.0f) ? (sz.y - 150.0f) * 60.0f : 0.0f;
        float cost = area + tallPenalty;

        if (cost < bestCost) {
            bestCost = cost;
            bestWrap = wrap;
            bestSize = sz;
        }
    }
    return { bestSize.x + padX, bestSize.y + padY, bestWrap };
}

// Helper to draw a rich representation of Markdown content
/**
 * @brief Parses Mermaid syntax into a GraphNode/GraphLink structure.
 * Supports Mindmap and Flowchart/Graph syntax.
 * @param app The application state.
 * @param mermaidContent The raw Mermaid string.
 * @param graphId Unique ID for the diagram context.
 * @param updateImNodes Whether to push positions to ImNodes context.
 * @return True if successful.
 */
bool ParseMermaidToGraph(AppState& app, const std::string& mermaidContent, int graphId, bool updateImNodes) {
    auto& graph = app.previewGraphs[graphId];
    if (graph.lastContent == mermaidContent && graph.initialized) {
        return false; 
    }

    graph.nodes.clear();
    graph.links.clear();
    graph.lastContent = mermaidContent;
    graph.initialized = true;

    std::stringstream ss(mermaidContent);
    std::string line;
    // Offset IDs by graphId
    int baseId = 10000 + (graphId * 10000); 
    int nextId = baseId; 
    int linkId = baseId + 5000;
    
    std::unordered_map<std::string, int> idMap; 

    auto GetOrCreateNode = [&](const std::string& name, const std::string& label, NodeShape shape) {
        if (idMap.find(name) == idMap.end()) {
            GraphNode node;
            node.id = nextId++;
            node.title = label.empty() ? name : label;
            node.x = 0; node.y = 0; // Default
            node.vx = node.vy = 0;
            node.type = NodeType::INSIGHT; 
            node.shape = shape; // Set shape
            graph.nodes.push_back(node);
            idMap[name] = node.id;
        }
        return idMap[name];
    };

    std::vector<std::pair<int, int>> indentStack; // <indent_level, node_id>
    bool isMindMap = false;

    // Helper to clean ID/Label and detect Shape
    auto ParseNodeStr = [](const std::string& raw, std::string& idOut, std::string& labelOut, NodeShape& shapeOut) {
        // Default
        shapeOut = NodeShape::BOX; // Default for graph is usually box, mindmap usually rounded. We'll set defaults later if needed, but here we detect explicit syntax.
        
        size_t start = std::string::npos;
        size_t end = std::string::npos;
        
        // Priority: Check longer delimiters first to avoiding partial matches (e.g. check '((' before '(')
        
        // Circular ((...))
        if ((start = raw.find("((")) != std::string::npos) { shapeOut = NodeShape::CIRCLE; start += 2; end = raw.rfind("))"); }
        // Cloud )(...)  (Mermaid syntax is `id)Label(` ??? No, checking docs: `id)Label(` is Cloud? Actually docs say `id)Label(` is Cloud in *mindmap*? Wait. 
        // Flowchart: `id(Label)` = Rounded. `id([Label])` = Stadium. `id[[Label]]` = Subroutine. `id[(Label)]` = Cylinder. `id((Label))` = Circle. `id>Label]` = Asymmetric. `id{Label}` = Rhombus. `id{{Label}}` = Hexagon. 
        // Mindmap: `id[Label]`, `id(Label)`, `id((Label))`, `id))Label((` (Bang), `id)Label(` (Cloud), `id{{Label}}` (Hexagon).
        // Let's implement checking based on Start AND End chars to be safe.

        // Hexagon {{...}}
        else if ((start = raw.find("{{")) != std::string::npos) { shapeOut = NodeShape::HEXAGON; start += 2; end = raw.rfind("}}"); }
        // Subroutine [[...]]
        else if ((start = raw.find("[[")) != std::string::npos) { shapeOut = NodeShape::SUBROUTINE; start += 2; end = raw.rfind("]]"); }
        // Cylinder [(...)]
        else if ((start = raw.find("[(")) != std::string::npos) { shapeOut = NodeShape::CYLINDER; start += 2; end = raw.rfind(")]"); }
        // Stadium ([...])
        else if ((start = raw.find("([")) != std::string::npos) { shapeOut = NodeShape::STADIUM; start += 2; end = raw.rfind("])"); }
        // Bang ))...((
        else if ((start = raw.find("))")) != std::string::npos) { shapeOut = NodeShape::BANG; start += 2; end = raw.rfind("(("); }
        // Cloud )...( 
        // Note: ')' is common, need to be careful. ')' at start? Mindmap syntax `id)Label(`.
        // Let's look for `)` after ID.
        
        // Let's refine parsing. We usually have `ID<DELIM_START>Label<DELIM_END>`.
        // Or just `Label` (Mindmap default).
        
        // We really should find the first non-ID char. 
        // But the current `raw` might be "A[Label]" or just "MindmapNode"
        
        if (start == std::string::npos) {
            // Check single char delimiters
             if ((start = raw.find("[")) != std::string::npos) { 
                 // Check if it's really [ and not [[ or [( or ([
                 // We already checked [[, [(, ([ above? Yes, if we put them before. 
                 // Wait, `find` finds the *first* occurrence. 
                 // If we have `[` at pos 1. `[[` would also have `[` at pos 1.
                 // So we must check specific multi-char delimiters FIRST.
                 
                 // Re-doing checks carefully.
            }
        }
        
        // RESET and do it properly with finding first delimiter
        start = std::string::npos;
        end = std::string::npos; 
        
        // Find FIRST occurrence of any starter
        size_t firstBracket = raw.find_first_of("[({)>");
        // Also ')' for Cloud/Bang in mindmap?
        // Note: "Bang" is `))Label((`. Start is `))`.
        // "Cloud" is `)Label(`. Start is `)`.
        size_t firstParenClose = raw.find(")"); 
        
        size_t bestStart = std::string::npos;
        
        if (firstBracket != std::string::npos) bestStart = firstBracket;
        if (firstParenClose != std::string::npos && (bestStart == std::string::npos || firstParenClose < bestStart)) bestStart = firstParenClose;

        if (bestStart != std::string::npos) {
            // Check what it is
            // Bang: ))
            if (raw.substr(bestStart, 2) == "))") {
                 shapeOut = NodeShape::BANG; start = bestStart + 2; end = raw.rfind("((");
            }
            // Cloud: )
            else if (raw.substr(bestStart, 1) == ")") {
                 shapeOut = NodeShape::CLOUD; start = bestStart + 1; end = raw.rfind("(");
            }
            // Hexagon: {{
            else if (raw.substr(bestStart, 2) == "{{") {
                 shapeOut = NodeShape::HEXAGON; start = bestStart + 2; end = raw.rfind("}}");
            }
            // Circle: ((
            else if (raw.substr(bestStart, 2) == "((") {
                 shapeOut = NodeShape::CIRCLE; start = bestStart + 2; end = raw.rfind("))");
            }
            // Subroutine: [[
            else if (raw.substr(bestStart, 2) == "[[") {
                 shapeOut = NodeShape::SUBROUTINE; start = bestStart + 2; end = raw.rfind("]]");
            }
            // Cylinder: [(
            else if (raw.substr(bestStart, 2) == "[(") {
                 shapeOut = NodeShape::CYLINDER; start = bestStart + 2; end = raw.rfind(")]");
            }
            // Stadium: ([
            else if (raw.substr(bestStart, 2) == "([") {
                 shapeOut = NodeShape::STADIUM; start = bestStart + 2; end = raw.rfind("])");
            }
            // Parallelogram Alt: [\ ... /] ?? (Not in plan but standard). skipping for now to stick to plan + requested.
            
            // Single char checks
            else if (raw.substr(bestStart, 1) == "[") {
                 shapeOut = NodeShape::BOX; start = bestStart + 1; end = raw.rfind("]");
            }
            else if (raw.substr(bestStart, 1) == "(") {
                 shapeOut = NodeShape::ROUNDED_BOX; start = bestStart + 1; end = raw.rfind(")");
            }
            else if (raw.substr(bestStart, 1) == "{") {
                 shapeOut = NodeShape::RHOMBUS; start = bestStart + 1; end = raw.rfind("}");
            }
            else if (raw.substr(bestStart, 1) == ">") {
                 shapeOut = NodeShape::ASYMMETRIC; start = bestStart + 1; end = raw.rfind("]");
            }
        }

        if (start != std::string::npos && end != std::string::npos && end > start) {
            // Extract ID and Label
            // ID is everything before bestStart
            idOut = raw.substr(0, bestStart);
            labelOut = raw.substr(start, end - start);
        } else {
            // No shape syntax, just text
            idOut = raw;
            labelOut = raw;
            // Default shape if none specified?
            // For Mindmap, default is usually Rounded or just Text. Current code used Rounded for everything.
            shapeOut = NodeShape::ROUNDED_BOX; 
        }
        
        // Trim
        auto Trim = [](std::string& s) {
            if (s.empty()) return;
            s.erase(0, s.find_first_not_of(" \t"));
            if (!s.empty()) s.erase(s.find_last_not_of(" \t") + 1);
        };
        Trim(idOut);
        Trim(labelOut);
    };

    while (std::getline(ss, line)) {
        std::string trimmed = line;
        size_t firstChar = trimmed.find_first_not_of(" \t");
        if (firstChar == std::string::npos) continue; // Empty line
        
        int indent = static_cast<int>(firstChar);
        trimmed = trimmed.substr(firstChar);

        if (StartsWith(trimmed, "mindmap")) {
            isMindMap = true;
            continue;
        }
        if (StartsWith(trimmed, "graph ") || StartsWith(trimmed, "flowchart ")) continue;
        if (StartsWith(trimmed, "%%")) continue; // Comments

        if (isMindMap) {
            // Mindmap Indentation Logic
            std::string id, label;
            NodeShape shape;
            ParseNodeStr(trimmed, id, label, shape);
            
            // If ID is empty (e.g. valid syntax but parser failed), use label
            if (id.empty()) id = label;

            int nodeId = GetOrCreateNode(id, label, shape);

            // Find parent
            while (!indentStack.empty() && indentStack.back().first >= indent) {
                indentStack.pop_back();
            }

            if (!indentStack.empty()) {
                // Link to parent
                GraphLink link;
                link.id = linkId++;
                link.startNode = indentStack.back().second;
                link.endNode = nodeId;
                graph.links.push_back(link);
            }
            
            indentStack.push_back({indent, nodeId});

        } else {
            // Graph / Flowchart Logic
            size_t arrowPos = line.find("-->");
            if (arrowPos != std::string::npos) {
                std::string left = line.substr(0, arrowPos);
                std::string right = line.substr(arrowPos + 3);

                std::string idA, labelA, idB, labelB;
                NodeShape shapeA, shapeB;
                ParseNodeStr(left, idA, labelA, shapeA);
                ParseNodeStr(right, idB, labelB, shapeB);

                if (!idA.empty() && !idB.empty()) {
                    GraphLink link;
                    link.id = linkId++;
                    link.startNode = GetOrCreateNode(idA, labelA, shapeA);
                    link.endNode = GetOrCreateNode(idB, labelB, shapeB);
                    graph.links.push_back(link);
                }
            } else {
                 // Single Node Definition: A[Label]
                 // Or just A
                 std::string id, label;
                 NodeShape shape;
                 ParseNodeStr(trimmed, id, label, shape);
                 if (!id.empty()) {
                     GetOrCreateNode(id, label, shape);
                }
            }
        }
    }
    
    // Finalize node lookup map
    graph.nodeById.clear();
    for (size_t i = 0; i < graph.nodes.size(); ++i) {
        graph.nodeById[graph.nodes[i].id] = static_cast<int>(i);
    }

    // Wrap Text & Calculate Sizes
    // Wrap Text & Calculate Sizes

    for (auto& node : graph.nodes) {
        NodeSizeResult res = EstimateNodeSizeAdaptive(node.title, 160.0f, 420.0f, 40.0f, 30.0f, 20.0f);
        node.w = res.w;
        node.h = res.h;
        node.wrapW = res.wrap;
    }

    // Populate Structural Cache
    graph.childrenNodes.clear();
    graph.roots.clear();
    std::unordered_map<int, int> inDegree;
    for (const auto& node : graph.nodes) inDegree[node.id] = 0;
    for (const auto& link : graph.links) {
        graph.childrenNodes[link.startNode].push_back(link.endNode);
        inDegree[link.endNode]++;
    }
    for (const auto& node : graph.nodes) {
        if (inDegree[node.id] == 0) {
            graph.roots.push_back(node.id);
        }
    }
    if (graph.roots.empty() && !graph.nodes.empty()) graph.roots.push_back(graph.nodes[0].id);

    // 1. Decide orientation via heuristic (Depth vs Breadth)
    std::unordered_map<int, int> nodeDepth;
    std::unordered_map<int, int> countPerDepth;
    int maxDepth = 0;
    int maxBreadth = 0;

    std::function<void(int, int, std::unordered_set<int>&)> WalkTopology = 
        [&](int u, int d, std::unordered_set<int>& visited) {
        visited.insert(u);
        nodeDepth[u] = d;
        countPerDepth[d]++;
        maxDepth = std::max(maxDepth, d);
        maxBreadth = std::max(maxBreadth, countPerDepth[d]);
        for (int v : graph.childrenNodes[u]) {
            if (visited.find(v) == visited.end()) {
                WalkTopology(v, d + 1, visited);
            }
        }
    };

    std::unordered_set<int> visitedTopology;
    for (int r : graph.roots) WalkTopology(r, 0, visitedTopology);

    graph.orientation = (maxDepth > maxBreadth) ? LayoutOrientation::TopDown : LayoutOrientation::LeftRight;

    // Subtree Breadth Helper (Dimension perpendicular to growth)
    std::unordered_map<int, float> subtreeBreadth;
    const float SIBLING_GAP = 60.0f; 

    std::function<float(int, std::unordered_set<int>&)> CalcBreadth = 
        [&](int u, std::unordered_set<int>& visited) -> float {
        visited.insert(u);
        float childrenB = 0;
        int childCount = 0;
        
        float myB = 0;
        {
            auto it = graph.nodeById.find(u);
            if (it != graph.nodeById.end()) {
                // In TB, breadth is W. In LR, breadth is H.
                myB = (graph.orientation == LayoutOrientation::TopDown) ? graph.nodes[it->second].w : graph.nodes[it->second].h;
            }
        }

        for (int v : graph.childrenNodes[u]) {
            if (visited.find(v) == visited.end()) {
                if (childCount > 0) childrenB += SIBLING_GAP;
                childrenB += CalcBreadth(v, visited);
                childCount++;
            }
        }
        
        subtreeBreadth[u] = std::max(myB, childrenB);
        return subtreeBreadth[u];
    };

    std::unordered_set<int> visitedBreadth;
    for(int root : graph.roots) CalcBreadth(root, visitedBreadth);

    std::function<void(int, float, float, std::unordered_set<int>&)> LayoutRecursive = 
        [&](int u, float primary, float secondaryStart, std::unordered_set<int>& visited) {
        visited.insert(u);
        
        GraphNode* uNode = nullptr;
        {
            auto it = graph.nodeById.find(u);
            if (it != graph.nodeById.end()) uNode = &graph.nodes[it->second];
        }
        if(!uNode) return;

        float totalB = subtreeBreadth[u];

        if (graph.orientation == LayoutOrientation::LeftRight) {
            uNode->x = primary;
            uNode->y = (secondaryStart + totalB * 0.5f) - (uNode->h * 0.5f);

            float hGap = std::clamp(uNode->w * 0.6f, 80.0f, 200.0f);
            float childX = primary + uNode->w + hGap;

            float childrenTotalB = 0;
            int childCount = 0;
            for(int v : graph.childrenNodes[u]) {
                if(visited.find(v) == visited.end()) {
                    if(childCount > 0) childrenTotalB += SIBLING_GAP;
                    childrenTotalB += subtreeBreadth[v];
                    childCount++;
                }
            }
            float childY = secondaryStart + (totalB - childrenTotalB) * 0.5f;
            for (int v : graph.childrenNodes[u]) {
                if (visited.find(v) == visited.end()) {
                    LayoutRecursive(v, childX, childY, visited);
                    childY += subtreeBreadth[v] + SIBLING_GAP;
                }
            }
        } else { // TopDown
            uNode->y = primary;
            uNode->x = (secondaryStart + totalB * 0.5f) - (uNode->w * 0.5f);

            float vGap = std::clamp(uNode->h * 0.6f, 60.0f, 160.0f);
            float childY = primary + uNode->h + vGap;

            float childrenTotalB = 0;
            int childCount = 0;
            for(int v : graph.childrenNodes[u]) {
                if(visited.find(v) == visited.end()) {
                    if(childCount > 0) childrenTotalB += SIBLING_GAP;
                    childrenTotalB += subtreeBreadth[v];
                    childCount++;
                }
            }
            float childX = secondaryStart + (totalB - childrenTotalB) * 0.5f;
            for (int v : graph.childrenNodes[u]) {
                if (visited.find(v) == visited.end()) {
                    LayoutRecursive(v, childY, childX, visited);
                    childX += subtreeBreadth[v] + SIBLING_GAP;
                }
            }
        }
    };

    std::unordered_set<int> visitedLayout;
    
    // Forest Layout: Center all roots globally
    float FOREST_GAP = 120.0f;
    float totalForestBreadth = 0;
    for (size_t i = 0; i < graph.roots.size(); ++i) {
        if (i > 0) totalForestBreadth += FOREST_GAP;
        totalForestBreadth += subtreeBreadth[graph.roots[i]];
    }

    float secondaryCursor = 50.0f; // Start with padding
    for (int root : graph.roots) {
         LayoutRecursive(root, 50.0f, secondaryCursor, visitedLayout);
         secondaryCursor += subtreeBreadth[root] + FOREST_GAP; 
    }

    if (updateImNodes) {
        ImNodes::EditorContextSet((ImNodesEditorContext*)app.previewGraphContext);
        // Apply positions to ImNodes (One-time layout)
        for (const auto& node : graph.nodes) {
            ImNodes::SetNodeGridSpacePos(node.id, ImVec2(node.x, node.y));
        }
    }
    return true;
}
/**
 * @brief Draws a static, pixel-perfect preview of a Mermaid graph.
 * Implements adaptive scaling (fit-to-view) and forest centering.
 * @param graph The cached graph state to render.
 */
void DrawStaticMermaidPreview(const AppState::PreviewGraphState& graph) {
    if (graph.nodes.empty()) {
        ImGui::TextDisabled("No diagram to display.");
        return;
    }

    ImDrawList* drawList = ImGui::GetWindowDrawList();
    ImVec2 origin = ImGui::GetCursorScreenPos();
    ImVec2 avail = ImGui::GetContentRegionAvail();
    if (avail.x < 1.0f || avail.y < 1.0f) return;

    // Bounds Calculation with sizes
    float minX = FLT_MAX, minY = FLT_MAX, maxX = -FLT_MAX, maxY = -FLT_MAX;
    
    // We need to re-estimate sizes for drawing bounds properly?
    // Nodes have x,y center positions. 
    // We can just rely on point clouds for bounds, or add the text size.
    // Let's do simple point bounds + padding.
    
    for (const auto& node : graph.nodes) {
        float halfW = node.w * 0.5f;
        float halfH = node.h * 0.5f;

        if (node.x - halfW < minX) minX = node.x - halfW;
        if (node.y - halfH < minY) minY = node.y - halfH;
        if (node.x + halfW > maxX) maxX = node.x + halfW;
        if (node.y + halfH > maxY) maxY = node.y + halfH;
    }
    // Add extra margin
    minX -= 20.0f; minY -= 20.0f;
    maxX += 20.0f; maxY += 20.0f;

    float graphW = maxX - minX;
    float graphH = maxY - minY;
    float centerX = minX + graphW * 0.5f;
    float centerY = minY + graphH * 0.5f;

    if (graphW < 1.0f) graphW = 1.0f;
    if (graphH < 1.0f) graphH = 1.0f;

    float padding = 40.0f;
    const float scale = 1.0f; // Force 1:1 for legibility and consistency

    // Define the Content Area for the parent scrollable child
    ImVec2 canvasSize(std::floor(graphW * scale + padding * 2), std::floor(graphH * scale + padding * 2));
    ImVec2 startPos = ImGui::GetCursorScreenPos();
    
    // Aligment: Center if smaller than available area, otherwise align top-left for scroll
    float offX = std::floor(std::max(0.0f, (avail.x - canvasSize.x) * 0.5f));
    float offY = std::floor(std::max(0.0f, (avail.y - canvasSize.y) * 0.5f));
    
    ImVec2 offset(std::floor(startPos.x + padding + offX - minX),
                  std::floor(startPos.y + padding + offY - minY));
                       
    // Use Dummy to reserve space for scrolling in the parent window
    ImGui::Dummy(canvasSize);

    // Draw Links (Bezier for Tree look)
    ImU32 linkColor = ImGui::GetColorU32(ImVec4(0.45f, 0.65f, 0.95f, 0.65f));
    for (const auto& link : graph.links) {
        const GraphNode* start = nullptr;
        const GraphNode* end = nullptr;
        
        auto itA = graph.nodeById.find(link.startNode);
        auto itB = graph.nodeById.find(link.endNode);
        
        if (itA != graph.nodeById.end()) start = &graph.nodes[itA->second];
        if (itB != graph.nodeById.end()) end = &graph.nodes[itB->second];
        if (!start || !end) continue;
        
        ImVec2 p1(offset.x + start->x * scale, offset.y + start->y * scale);
        ImVec2 p2(offset.x + end->x * scale, offset.y + end->y * scale);
        
        // Adaptive curved links
        ImVec2 cp1, cp2;
        if (graph.orientation == LayoutOrientation::LeftRight) {
            float cpDist = (p2.x - p1.x) * 0.5f;
            cp1 = ImVec2(p1.x + cpDist, p1.y);
            cp2 = ImVec2(p2.x - cpDist, p2.y);
        } else { // TopDown
            float cpDist = (p2.y - p1.y) * 0.5f;
            cp1 = ImVec2(p1.x, p1.y + cpDist);
            cp2 = ImVec2(p2.x, p2.y - cpDist);
        }
        
        drawList->AddBezierCubic(p1, cp1, cp2, p2, linkColor, 2.0f);
    }

    // Draw Nodes
    ImU32 textColor = ImGui::GetColorU32(ImGuiCol_Text);

    for (const auto& node : graph.nodes) {
        ImVec2 center(offset.x + node.x * scale, offset.y + node.y * scale);
        float w = node.w * scale;
        float h = node.h * scale;
        
        ImVec2 min(center.x - w * 0.5f, center.y - h * 0.5f);
        ImVec2 max(center.x + w * 0.5f, center.y + h * 0.5f);

        // Color Logic
        float dx = node.x - centerX;
        float dy = node.y - centerY;
        float angle = std::atan2(dy, dx); 
        float hue = (angle + 3.14159f) / (2.0f * 3.14159f);
        float r, g, b;
        ImGui::ColorConvertHSVtoRGB(hue, 0.6f, 0.5f, r, g, b); // Darker bg
        ImU32 nodeBg = ImGui::GetColorU32(ImVec4(r, g, b, 1.0f));
        ImGui::ColorConvertHSVtoRGB(hue, 0.6f, 0.8f, r, g, b); // Brighter border
        ImU32 nodeBorder = ImGui::GetColorU32(ImVec4(r, g, b, 1.0f));

        switch (node.shape) {
            case NodeShape::ROUNDED_BOX:
                drawList->AddRectFilled(min, max, nodeBg, 8.0f);
                drawList->AddRect(min, max, nodeBorder, 8.0f);
                break;
            case NodeShape::BOX:
                drawList->AddRectFilled(min, max, nodeBg, 0.0f);
                drawList->AddRect(min, max, nodeBorder, 0.0f);
                break;
            case NodeShape::CIRCLE:
                {
                    float radius = std::max((max.x - min.x), (max.y - min.y)) * 0.5f;
                    drawList->AddCircleFilled(center, radius, nodeBg);
                    drawList->AddCircle(center, radius, nodeBorder);
                }
                break;
            case NodeShape::STADIUM:
                {
                    float radius = (max.y - min.y) * 0.5f;
                    drawList->AddRectFilled(min, max, nodeBg, radius);
                    drawList->AddRect(min, max, nodeBorder, radius);
                }
                break;
            case NodeShape::SUBROUTINE:
                {
                    drawList->AddRectFilled(min, max, nodeBg, 0.0f);
                    drawList->AddRect(min, max, nodeBorder, 0.0f);
                    // Add vertical lines
                    float indent = 10.0f;
                    drawList->AddLine(ImVec2(min.x + indent, min.y), ImVec2(min.x + indent, max.y), nodeBorder);
                    drawList->AddLine(ImVec2(max.x - indent, min.y), ImVec2(max.x - indent, max.y), nodeBorder);
                }
                break;
            case NodeShape::CYLINDER:
                {
                    // Approximate cylinder
                    float rx = (max.x - min.x) * 0.5f;
                    float ry = 5.0f; // Ellipse height
                    ImVec2 topCenter(center.x, min.y + ry);
                    ImVec2 bottomCenter(center.x, max.y - ry);
                    
                    // Body
                    drawList->AddRectFilled(ImVec2(min.x, min.y + ry), ImVec2(max.x, max.y - ry), nodeBg);
                    
                    // Top Ellipse
                    drawList->AddEllipseFilled(topCenter, ImVec2(rx, ry), nodeBg);
                    drawList->AddEllipse(topCenter, ImVec2(rx, ry), nodeBorder);
                    
                    // Bottom Ellipse (Half only really needed for outline, but full filled is fine)
                    drawList->AddEllipseFilled(bottomCenter, ImVec2(rx, ry), nodeBg);
                    drawList->AddEllipse(bottomCenter, ImVec2(rx, ry), nodeBorder);
                    
                    // Sides
                    drawList->AddLine(ImVec2(min.x, min.y + ry), ImVec2(min.x, max.y - ry), nodeBorder);
                    drawList->AddLine(ImVec2(max.x, min.y + ry), ImVec2(max.x, max.y - ry), nodeBorder);
                }
                break;
            case NodeShape::HEXAGON:
                {
                    ImVec2 p[6];
                    float indent = 10.0f;
                    p[0] = ImVec2(min.x + indent, min.y);
                    p[1] = ImVec2(max.x - indent, min.y);
                    p[2] = ImVec2(max.x + indent, center.y); // Point out
                    p[3] = ImVec2(max.x - indent, max.y);
                    p[4] = ImVec2(min.x + indent, max.y);
                    p[5] = ImVec2(min.x - indent, center.y); // Point out
                    
                    drawList->AddConvexPolyFilled(p, 6, nodeBg);
                    drawList->AddPolyline(p, 6, nodeBorder, ImDrawFlags_Closed, 1.0f);
                }
                break;
            case NodeShape::RHOMBUS:
                {
                    ImVec2 p[4];
                    p[0] = ImVec2(center.x, min.y - 5.0f); // Top
                    p[1] = ImVec2(max.x + 10.0f, center.y); // Right
                    p[2] = ImVec2(center.x, max.y + 5.0f); // Bottom
                    p[3] = ImVec2(min.x - 10.0f, center.y); // Left
                    
                    drawList->AddConvexPolyFilled(p, 4, nodeBg);
                    drawList->AddPolyline(p, 4, nodeBorder, ImDrawFlags_Closed, 1.0f);
                }
                break;
             case NodeShape::ASYMMETRIC:
                {
                    ImVec2 p[5];
                    float indent = 15.0f;
                    p[0] = min; 
                    p[1] = ImVec2(max.x - indent, min.y);
                    p[2] = ImVec2(max.x, center.y); // Point
                    p[3] = ImVec2(max.x - indent, max.y);
                    p[4] = ImVec2(min.x, max.y);
                    
                    drawList->AddConvexPolyFilled(p, 5, nodeBg);
                    drawList->AddPolyline(p, 5, nodeBorder, ImDrawFlags_Closed, 1.0f);
                }
                break;
            // Fallback / Approximations
            case NodeShape::BANG:
            case NodeShape::CLOUD:
            default:
                 drawList->AddRectFilled(min, max, nodeBg, 8.0f); // Default rounded
                 drawList->AddRect(min, max, nodeBorder, 8.0f);
                 break;
        }
        
        ImVec2 textSize = ImGui::CalcTextSize(node.title.c_str(), nullptr, false, node.wrapW);
        ImVec2 textPos(std::floor(center.x - textSize.x * 0.5f), std::floor(center.y - textSize.y * 0.5f));
        
        drawList->AddText(
            ImGui::GetFont(),                 
            ImGui::GetFontSize(),             
            textPos,
            textColor,
            node.title.c_str(),
            nullptr,
            node.wrapW
        );
    }
}

void DrawMarkdownPreview(AppState& app, const std::string& content, bool staticMermaidPreview) {
    auto label = [&app](const char* withEmoji, const char* plain) {
        return app.emojiEnabled ? withEmoji : plain;
    };

    std::stringstream ss(content);
    std::string line;
    
    bool inCodeBlock = false;
    std::string codeBlockLanguage = "";
    std::string codeBlockContent = "";
    int codeBlockIdCounter = 0;

    // Reset preview graph if content is completely new/empty
    // Actually ParseMermaidToGraph handles caching per-block content 
    // BUT we need to be careful if there are MULTIPLE mermaid blocks.
    // Simplifying assumption: The user usually views one mermaid diagram at a time or the last one persists.
    // Ideally we would support multiple, but one shared previewGraph state means they would merge.
    // Let's clear it at start of frame? No, physics needs persistence.
    // Solution: Only render the FIRST mermaid block interactively, or render all (merged).
    
    while (std::getline(ss, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();

        if (StartsWith(line, "```")) {
            if (inCodeBlock) {
                // End block
                int blockId = codeBlockIdCounter++;
                ImGui::PushID(blockId);
                if (codeBlockLanguage == "mermaid") {
                    
                    // Parse (Calculates Layout Once)
                    bool newLayout = ParseMermaidToGraph(app, codeBlockContent, blockId, !staticMermaidPreview); 
                    auto& graph = app.previewGraphs[blockId];

                    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.12f, 0.14f, 0.18f, 1.0f));

                    if (staticMermaidPreview) {
                        ImGui::BeginChild("##mermaid_graph", ImVec2(0, 700), true);
                        DrawStaticMermaidPreview(graph);
                        ImGui::EndChild();
                        ImGui::PopStyleColor();
                    } else {
                        if (ImGui::Button("Fit to Screen") || newLayout) {
                            ImNodes::EditorContextSet((ImNodesEditorContext*)app.previewGraphContext);
                            
                            // Calculate Bounds
                            ImVec2 min(FLT_MAX, FLT_MAX);
                            ImVec2 max(-FLT_MAX, -FLT_MAX);
                            if (graph.nodes.empty()) {
                                min = max = ImVec2(0,0);
                            } else {
                                for(auto& n : graph.nodes) {
                                    if(n.x < min.x) min.x = n.x;
                                    if(n.y < min.y) min.y = n.y;
                                    if(n.x > max.x) max.x = n.x;
                                    if(n.y > max.y) max.y = n.y;
                                }
                            }
                            ImVec2 center = ImVec2((min.x + max.x)*0.5f, (min.y + max.y)*0.5f);
                            ImVec2 canvasSize(ImGui::GetContentRegionAvail().x, 400.0f);
                            ImVec2 pan = ImVec2(canvasSize.x * 0.5f - center.x, canvasSize.y * 0.5f - center.y);

                            ImNodes::EditorContextResetPanning(pan);
                        }
                        
                        ImGui::BeginChild("##mermaid_graph", ImVec2(0, 700), true);
                        
                        // Switch Context & Interactive Editor
                        ImNodes::EditorContextSet((ImNodesEditorContext*)app.previewGraphContext);
                        
                        // Styling
                        ImNodes::PushColorStyle(ImNodesCol_GridBackground, ImGui::GetColorU32(ImVec4(0.12f, 0.14f, 0.18f, 1.0f)));
                        ImNodes::PushColorStyle(ImNodesCol_GridLine, ImGui::GetColorU32(ImVec4(0.2f, 0.2f, 0.2f, 0.5f)));
                        
                        ImNodes::BeginNodeEditor();

                        // Draw Nodes
                        for (auto& node : graph.nodes) {
                            // Use Hash for consistent color
                            std::hash<std::string> hasher;
                            size_t h = hasher(node.title);
                            float hue = (h % 100) / 100.0f;
                            
                            ImVec4 bgCol, titleCol, titleSelCol;
                            ImGui::ColorConvertHSVtoRGB(hue, 0.6f, 0.3f, bgCol.x, bgCol.y, bgCol.z); bgCol.w = 1.0f;
                            ImGui::ColorConvertHSVtoRGB(hue, 0.6f, 0.5f, titleCol.x, titleCol.y, titleCol.z); titleCol.w = 1.0f;
                            ImGui::ColorConvertHSVtoRGB(hue, 0.6f, 0.6f, titleSelCol.x, titleSelCol.y, titleSelCol.z); titleSelCol.w = 1.0f;

                            ImNodes::PushColorStyle(ImNodesCol_NodeBackground, ImGui::GetColorU32(bgCol));
                            ImNodes::PushColorStyle(ImNodesCol_TitleBar, ImGui::GetColorU32(titleCol));
                            ImNodes::PushColorStyle(ImNodesCol_TitleBarSelected, ImGui::GetColorU32(titleSelCol));

                            // No forced SetNodeGridSpacePos here - handled in Parse
                            ImNodes::BeginNode(node.id);
                            
                            // Fake Title Bar
                            ImNodes::BeginNodeTitleBar();
                            ImGui::PushTextWrapPos(ImGui::GetCursorPos().x + NODE_MAX_WIDTH);
                            ImGui::TextUnformatted(node.title.c_str());
                            ImGui::PopTextWrapPos();
                            ImNodes::EndNodeTitleBar();

                            // Attributes to allow links
                            // We use dummy attributes hidden relative to node
                            ImNodes::BeginOutputAttribute(node.id << 8);
                            ImNodes::EndOutputAttribute();
                            
                            ImNodes::BeginInputAttribute((node.id << 8) + 1);
                            ImNodes::EndInputAttribute();

                            ImNodes::EndNode();
                            
                            ImNodes::PopColorStyle();
                            ImNodes::PopColorStyle();
                            ImNodes::PopColorStyle();
                        }
                        
                        // Draw Links
                        for (const auto& link : graph.links) {
                            ImNodes::Link(link.id, link.startNode << 8, (link.endNode << 8) + 1);
                        }

                        ImNodes::EndNodeEditor();
                        ImNodes::PopColorStyle();
                        ImNodes::PopColorStyle();

                        ImGui::EndChild();
                        ImGui::PopStyleColor();
                    }
                } else {
                    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.1f, 0.1f, 0.1f, 1.0f));
                    ImGui::BeginChild("##code", ImVec2(0, 0), true, ImGuiWindowFlags_AlwaysAutoResize);
                    if (!codeBlockLanguage.empty()) {
                         ImGui::TextDisabled("%s", codeBlockLanguage.c_str());
                         ImGui::Separator();
                    }
                    ImGui::PushFont(ImGui::GetIO().Fonts->Fonts[0]); 
                    ImGui::TextUnformatted(codeBlockContent.c_str());
                    ImGui::PopFont();
                    ImGui::EndChild();
                    ImGui::PopStyleColor();
                }
                ImGui::PopID();
                inCodeBlock = false;
                codeBlockContent = "";
            } else {
                inCodeBlock = true;
                if (line.length() > 3) {
                    codeBlockLanguage = line.substr(3);
                    auto start = codeBlockLanguage.find_first_not_of(" \t");
                    if (start != std::string::npos) codeBlockLanguage = codeBlockLanguage.substr(start);
                    auto end = codeBlockLanguage.find_last_not_of(" \t");
                    if (end != std::string::npos) codeBlockLanguage = codeBlockLanguage.substr(0, end + 1);
                } else {
                    codeBlockLanguage = "";
                }
            }
            continue;
        }

        if (inCodeBlock) {
            codeBlockContent += line + "\n";
            continue;
        }

        // Detect indentation and trimmed content
        size_t firstChar = line.find_first_not_of(" \t");
        if (firstChar == std::string::npos) {
            ImGui::Spacing();
            continue;
        }
        std::string indent = line.substr(0, firstChar);
        std::string trimmed = line.substr(firstChar);
        
        // Standard Markdown Rendering on trimmed content
        if (StartsWith(trimmed, "# ")) {
            ImGui::TextColored(ImVec4(0.4f, 0.7f, 1.0f, 1.0f), "%s", trimmed.substr(2).c_str());
            ImGui::Separator();
        } else if (StartsWith(trimmed, "## ")) {
            ImGui::TextColored(ImVec4(0.3f, 0.6f, 0.9f, 1.0f), "%s", trimmed.substr(3).c_str());
        } else if (StartsWith(trimmed, "### ")) {
            ImGui::TextColored(ImVec4(0.2f, 0.5f, 0.8f, 1.0f), "%s", trimmed.substr(4).c_str());
        } else if (StartsWith(trimmed, "- [ ] ") || StartsWith(trimmed, "* [ ] ")) {
            ImGui::TextUnformatted(indent.c_str()); ImGui::SameLine(0, 0);
            ImGui::TextUnformatted(label("📋", "[ ]")); ImGui::SameLine();
            ImGui::TextWrapped("%s", trimmed.substr(6).c_str());
        } else if (StartsWith(trimmed, "- [x] ") || StartsWith(trimmed, "* [x] ")) {
            ImGui::TextUnformatted(indent.c_str()); ImGui::SameLine(0, 0);
            ImGui::TextColored(ImVec4(0.5f, 1.0f, 0.5f, 1.0f), "%s", label("✅", "[x]")); ImGui::SameLine();
            ImGui::TextDisabled("%s", trimmed.substr(6).c_str());
        } else if (StartsWith(trimmed, "- ") || StartsWith(trimmed, "* ") || StartsWith(trimmed, "• ") || StartsWith(trimmed, "– ") || StartsWith(trimmed, "— ")) {
            ImGui::TextUnformatted(indent.c_str()); ImGui::SameLine(0, 0);
            ImGui::TextUnformatted("- "); ImGui::SameLine(0, 0);
            
            size_t skip = 2;
            if (StartsWith(trimmed, "• ")) skip = std::strlen("• ");
            else if (StartsWith(trimmed, "– ")) skip = std::strlen("– ");
            else if (StartsWith(trimmed, "— ")) skip = std::strlen("— ");
            ImGui::TextWrapped("%s", trimmed.substr(skip).c_str());
        } else if (StartsWith(trimmed, "> ")) {
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.7f, 0.7f, 0.7f, 1.0f));
            ImGui::TextUnformatted(indent.c_str()); ImGui::SameLine(0, 0);
            ImGui::TextWrapped(" | %s", trimmed.substr(2).c_str());
            ImGui::PopStyleColor();
        } else {
            // Indent the plain text if necessary
            if (!indent.empty()) { ImGui::TextUnformatted(indent.c_str()); ImGui::SameLine(0, 0); }
            
            size_t lastPos = 0;
            size_t startPos = trimmed.find("[[");
            while (startPos != std::string::npos) {
                if (startPos > lastPos) {
                    ImGui::TextWrapped("%s", trimmed.substr(lastPos, startPos - lastPos).c_str());
                    ImGui::SameLine(0, 0);
                }
                size_t endPos = trimmed.find("]]", startPos + 2);
                if (endPos != std::string::npos) {
                    std::string linkName = trimmed.substr(startPos + 2, endPos - startPos - 2);
                    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.3f, 0.5f, 1.0f));
                    if (ImGui::SmallButton(linkName.c_str())) {
                         app.selectedFilename = linkName + ".md";
                    }
                    ImGui::PopStyleColor();
                    ImGui::SameLine(0, 0);
                    lastPos = endPos + 2;
                    startPos = trimmed.find("[[", lastPos);
                } else { break; }
            }
            if (lastPos < trimmed.size()) {
                ImGui::TextWrapped("%s", trimmed.substr(lastPos).c_str());
            }
        }
    }
}

std::tm ToLocalTime(std::time_t tt) {
    std::tm tm = {};
#if defined(_WIN32)
    localtime_s(&tm, &tt);
#else
    localtime_r(&tt, &tm);
#endif
    return tm;
}

int TextEditCallback(ImGuiInputTextCallbackData* data) {
    if (data->EventFlag == ImGuiInputTextFlags_CallbackResize) {
        auto* str = static_cast<std::string*>(data->UserData);
        str->resize(data->BufTextLen);
        data->Buf = str->data();
    }
    return 0;
}

bool InputTextMultilineString(const char* label, std::string* str, const ImVec2& size, ImGuiInputTextFlags flags = 0) {
    flags |= ImGuiInputTextFlags_CallbackResize;
    if (str->capacity() == 0) {
        str->reserve(1024);
    }
    return ImGui::InputTextMultiline(label, str->data(), str->capacity() + 1, size, flags, TextEditCallback, str);
}

bool TaskCard(const char* id, const std::string& text, float width) {
    ImGuiStyle& style = ImGui::GetStyle();
    float paddingX = style.FramePadding.x;
    float paddingY = style.FramePadding.y;
    float wrapWidth = width - paddingX * 2.0f;
    if (wrapWidth < 1.0f) {
        wrapWidth = 1.0f;
    }

    ImVec2 textSize = ImGui::CalcTextSize(text.c_str(), nullptr, false, wrapWidth);
    float height = textSize.y + paddingY * 2.0f;

    ImGui::PushID(id);
    ImGui::InvisibleButton("##task", ImVec2(width, height));
    bool hovered = ImGui::IsItemHovered();
    bool active = ImGui::IsItemActive();
    bool clicked = ImGui::IsItemClicked();

    ImU32 bg = ImGui::GetColorU32(active ? ImGuiCol_ButtonActive : (hovered ? ImGuiCol_ButtonHovered : ImGuiCol_Button));
    ImU32 border = ImGui::GetColorU32(ImGuiCol_Border);
    ImVec2 min = ImGui::GetItemRectMin();
    ImVec2 max = ImGui::GetItemRectMax();
    ImDrawList* drawList = ImGui::GetWindowDrawList();
    drawList->AddRectFilled(min, max, bg, 4.0f);
    drawList->AddRect(min, max, border, 4.0f);

    ImVec2 textPos(min.x + paddingX, min.y + paddingY);
    drawList->AddText(ImGui::GetFont(), ImGui::GetFontSize(), textPos,
                      ImGui::GetColorU32(ImGuiCol_Text), text.c_str(), nullptr, wrapWidth);

    ImGui::PopID();
    return clicked;
}

std::filesystem::path ResolveBrowsePath(const char* buffer, const std::string& fallbackRoot) {
    namespace fs = std::filesystem;
    std::error_code ec;
    fs::path current;
    if (buffer && buffer[0] != '\0') {
        current = fs::path(buffer);
    } else if (!fallbackRoot.empty()) {
        current = fs::path(fallbackRoot);
    } else {
        current = fs::current_path(ec);
    }

    if (current.empty()) {
        current = fs::current_path(ec);
    }

    if (!fs::exists(current, ec) || !fs::is_directory(current, ec)) {
        fs::path parent = current.parent_path();
        if (!parent.empty() && fs::exists(parent, ec) && fs::is_directory(parent, ec)) {
            current = parent;
        }
    }

    if (current.empty()) {
        current = fs::current_path(ec);
    }

    return current;
}

void SetPathBuffer(char* buffer, size_t bufferSize, const std::filesystem::path& path) {
    if (!buffer || bufferSize == 0) {
        return;
    }
    std::string value = path.string();
    std::snprintf(buffer, bufferSize, "%s", value.c_str());
}

std::vector<std::filesystem::path> GetRootShortcuts() {
    namespace fs = std::filesystem;
    std::vector<fs::path> roots;
    std::error_code ec;

#if defined(_WIN32)
    for (char drive = 'A'; drive <= 'Z'; ++drive) {
        std::string root = std::string(1, drive) + ":\\";
        if (fs::exists(root, ec)) {
            roots.emplace_back(root);
        }
    }
#elif defined(__APPLE__)
    const char* candidates[] = { "/", "/Volumes", "/Users" };
    for (const char* path : candidates) {
        if (fs::exists(path, ec)) {
            roots.emplace_back(path);
        }
    }
#else
    const char* candidates[] = { "/", "/mnt", "/media", "/run/media", "/home" };
    for (const char* path : candidates) {
        if (fs::exists(path, ec)) {
            roots.emplace_back(path);
        }
    }
#endif

    if (roots.empty()) {
        roots.emplace_back("/");
    }
    return roots;
}

bool DrawFolderBrowser(const char* id,
                       char* pathBuffer,
                       size_t bufferSize,
                       const std::string& fallbackRoot) {
    namespace fs = std::filesystem;
    ImGui::PushID(id);

    fs::path current = ResolveBrowsePath(pathBuffer, fallbackRoot);
    bool updated = false;

    ImGui::Text("Localizar:");
    ImGui::SameLine();
    ImGui::TextUnformatted(current.string().c_str());

    if (ImGui::Button("Subir")) {
        if (current.has_parent_path()) {
            current = current.parent_path();
            updated = true;
        }
    }
    ImGui::SameLine();
    if (ImGui::Button("Usar Atual")) {
        updated = true;
    }

    ImGui::Separator();
    ImGui::Text("Raízes:");
    if (ImGui::BeginChild("Roots", ImVec2(0, 70), true)) {
        auto roots = GetRootShortcuts();
        for (const auto& root : roots) {
            std::string label = root.string();
            if (ImGui::Selectable(label.c_str())) {
                current = root;
                updated = true;
            }
        }
    }
    ImGui::EndChild();

    ImGui::Text("Pastas:");
    if (ImGui::BeginChild("FolderList", ImVec2(0, 200), true)) {
        std::error_code ec;
        if (!fs::exists(current, ec) || !fs::is_directory(current, ec)) {
            ImGui::TextDisabled("Pasta não disponível.");
        } else {
            std::vector<fs::directory_entry> entries;
            for (const auto& entry : fs::directory_iterator(current, ec)) {
                if (entry.is_directory(ec)) {
                    entries.push_back(entry);
                }
            }
            if (ec) {
                ImGui::TextDisabled("Não foi possível ler a pasta.");
            } else {
                std::sort(entries.begin(), entries.end(),
                          [](const fs::directory_entry& a, const fs::directory_entry& b) {
                              return a.path().filename().string() < b.path().filename().string();
                          });
                for (const auto& entry : entries) {
                    std::string name = entry.path().filename().string();
                    if (name.empty()) {
                        name = entry.path().string();
                    }
                    if (ImGui::Selectable(name.c_str())) {
                        current = entry.path();
                        updated = true;
                    }
                }
            }
        }
    }
    ImGui::EndChild();

    if (updated) {
        SetPathBuffer(pathBuffer, bufferSize, current);
    }

    ImGui::PopID();
    return updated; // This return value isn't used for "confirmed", but we need a specific one for file confirmation.
}

bool DrawFileBrowser(const char* id,
                     char* pathBuffer,
                     size_t bufferSize,
                     const std::string& fallbackRoot) {
    namespace fs = std::filesystem;
    ImGui::PushID(id);

    fs::path current = ResolveBrowsePath(pathBuffer, fallbackRoot);
    if (!fs::is_directory(current)) {
        if (current.has_parent_path()) current = current.parent_path();
        else current = "/";
    }

    bool updated = false;
    bool confirmed = false;

    ImGui::Text("Localizar:");
    ImGui::SameLine();
    ImGui::TextUnformatted(current.string().c_str());

    if (ImGui::Button("Subir")) {
        if (current.has_parent_path()) {
            current = current.parent_path();
            updated = true;
        }
    }
    
    ImGui::Separator();
    
    // Removed redundant InputText "Path" as the caller (DrawUI) usually renders one.

    ImGui::Text("Arquivos:");
    if (ImGui::BeginChild("FileList", ImVec2(0, 300), true)) {
        std::error_code ec;
        if (!fs::exists(current, ec) || !fs::is_directory(current, ec)) {
            ImGui::TextDisabled("Folder not available.");
        } else {
            std::vector<fs::directory_entry> entries;
            for (const auto& entry : fs::directory_iterator(current, ec)) {
                entries.push_back(entry);
            }
            std::sort(entries.begin(), entries.end(),
                      [](const fs::directory_entry& a, const fs::directory_entry& b) {
                          if (a.is_directory() != b.is_directory()) 
                              return a.is_directory() > b.is_directory();
                          return a.path().filename().string() < b.path().filename().string();
                      });

            std::string currentSelection = pathBuffer;

            for (const auto& entry : entries) {
                std::string name = entry.path().filename().string();
                if (entry.is_directory()) name = "[DIR] " + name;
                else name = "📄 " + name;

                bool isSelected = false;
                if (!entry.is_directory()) {
                     if (currentSelection == entry.path().string()) {
                         isSelected = true;
                     }
                }

                if (ImGui::Selectable(name.c_str(), isSelected, ImGuiSelectableFlags_AllowDoubleClick)) {
                    if (entry.is_directory()) {
                        if (ImGui::IsMouseDoubleClicked(0)) {
                            current = entry.path();
                            updated = true;
                        }
                    } else {
                        // It's a file
                        std::string fullPath = entry.path().string();
                        if (fullPath.length() < bufferSize) {
                            std::strcpy(pathBuffer, fullPath.c_str());
                            if (ImGui::IsMouseDoubleClicked(0)) {
                                confirmed = true;
                            }
                        }
                    }
                }
            }
        }
    }
    ImGui::EndChild();

    if (updated) {
        SetPathBuffer(pathBuffer, bufferSize, current);
    }

    ImGui::PopID();
    return confirmed;
}


void DrawNodeGraph(AppState& app) {
    auto label = [&app](const char* withEmoji, const char* plain) {
        return app.emojiEnabled ? withEmoji : plain;
    };
    ImNodes::EditorContextSet((ImNodesEditorContext*)app.mainGraphContext);
    ImNodes::BeginNodeEditor();

    // 1. Draw Nodes
    for (auto& node : app.graphNodes) {
        // Set position for physics, but skip if currently selected (let user drag)
        if (app.physicsEnabled && !ImNodes::IsNodeSelected(node.id)) {
            ImNodes::SetNodeGridSpacePos(node.id, ImVec2(node.x, node.y));
        }

        bool isTask = (node.type == NodeType::TASK);
        if (isTask) {
            ImNodes::PushColorStyle(ImNodesCol_NodeBackground, IM_COL32(50, 50, 50, 255));
            ImNodes::PushColorStyle(ImNodesCol_TitleBar, 
                node.isCompleted ? IM_COL32(46, 125, 50, 200) : IM_COL32(230, 81, 0, 200));
        }

        ImNodes::BeginNode(node.id);
        
        ImNodes::BeginNodeTitleBar();
        ImGui::PushTextWrapPos(ImGui::GetCursorPos().x + (node.wrapW - 30.0f)); // Reserve space for padding
        if (isTask) {
             const char* emoji = "📋 "; // To-Do
             if (node.isCompleted) emoji = "✅ ";
             else if (node.isInProgress) emoji = "⏳ ";
             
             ImGui::TextUnformatted(emoji);
             ImGui::SameLine();
        }
        ImGui::TextUnformatted(node.title.c_str());
        ImGui::PopTextWrapPos();
        ImNodes::EndNodeTitleBar();

        ImNodes::BeginOutputAttribute(node.id << 8); 
        ImGui::Dummy(ImVec2(10, 0));
        ImNodes::EndOutputAttribute();

        ImNodes::BeginInputAttribute((node.id << 8) + 1);
        ImGui::Dummy(ImVec2(10, 0));
        ImNodes::EndInputAttribute();

        ImNodes::EndNode();

        if (isTask) {
            ImNodes::PopColorStyle();
            ImNodes::PopColorStyle();
        }
    }

    // 2. Draw Links
    for (const auto& link : app.graphLinks) {
        int startAttr = (link.startNode << 8);
        int endAttr = (link.endNode << 8) + 1;
        ImNodes::Link(link.id, startAttr, endAttr);
    }

    ImNodes::EndNodeEditor();

    // 3. Sync User Dragging back to AppState
    std::unordered_set<int> selectedNodes;
    for (auto& node : app.graphNodes) {
        if (ImNodes::IsNodeSelected(node.id)) {
            selectedNodes.insert(node.id);
            ImVec2 pos = ImNodes::GetNodeGridSpacePos(node.id);
            node.x = pos.x;
            node.y = pos.y;
            node.vx = 0;
            node.vy = 0;
        }
    }

    // 4. Update Physics
    if (!app.graphNodes.empty() && app.physicsEnabled) {
        app.UpdateGraphPhysics(selectedNodes);
    }
}

} // namespace

void DrawUI(AppState& app) {
    bool needRefresh = false;
    if (app.pendingRefresh.exchange(false)) {
        app.RefreshInbox();
        app.RefreshAllInsights();
    }
    auto label = [&app](const char* withEmoji, const char* plain) {
        return app.emojiEnabled ? withEmoji : plain;
    };
    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(viewport->WorkPos);
    ImGui::SetNextWindowSize(viewport->WorkSize);
    ImGui::Begin("Main", NULL, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_MenuBar);
    const bool hasProject = (app.organizerService != nullptr);
    const bool canChangeProject = !app.isProcessing.load();

    if (ImGui::BeginMenuBar()) {
        if (ImGui::BeginMenu("File")) {
            if (ImGui::MenuItem("Novo Projeto...", nullptr, false, canChangeProject)) {
                app.showNewProjectModal = true;
            }
            if (ImGui::MenuItem("Abrir Projeto...", nullptr, false, canChangeProject)) {
                app.showOpenProjectModal = true;
            }
            if (ImGui::MenuItem("Abrir Arquivo...", nullptr, false, !app.isProcessing.load())) {
                app.showOpenFileModal = true;
                // Reset buffer or set default?
                app.openFilePathBuffer[0] = '\0';
            }
            if (ImGui::MenuItem("Salvar Projeto", nullptr, false, hasProject && canChangeProject)) {
                if (!app.SaveProject()) {
                    app.AppendLog("[SYSTEM] Falha ao salvar projeto.\n");
                }
            }
            if (ImGui::MenuItem("Salvar Projeto Como...", nullptr, false, canChangeProject)) {
                app.showSaveAsProjectModal = true;
            }
            if (ImGui::MenuItem("Fechar Arquivo", nullptr, false, app.selectedExternalFileIndex != -1)) {
                if (app.selectedExternalFileIndex >= 0 && app.selectedExternalFileIndex < (int)app.externalFiles.size()) {
                    app.externalFiles.erase(app.externalFiles.begin() + app.selectedExternalFileIndex);
                    if (app.selectedExternalFileIndex >= (int)app.externalFiles.size()) {
                        app.selectedExternalFileIndex = (int)app.externalFiles.size() - 1;
                    }
                }
            }
            ImGui::Separator();
            if (ImGui::MenuItem("Sair", nullptr, false, canChangeProject)) {
                app.requestExit = true;
            }
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("⚙️ Configurações")) {
             if (ImGui::MenuItem("Preferências...", nullptr, false, true)) {
                 app.showSettingsModal = true;
             }
             ImGui::EndMenu();
        }

        if (ImGui::BeginMenu(label("🛠️ Ferramentas", "Ferramentas"))) {
            if (ImGui::BeginMenu(label("🕸️ Configurações do Grafo", "Configurações do Grafo"))) {
                if (ImGui::MenuItem(label("🕸️ Mostrar Tarefas", "Mostrar Tarefas"), nullptr, app.showTasksInGraph)) {
                    app.showTasksInGraph = !app.showTasksInGraph;
                    app.RebuildGraph();
                }
                if (ImGui::MenuItem(label("🔄 Animação", "Animação"), nullptr, app.physicsEnabled)) {
                    app.physicsEnabled = !app.physicsEnabled;
                }
                ImGui::Separator();
                if (ImGui::MenuItem(label("📤 Exportar Mermaid", "Exportar Mermaid"))) {
                    std::string mermaid = app.ExportToMermaid();
                    ImGui::SetClipboardText(mermaid.c_str());
                    app.outputLog += "[Info] Mapa mental exportado para o clipboard.\n";
                }
                if (ImGui::MenuItem(label("📁 Exportar Full (.md)", "Exportar Completo (.md)"))) {
                    std::string fullMd = app.ExportFullMarkdown();
                    ImGui::SetClipboardText(fullMd.c_str());
                    app.outputLog += "[Info] Conhecimento completo exportado para o clipboard.\n";
                }
                if (ImGui::MenuItem(label("🎯 Centralizar Grafo", "Centralizar Grafo"))) {
                    app.CenterGraph();
                }
                ImGui::EndMenu();
            }
            ImGui::EndMenu();
        }

        ImGui::EndMenuBar();
    }

    // --- Settings Modal ---
    if (app.showSettingsModal) {
        ImGui::OpenPopup("Preferências");
    }
    if (ImGui::BeginPopupModal("Preferências", &app.showSettingsModal, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::Text("Configurações Gerais");
        ImGui::Separator();
        
        // AI Persona Selection
        ImGui::Text("🧠 Personalidade da IA");
        
        const char* personas[] = { "Analista Cognitivo", "Secretário Executivo", "Brainstormer" };
        int currentItem = static_cast<int>(app.currentPersona);
        
        if (ImGui::Combo("##persona", &currentItem, personas, IM_ARRAYSIZE(personas))) {
            app.currentPersona = static_cast<domain::AIPersona>(currentItem);
            if (app.organizerService) {
                app.organizerService->setAIPersona(app.currentPersona);
            }
        }
        
        ImGui::TextDisabled((currentItem == 0) ? "Focado em tensão, conflito e estratégia." : 
                            (currentItem == 1) ? "Focado em tarefas, resumo e eficiência." : 
                            "Focado em expansão, criatividade e divergência.");

        ImGui::Separator();
        ImGui::Dummy(ImVec2(0, 10));
        
        if (ImGui::Button("Fechar", ImVec2(120, 0))) {
            app.showSettingsModal = false;
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }

    if (app.showNewProjectModal) {
        std::filesystem::path defaultPath = ResolveBrowsePath(nullptr, app.projectRoot);
        SetPathBuffer(app.projectPathBuffer, sizeof(app.projectPathBuffer), defaultPath);
        ImGui::OpenPopup("New Project");
        app.showNewProjectModal = false;
    }
    if (ImGui::BeginPopupModal("New Project", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::Text("Pasta do projeto:");
        ImGui::InputText("##newproject", app.projectPathBuffer, sizeof(app.projectPathBuffer));
        DrawFolderBrowser("new_project_browser",
                          app.projectPathBuffer,
                          sizeof(app.projectPathBuffer),
                          app.projectRoot);
        if (ImGui::Button("Criar", ImVec2(120, 0))) {
            if (!app.NewProject(app.projectPathBuffer)) {
                app.AppendLog("[SYSTEM] Falha ao criar projeto.\n");
            }
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancelar", ImVec2(120, 0))) {
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }

    if (app.showOpenProjectModal) {
        std::filesystem::path defaultPath = ResolveBrowsePath(nullptr, app.projectRoot);
        SetPathBuffer(app.projectPathBuffer, sizeof(app.projectPathBuffer), defaultPath);
        ImGui::OpenPopup("Open Project");
        app.showOpenProjectModal = false;
    }
    if (ImGui::BeginPopupModal("Open Project", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::Text("Pasta do projeto:");
        ImGui::InputText("##openproject", app.projectPathBuffer, sizeof(app.projectPathBuffer));
        DrawFolderBrowser("open_project_browser",
                          app.projectPathBuffer,
                          sizeof(app.projectPathBuffer),
                          app.projectRoot);
        if (ImGui::Button("Abrir", ImVec2(120, 0))) {
            if (!app.OpenProject(app.projectPathBuffer)) {
                app.AppendLog("[SYSTEM] Falha ao abrir projeto.\n");
            }
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancelar", ImVec2(120, 0))) {
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }

    if (app.showSaveAsProjectModal) {
        std::filesystem::path defaultPath = ResolveBrowsePath(nullptr, app.projectRoot);
        SetPathBuffer(app.projectPathBuffer, sizeof(app.projectPathBuffer), defaultPath);
        ImGui::OpenPopup("Save Project As");
        app.showSaveAsProjectModal = false;
    }
    if (ImGui::BeginPopupModal("Save Project As", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::Text("Pasta do projeto:");
        ImGui::InputText("##saveprojectas", app.projectPathBuffer, sizeof(app.projectPathBuffer));
        DrawFolderBrowser("save_project_browser",
                          app.projectPathBuffer,
                          sizeof(app.projectPathBuffer),
                          app.projectRoot);
        if (ImGui::Button("Salvar", ImVec2(120, 0))) {
            if (!app.SaveProjectAs(app.projectPathBuffer)) {
                app.AppendLog("[SYSTEM] Falha ao salvar projeto como.\n");
            }
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancelar", ImVec2(120, 0))) {
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }

    if (app.showOpenFileModal) {
        ImGui::OpenPopup("Open File");
        app.showOpenFileModal = false;
    }
    if (ImGui::BeginPopupModal("Open File", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::Text("Caminho do arquivo:");
        ImGui::InputText("##openfilepath", app.openFilePathBuffer, sizeof(app.openFilePathBuffer));
        if (DrawFileBrowser("open_file_browser", 
                        app.openFilePathBuffer, 
                        sizeof(app.openFilePathBuffer), 
                        app.projectRoot.empty() ? "/" : app.projectRoot)) {
            app.OpenExternalFile(app.openFilePathBuffer);
            ImGui::CloseCurrentPopup();
        }
        
        if (ImGui::Button("Abrir", ImVec2(120,0))) {
            app.OpenExternalFile(app.openFilePathBuffer);
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancelar", ImVec2(120,0))) {
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }

    if (hasProject) {
        ImGui::TextDisabled("Project: %s", app.projectRoot.c_str());
    } else {
        ImGui::TextDisabled("Nenhum projeto aberto.");
    }
    ImGui::Separator();

    if (ImGui::BeginTabBar("MyTabs")) {
        
        ImGuiTabItemFlags flags0 = (app.requestedTab == 0) ? ImGuiTabItemFlags_SetSelected : 0;
        if (ImGui::BeginTabItem(label("🎙️ Dashboard & Inbox", "Dashboard & Inbox"), NULL, flags0)) {
            if (app.requestedTab == 0) app.requestedTab = -1;
            app.activeTab = 0;
            if (!hasProject) {
                ImGui::TextDisabled("Nenhum projeto aberto.");
                ImGui::TextDisabled("Use File > New Project ou File > Open Project para comecar.");
            } else {
                ImGui::Spacing();
                
                if (ImGui::Button(label("🔄 Refresh Inbox", "Refresh Inbox"), ImVec2(150, 30))) {
                    app.RefreshInbox();
                }
                
                ImGui::Separator();
                
                ImGui::Text("Entrada (%zu ideias):", app.inboxThoughts.size());
                ImGui::BeginChild("InboxList", ImVec2(0, 200), true);
                for (const auto& thought : app.inboxThoughts) {
                    bool isSelected = (app.selectedInboxFilename == thought.filename);
                    if (ImGui::Selectable(thought.filename.c_str(), isSelected)) {
                        app.selectedInboxFilename = thought.filename;
                    }
                }
                ImGui::EndChild();

                ImGui::Spacing();
                
                bool processing = app.isProcessing.load();
                if (processing) ImGui::BeginDisabled();
                auto startBatch = [&app](bool force) {
                    app.isProcessing.store(true);
                    app.AppendLog(force ? "[SYSTEM] Starting AI reprocess (batch)...\n" : "[SYSTEM] Starting AI batch processing...\n");
                    std::thread([&app, force]() {
                        app.organizerService->processInbox(force);
                        bool consolidated = app.organizerService->updateConsolidatedTasks();
                        app.isProcessing.store(false);
                        app.AppendLog("[SYSTEM] Processing finished.\n");
                        app.AppendLog(consolidated ? "[SYSTEM] Consolidated tasks updated.\n" : "[SYSTEM] Consolidated tasks failed.\n");
                        app.pendingRefresh.store(true);
                    }).detach();
                };

                auto startSingle = [&app](const std::string& filename, bool force) {
                    app.isProcessing.store(true);
                    app.AppendLog(force ? "[SYSTEM] Starting AI reprocess for " + filename + "...\n"
                                        : "[SYSTEM] Starting AI processing for " + filename + "...\n");
                    std::thread([&app, filename, force]() {
                        auto result = app.organizerService->processInboxItem(filename, force);
                        switch (result) {
                        case application::OrganizerService::ProcessResult::Processed:
                            app.AppendLog("[SYSTEM] Processing finished for " + filename + ".\n");
                            break;
                        case application::OrganizerService::ProcessResult::SkippedUpToDate:
                            app.AppendLog("[SYSTEM] Skipped (up-to-date): " + filename + ".\n");
                            break;
                        case application::OrganizerService::ProcessResult::NotFound:
                            app.AppendLog("[SYSTEM] Inbox file not found: " + filename + ".\n");
                            break;
                        case application::OrganizerService::ProcessResult::Failed:
                            app.AppendLog("[SYSTEM] Processing failed for " + filename + ".\n");
                            break;
                        }
                        if (result != application::OrganizerService::ProcessResult::NotFound
                            && result != application::OrganizerService::ProcessResult::Failed) {
                            bool consolidated = app.organizerService->updateConsolidatedTasks();
                            app.AppendLog(consolidated ? "[SYSTEM] Consolidated tasks updated.\n" : "[SYSTEM] Consolidated tasks failed.\n");
                        }
                        app.isProcessing.store(false);
                        app.pendingRefresh.store(true);
                    }).detach();
                };

                const bool hasSelection = !app.selectedInboxFilename.empty();
                const char* runLabel = hasSelection
                    ? label("🧠 Run Selected", "Run Selected")
                    : label("🧠 Run AI Orchestrator", "Run AI Orchestrator");
                if (ImGui::Button(runLabel, ImVec2(250, 50))) {
                    if (hasSelection) {
                        startSingle(app.selectedInboxFilename, false);
                    } else {
                        startBatch(false);
                    }
                }
                if (hasSelection) {
                    ImGui::SameLine();
                    if (ImGui::Button(label("🧠 Run All", "Run All"), ImVec2(120, 50))) {
                        startBatch(false);
                    }
                    ImGui::SameLine();
                    if (ImGui::Button(label("🔁 Reprocess Selected", "Reprocess Selected"), ImVec2(180, 50))) {
                        startSingle(app.selectedInboxFilename, true);
                    }
                } else {
                    ImGui::SameLine();
                    if (ImGui::Button(label("🔁 Reprocess All", "Reprocess All"), ImVec2(150, 50))) {
                        startBatch(true);
                    }
                }
                if (processing) ImGui::EndDisabled();

                if (app.isProcessing.load()) {
                    ImGui::SameLine();
                    ImGui::TextColored(ImVec4(1, 1, 0, 1), "⏳ Thinking...");
                }
                if (app.isTranscribing.load()) {
                    ImGui::SameLine();
                    ImGui::TextColored(ImVec4(0, 1, 1, 1), "🎙️ Transcribing Audio...");
                }

                ImGui::Separator();
                ImGui::Text("System Log:");
                ImGui::BeginChild("Log", ImVec2(0, -100), true);
                std::string logSnapshot = app.GetLogSnapshot();
                ImGui::TextUnformatted(logSnapshot.c_str());
                if (ImGui::GetScrollY() >= ImGui::GetScrollMaxY())
                    ImGui::SetScrollHereY(1.0f);
                ImGui::EndChild();

                ImGui::Separator();
                ImGui::Text("%s", label("🔥 Activity Heatmap (Last 30 Days)", "Activity Heatmap (Last 30 Days)"));
                
                // Render Heatmap
                ImDrawList* draw_list = ImGui::GetWindowDrawList();
                ImVec2 p = ImGui::GetCursorScreenPos();
                float size = 15.0f;
                float spacing = 3.0f;
                int maxCount = 0;
                for (const auto& entry : app.activityHistory) {
                    if (entry.second > maxCount) maxCount = entry.second;
                }
                const auto now = std::chrono::system_clock::now();
                
                for (int i = 0; i < 30; ++i) {
                    ImVec4 color = ImVec4(0.15f, 0.15f, 0.15f, 1.0f);
                    auto day = now - std::chrono::hours(24 * (29 - i));
                    std::time_t tt = std::chrono::system_clock::to_time_t(day);
                    std::tm tm = ToLocalTime(tt);
                    char buffer[11];
                    std::strftime(buffer, sizeof(buffer), "%Y-%m-%d", &tm);
                    auto it = app.activityHistory.find(buffer);
                    int count = (it != app.activityHistory.end()) ? it->second : 0;
                    if (count > 0) {
                        float intensity = (maxCount > 0) ? (static_cast<float>(count) / maxCount) : 0.0f;
                        float green = 0.3f + (0.7f * intensity);
                        color = ImVec4(0.0f, green, 0.0f, 1.0f);
                    }

                    draw_list->AddRectFilled(ImVec2(p.x + i * (size + spacing), p.y), 
                                            ImVec2(p.x + i * (size + spacing) + size, p.y + size), 
                                            ImColor(color));
                }
                ImGui::Dummy(ImVec2(0, size + 10));
            }

            ImGui::EndTabItem();
        }

        ImGuiTabItemFlags flags1 = (app.requestedTab == 1) ? ImGuiTabItemFlags_SetSelected : 0;
        if (ImGui::BeginTabItem(label("📚 Organized Knowledge", "Organized Knowledge"), NULL, flags1)) {
            if (app.requestedTab == 1) app.requestedTab = -1;
            bool enteringKnowledge = (app.activeTab != 1);
            app.activeTab = 1;
            if (enteringKnowledge && hasProject && !app.isProcessing.load()) {
                app.RefreshAllInsights();
            }
            if (!hasProject) {
                ImGui::TextDisabled("Nenhum projeto aberto.");
                ImGui::TextDisabled("Use File > New Project ou File > Open Project para comecar.");
            } else {
                ImGui::Checkbox("Visao unificada", &app.unifiedKnowledgeView);
                ImGui::Separator();

                if (app.unifiedKnowledgeView) {
                    ImGui::BeginChild("UnifiedKnowledge", ImVec2(0, 0), true);
                    if (app.unifiedKnowledge.empty()) {
                        ImGui::TextDisabled("Nenhum insight disponivel.");
                    } else {
                        ImGui::PushTextWrapPos(0.0f);
                        ImGui::TextUnformatted(app.unifiedKnowledge.c_str());
                        ImGui::PopTextWrapPos();
                    }
                    ImGui::EndChild();
                } else {
                    // Coluna da Esquerda: Lista de Arquivos
                    ImGui::BeginChild("NotesList", ImVec2(250, 0), true);
                    for (const auto& insight : app.allInsights) {
                        const std::string& filename = insight.getMetadata().id;
                        bool isSelected = (app.selectedFilename == filename);
                        if (ImGui::Selectable(filename.c_str(), isSelected)) {
                            app.selectedFilename = filename;
                            app.selectedNoteContent = insight.getContent();
                            std::snprintf(app.saveAsFilename, sizeof(app.saveAsFilename), "%s", filename.c_str());
                            
                            // Update Domain Insight
                            domain::Insight::Metadata meta;
                            meta.id = filename;
                            app.currentInsight = std::make_unique<domain::Insight>(meta, app.selectedNoteContent);
                            app.currentInsight->parseActionablesFromContent();

                            // Fetch Backlinks
                            if (app.organizerService) {
                                app.currentBacklinks = app.organizerService->getBacklinks(filename);
                            }
                        }
                    }
                    ImGui::EndChild();

                    ImGui::SameLine();

                    // Coluna da Direita: Conteudo / Editor
                    ImGui::BeginChild("NoteContent", ImVec2(0, 0), true);
                    if (!app.selectedFilename.empty()) {
                        if (ImGui::Button(label("💾 Save Changes", "Save Changes"), ImVec2(150, 30))) {
                            app.organizerService->updateNote(app.selectedFilename, app.selectedNoteContent);
                            app.AppendLog("[SYSTEM] Saved changes to " + app.selectedFilename + "\n");
                            app.RefreshAllInsights();
                        }
                        ImGui::SameLine();

                        ImGui::SetNextItemWidth(200.0f);
                        ImGui::InputText("##saveasname", app.saveAsFilename, sizeof(app.saveAsFilename));
                        ImGui::SameLine();
                        if (ImGui::Button(label("📂 Save As", "Save As"), ImVec2(100, 30))) {
                            std::string newName = app.saveAsFilename;
                            if (!newName.empty()) {
                                if (newName.find(".md") == std::string::npos && newName.find(".txt") == std::string::npos) {
                                    newName += ".md";
                                }
                                app.organizerService->updateNote(newName, app.selectedNoteContent);
                                app.selectedFilename = newName;
                                app.AppendLog("[SYSTEM] Saved as " + newName + "\n");
                                app.RefreshAllInsights();
                            }
                        }

                        ImGui::Separator();
                        
                        // Editor / Visual Switcher
                        if (ImGui::BeginTabBar("EditorTabs")) {
                            if (ImGui::BeginTabItem(label("📝 Editor", "Editor"))) {
                                app.previewMode = false;
                                ImGui::EndTabItem();
                            }
                            if (ImGui::BeginTabItem(label("👁️ Visual", "Visual"))) {
                                app.previewMode = true;
                                ImGui::EndTabItem();
                            }
                            ImGui::EndTabBar();
                        }

                        if (app.previewMode) {
                            ImGui::BeginChild("PreviewScroll", ImVec2(0, -200), true);
                            DrawMarkdownPreview(app, app.selectedNoteContent, false);
                            ImGui::EndChild();
                        } else {
                            // Editor de texto (InputTextMultiline com Resize)
                            if (InputTextMultilineString("##editor", &app.selectedNoteContent, ImVec2(-FLT_MIN, -200))) {
                                if (app.currentInsight) {
                                    app.currentInsight->setContent(app.selectedNoteContent);
                                    app.currentInsight->parseActionablesFromContent();
                                }
                            }
                        }
                        
                        ImGui::Separator();
                        ImGui::Text("%s", label("🔗 Backlinks (Mencionado em):", "Backlinks (Mencionado em):"));
                        if (app.currentBacklinks.empty()) {
                            ImGui::TextDisabled("Nenhuma referencia encontrada.");
                        } else {
                            for (const auto& link : app.currentBacklinks) {
                                if (ImGui::Button(link.c_str())) {
                                    // Logic to switch to this note would go here
                                    // For simplicity, we just log it
                                    app.AppendLog("[UI] Jumping to " + link + "\n");
                                }
                                ImGui::SameLine();
                            }
                            ImGui::NewLine();
                        }
                    } else {
                        ImGui::Text("Select a note from the list to view or edit.");
                    }
                    ImGui::EndChild();
                }
            }

            ImGui::EndTabItem();
        }

        ImGuiTabItemFlags flags2 = (app.requestedTab == 2) ? ImGuiTabItemFlags_SetSelected : 0;
        if (ImGui::BeginTabItem(label("🏭 Execução", "Execução"), NULL, flags2)) {
            if (app.requestedTab == 2) app.requestedTab = -1;
            app.activeTab = 2;
            if (!hasProject) {
                ImGui::TextDisabled("Nenhum projeto aberto.");
                ImGui::TextDisabled("Use File > New Project ou File > Open Project para comecar.");
            } else {
                if (ImGui::Button(label("🔄 Refresh Tasks", "Refresh Tasks"), ImVec2(120, 30))) {
                    app.RefreshAllInsights();
                }
                ImGui::Separator();

                const ImGuiTableFlags tableFlags = ImGuiTableFlags_SizingStretchSame
                    | ImGuiTableFlags_NoBordersInBody
                    | ImGuiTableFlags_NoSavedSettings
                    | ImGuiTableFlags_PadOuterX;
                ImGui::PushStyleVar(ImGuiStyleVar_CellPadding, ImVec2(12.0f, 12.0f));
                ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(8.0f, 8.0f));

                const bool useConsolidatedTasks = app.consolidatedInsight && !app.consolidatedInsight->getActionables().empty();
                std::vector<const domain::Insight*> taskSources;
                if (useConsolidatedTasks) {
                    taskSources.push_back(app.consolidatedInsight.get());
                } else {
                    for (const auto& insight : app.allInsights) {
                        taskSources.push_back(&insight);
                    }
                }

                auto renderColumn = [&](const char* childId,
                                        const char* title,
                                        const ImVec4& titleColor,
                                        const ImVec4& backgroundColor,
                                        auto shouldShow,
                                        bool completed,
                                        bool inProgress) {
                    ImGui::TextColored(titleColor, "%s", title);
                    ImGui::Dummy(ImVec2(0.0f, 4.0f));
                    ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 6.0f);
                    ImGui::PushStyleVar(ImGuiStyleVar_ChildBorderSize, 1.0f);
                    ImGui::PushStyleColor(ImGuiCol_ChildBg, backgroundColor);
                    ImGui::BeginChild(childId, ImVec2(0, 0), true);
                    for (const auto* insight : taskSources) {
                        const auto& actionables = insight->getActionables();
                        for (size_t i = 0; i < actionables.size(); ++i) {
                            if (!shouldShow(actionables[i])) {
                                continue;
                            }
                            std::string itemId = std::string(childId) + insight->getMetadata().id + std::to_string(i);
                            float cardWidth = ImGui::GetContentRegionAvail().x;
                            if (cardWidth < 1.0f) {
                                cardWidth = 1.0f;
                            }
                            if (TaskCard(itemId.c_str(), actionables[i].description, cardWidth)) {
                                app.selectedFilename = insight->getMetadata().id;
                                app.selectedNoteContent = insight->getContent();

                                domain::Insight::Metadata meta;
                                meta.id = app.selectedFilename;
                                app.currentInsight = std::make_unique<domain::Insight>(meta, app.selectedNoteContent);
                                app.currentInsight->parseActionablesFromContent();
                            }
                            if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort)) {
                                ImGui::BeginTooltip();
                                if (useConsolidatedTasks) {
                                    ImGui::Text("Origem: consolidado");
                                } else {
                                    ImGui::Text("Origem: %s", insight->getMetadata().id.c_str());
                                }
                                ImGui::EndTooltip();
                            }

                            if (ImGui::BeginDragDropSource()) {
                                std::string payload = insight->getMetadata().id + "|" + std::to_string(i);
                                ImGui::SetDragDropPayload("DND_TASK", payload.c_str(), payload.size() + 1);
                                ImGui::Text("Movendo: %s", actionables[i].description.c_str());
                                ImGui::EndDragDropSource();
                            }
                        }
                    }
                    ImGui::EndChild();
                    if (app.organizerService && ImGui::BeginDragDropTarget()) {
                        if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("DND_TASK")) {
                            std::string data = static_cast<const char*>(payload->Data);
                            size_t sep = data.find('|');
                            std::string filename = data.substr(0, sep);
                            int index = std::stoi(data.substr(sep + 1));
                            app.organizerService->setTaskStatus(filename, index, completed, inProgress);
                            needRefresh = true;
                        }
                        ImGui::EndDragDropTarget();
                    }
                    ImGui::PopStyleColor();
                    ImGui::PopStyleVar(2);
                };

                if (ImGui::BeginTable("ExecutionColumns", 3, tableFlags)) {
                    ImGui::TableNextRow();
                    ImGui::TableNextColumn();
                    renderColumn(
                        "todo",
                        label("📋 A FAZER", "A FAZER"),
                        ImVec4(1, 0.8f, 0, 1),
                        ImVec4(0.08f, 0.09f, 0.10f, 1.0f),
                        [](const domain::Actionable& task) { return !task.isCompleted && !task.isInProgress; },
                        false,
                        false
                    );

                    ImGui::TableNextColumn();
                    renderColumn(
                        "progress",
                        label("⏳ EM ANDAMENTO", "EM ANDAMENTO"),
                        ImVec4(0, 0.7f, 1, 1),
                        ImVec4(0.07f, 0.09f, 0.11f, 1.0f),
                        [](const domain::Actionable& task) { return task.isInProgress; },
                        false,
                        true
                    );

                    ImGui::TableNextColumn();
                    renderColumn(
                        "done",
                        label("✅ FEITO", "FEITO"),
                        ImVec4(0, 1, 0, 1),
                        ImVec4(0.07f, 0.10f, 0.08f, 1.0f),
                        [](const domain::Actionable& task) { return task.isCompleted; },
                        true,
                        false
                    );

                    ImGui::EndTable();
                }

                ImGui::PopStyleVar(2);
            }
            ImGui::EndTabItem();
        }
        
        ImGuiTabItemFlags flags3 = (app.requestedTab == 3) ? ImGuiTabItemFlags_SetSelected : 0;
        if (ImGui::BeginTabItem(label("🕸️ Neural Web", "Neural Web"), NULL, flags3)) {
            if (app.requestedTab == 3) app.requestedTab = -1;
            bool enteringGraph = (app.activeTab != 3);
            app.activeTab = 3;

            if (enteringGraph && hasProject) {
                app.RebuildGraph();
            }

            if (!hasProject) {
                ImGui::TextDisabled("Nenhum projeto aberto.");
            } else {
                DrawNodeGraph(app);
            }
            ImGui::EndTabItem();
        }

        // External Files Tab
        ImGuiTabItemFlags flagsExt = (app.requestedTab == 4) ? ImGuiTabItemFlags_SetSelected : 0;
        if (ImGui::BeginTabItem(label("📂 External Files", "External Files"), NULL, flagsExt)) {
            if (app.requestedTab == 4) app.requestedTab = -1;
            app.activeTab = 4;

            if (app.externalFiles.empty()) {
                ImGui::TextDisabled("No external files open.");
                ImGui::TextDisabled("Use File > Open File... to open .txt or .md files.");
            } else {
                // File Tabs
                if (ImGui::BeginTabBar("ExternalFilesTabs", ImGuiTabBarFlags_AutoSelectNewTabs)) {
                    for (int i = 0; i < (int)app.externalFiles.size(); ++i) {
                        bool open = true;
                        if (ImGui::BeginTabItem(app.externalFiles[i].filename.c_str(), &open)) {
                            app.selectedExternalFileIndex = i;
                            
                            if (ImGui::Button(label("💾 Save", "Save"))) {
                                app.SaveExternalFile(i);
                            }
                            ImGui::SameLine();
                            if (ImGui::Button(label("❌ Close", "Close"))) {
                                open = false;
                            }
                            ImGui::SameLine();
                            if (ImGui::Checkbox(label("👁️ Preview", "Preview"), &app.previewMode)) {
                                // toggle
                            }

                            ImGui::Separator();
                            
                            ExternalFile& file = app.externalFiles[i];

                            if (app.previewMode) {
                                ImGui::BeginChild("ExtPreview", ImVec2(0, -10), true);
                                DrawMarkdownPreview(app, file.content, true);
                                ImGui::EndChild();
                            } else {
                                if (InputTextMultilineString("##exteditor", &file.content, ImVec2(-FLT_MIN, -10))) {
                                    file.modified = true;
                                }
                            }
                            
                            ImGui::EndTabItem();
                        }
                        if (!open) {
                            app.externalFiles.erase(app.externalFiles.begin() + i);
                            if (app.selectedExternalFileIndex >= i) app.selectedExternalFileIndex--;
                            i--; // adjust index
                        }
                    }
                    ImGui::EndTabBar();
                }
            }
            ImGui::EndTabItem();
        }
        
        ImGui::EndTabBar();
    }

    ImGui::End();

    if (needRefresh) {
        app.RefreshAllInsights();
    }
}

} // namespace ideawalker::ui
