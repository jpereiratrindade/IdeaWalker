#include "domain/writing/MermaidParser.hpp"
#include <sstream>
#include <unordered_map>
#include <unordered_set>
#include <algorithm>
#include <cmath>

namespace ideawalker::domain::writing {

namespace {
    bool StartsWith(const std::string& text, const char* prefix) {
        return text.compare(0, std::string(prefix).length(), prefix) == 0;
    }

    void Trim(std::string& s) {
        if (s.empty()) return;
        s.erase(0, s.find_first_not_of(" \t"));
        if (!s.empty()) s.erase(s.find_last_not_of(" \t") + 1);
    }

    void ParseNodeStr(const std::string& raw, std::string& idOut, std::string& labelOut, NodeShape& shapeOut) {
        shapeOut = NodeShape::BOX; 
        
        size_t bestStart = std::string::npos;
        size_t firstBracket = raw.find_first_of("[({)>");
        size_t firstParenClose = raw.find(")"); 
        
        if (firstBracket != std::string::npos) bestStart = firstBracket;
        if (firstParenClose != std::string::npos && (bestStart == std::string::npos || firstParenClose < bestStart)) bestStart = firstParenClose;

        size_t start = std::string::npos;
        size_t end = std::string::npos;

        if (bestStart != std::string::npos) {
            std::string prefix = raw.substr(bestStart, 2);
            if (prefix == "))") { shapeOut = NodeShape::BANG; start = bestStart + 2; end = raw.rfind("(("); }
            else if (prefix[0] == ')') { shapeOut = NodeShape::CLOUD; start = bestStart + 1; end = raw.rfind("("); }
            else if (prefix == "{{") { shapeOut = NodeShape::HEXAGON; start = bestStart + 2; end = raw.rfind("}}"); }
            else if (prefix == "((") { shapeOut = NodeShape::CIRCLE; start = bestStart + 2; end = raw.rfind("))"); }
            else if (prefix == "[[") { shapeOut = NodeShape::SUBROUTINE; start = bestStart + 2; end = raw.rfind("]]"); }
            else if (prefix == "[(") { shapeOut = NodeShape::CYLINDER; start = bestStart + 2; end = raw.rfind(")]"); }
            else if (prefix == "([") { shapeOut = NodeShape::STADIUM; start = bestStart + 2; end = raw.rfind("])"); }
            else if (prefix[0] == '[') { shapeOut = NodeShape::BOX; start = bestStart + 1; end = raw.rfind("]"); }
            else if (prefix[0] == '(') { shapeOut = NodeShape::ROUNDED_BOX; start = bestStart + 1; end = raw.rfind(")"); }
            else if (prefix[0] == '{') { shapeOut = NodeShape::RHOMBUS; start = bestStart + 1; end = raw.rfind("}"); }
            else if (prefix[0] == '>') { shapeOut = NodeShape::ASYMMETRIC; start = bestStart + 1; end = raw.rfind("]"); }
        }

        if (start != std::string::npos && end != std::string::npos && end > start) {
            idOut = raw.substr(0, bestStart);
            labelOut = raw.substr(start, end - start);
        } else {
            idOut = raw;
            labelOut = raw;
            shapeOut = NodeShape::ROUNDED_BOX; 
        }
        
        Trim(idOut);
        Trim(labelOut);
    }
}

bool MermaidParser::Parse(const std::string& content, PreviewGraphState& graph, SizeCalculator calculator, int baseId) {
    if (graph.lastContent == content && graph.initialized) {
        return false; 
    }

    graph.nodes.clear();
    graph.links.clear();
    graph.lastContent = content;
    graph.initialized = true;

    std::stringstream ss(content);
    std::string line;
    int nextId = baseId; 
    int linkId = baseId + 5000;
    
    std::unordered_map<std::string, int> idMap; 

    auto GetOrCreateNode = [&](const std::string& name, const std::string& label, NodeShape shape) {
        if (idMap.find(name) == idMap.end()) {
            GraphNode node;
            node.id = nextId++;
            node.title = label.empty() ? name : label;
            node.x = 0; node.y = 0;
            node.vx = node.vy = 0;
            node.type = NodeType::INSIGHT; 
            node.shape = shape;

            // Calculate size using provided calculator
            if (calculator) {
                auto res = calculator(node.title);
                node.w = res.width;
                node.h = res.height;
                node.wrapW = res.wrapWidth;
            }

            graph.nodes.push_back(node);
            idMap[name] = node.id;
        }
        return idMap[name];
    };

    std::vector<std::pair<int, int>> indentStack; 
    bool isMindMap = false;

    while (std::getline(ss, line)) {
        std::string trimmed = line;
        size_t firstChar = trimmed.find_first_not_of(" \t");
        if (firstChar == std::string::npos) continue;
        
        int indent = static_cast<int>(firstChar);
        trimmed = trimmed.substr(firstChar);

        if (StartsWith(trimmed, "mindmap")) { isMindMap = true; continue; }
        if (StartsWith(trimmed, "graph ") || StartsWith(trimmed, "flowchart ")) continue;
        if (StartsWith(trimmed, "%%")) continue;

        if (isMindMap) {
            std::string id, label;
            NodeShape shape;
            ParseNodeStr(trimmed, id, label, shape);
            if (id.empty()) id = label;
            int nodeId = GetOrCreateNode(id, label, shape);
            while (!indentStack.empty() && indentStack.back().first >= indent) indentStack.pop_back();
            if (!indentStack.empty()) {
                GraphLink link;
                link.id = linkId++;
                link.startNode = indentStack.back().second;
                link.endNode = nodeId;
                graph.links.push_back(link);
            }
            indentStack.push_back({indent, nodeId});
        } else {
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
                 std::string id, label;
                 NodeShape shape;
                 ParseNodeStr(trimmed, id, label, shape);
                 if (!id.empty()) GetOrCreateNode(id, label, shape);
            }
        }
    }

    // --- Layout Logic ---
    graph.childrenNodes.clear();
    graph.roots.clear();
    graph.nodeById.clear();
    std::unordered_map<int, int> inDegree;

    for (size_t i = 0; i < graph.nodes.size(); ++i) {
        graph.nodeById[graph.nodes[i].id] = static_cast<int>(i);
        inDegree[graph.nodes[i].id] = 0;
    }
    for (const auto& link : graph.links) {
        graph.childrenNodes[link.startNode].push_back(link.endNode);
        inDegree[link.endNode]++;
    }
    for (const auto& node : graph.nodes) {
        if (inDegree[node.id] == 0) graph.roots.push_back(node.id);
    }
    if (graph.roots.empty() && !graph.nodes.empty()) graph.roots.push_back(graph.nodes[0].id);

    // Heuristics
    std::unordered_map<int, int> countPerDepth;
    int maxDepth = 0;
    int maxBreadth = 0;
    std::function<void(int, int, std::unordered_set<int>&)> WalkTopology = [&](int u, int d, std::unordered_set<int>& visited) {
        visited.insert(u);
        countPerDepth[d]++;
        maxDepth = std::max(maxDepth, d);
        maxBreadth = std::max(maxBreadth, countPerDepth[d]);
        for (int v : graph.childrenNodes[u]) if (visited.find(v) == visited.end()) WalkTopology(v, d + 1, visited);
    };

    std::unordered_set<int> visitedTopology;
    for (int r : graph.roots) WalkTopology(r, 0, visitedTopology);
    graph.orientation = (maxDepth > maxBreadth) ? LayoutOrientation::TopDown : LayoutOrientation::LeftRight;

    // Breadth Calculation
    std::unordered_map<int, float> subtreeBreadth;
    const float SIBLING_GAP = 60.0f; 
    std::function<float(int, std::unordered_set<int>&)> CalcBreadth = [&](int u, std::unordered_set<int>& visited) -> float {
        visited.insert(u);
        float childrenB = 0;
        int childCount = 0;
        float myB = (graph.orientation == LayoutOrientation::TopDown) ? graph.nodes[graph.nodeById[u]].w : graph.nodes[graph.nodeById[u]].h;

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

    // Recursive Layout
    std::function<void(int, float, float, std::unordered_set<int>&)> LayoutRecursive = 
        [&](int u, float primary, float secondaryStart, std::unordered_set<int>& visited) {
        visited.insert(u);
        GraphNode& uNode = graph.nodes[graph.nodeById[u]];
        float totalB = subtreeBreadth[u];

        if (graph.orientation == LayoutOrientation::LeftRight) {
            uNode.x = primary;
            uNode.y = (secondaryStart + totalB * 0.5f) - (uNode.h * 0.5f);
            float hGap = std::clamp(uNode.w * 0.6f, 80.0f, 200.0f);
            float childX = primary + uNode.w + hGap;
            float childrenTotalB = 0; int childCount = 0;
            for(int v : graph.childrenNodes[u]) {
                if(visited.find(v) == visited.end()) {
                    if(childCount > 0) childrenTotalB += SIBLING_GAP;
                    childrenTotalB += subtreeBreadth[v]; childCount++;
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
            uNode.y = primary;
            uNode.x = (secondaryStart + totalB * 0.5f) - (uNode.w * 0.5f);
            float vGap = std::clamp(uNode.h * 0.6f, 60.0f, 160.0f);
            float childY = primary + uNode.h + vGap;
            float childrenTotalB = 0; int childCount = 0;
            for(int v : graph.childrenNodes[u]) {
                if(visited.find(v) == visited.end()) {
                    if(childCount > 0) childrenTotalB += SIBLING_GAP;
                    childrenTotalB += subtreeBreadth[v]; childCount++;
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
    float FOREST_GAP = 120.0f;
    float secondaryCursor = 50.0f;
    for (int root : graph.roots) {
         LayoutRecursive(root, 50.0f, secondaryCursor, visitedLayout);
         secondaryCursor += subtreeBreadth[root] + FOREST_GAP; 
    }

    return true;
}

} // namespace ideawalker::domain::writing
