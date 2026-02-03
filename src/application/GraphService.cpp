#include "application/GraphService.hpp"
#include <cmath>
#include <algorithm>
#include <unordered_map>
#include <unordered_set>

namespace ideawalker::application {

using namespace ideawalker::domain::writing;

void GraphService::RebuildGraph(const std::vector<domain::Insight>& insights, 
                                bool showTasks,
                                std::vector<GraphNode>& nodes, 
                                std::vector<GraphLink>& links) {
    nodes.clear();
    links.clear();

    if (insights.empty()) return;

    std::unordered_map<std::string, int> nameToId;
    int nextId = 0;
    int linkIdCounter = 0;

    // 1. Create Nodes and internal Task Links
    for (const auto& insight : insights) {
        GraphNode node;
        node.id = nextId++;
        node.type = NodeType::INSIGHT;
        node.title = insight.getMetadata().title.empty() ? insight.getMetadata().id : insight.getMetadata().title;
        
        // Detect Hypothesis Nodes
        for (const auto& tag : insight.getMetadata().tags) {
            std::string t = tag;
            std::transform(t.begin(), t.end(), t.begin(), ::tolower);
            if (t.find("hypothe") != std::string::npos || t.find("hipote") != std::string::npos) {
                node.type = NodeType::HYPOTHESIS;
                break;
            }
        }
        
        float angle = (float(node.id) / insights.size()) * 2.0f * 3.14159f;
        float radius = 100.0f + (rand() % 200); 
        node.x = 800.0f + std::cos(angle) * radius;
        node.y = 450.0f + std::sin(angle) * radius;
        node.vx = node.vy = 0.0f;

        nodes.push_back(node);
        int parentId = node.id;
        
        nameToId[insight.getMetadata().id] = node.id;
        if (!insight.getMetadata().title.empty()) {
            nameToId[insight.getMetadata().title] = node.id;
        }

        // Add Tasks as sub-nodes if enabled
        if (showTasks) {
            for (const auto& task : insight.getActionables()) {
                GraphNode taskNode;
                taskNode.id = nextId++;
                taskNode.type = NodeType::TASK;
                taskNode.title = task.description;
                taskNode.isCompleted = task.isCompleted;
                taskNode.isInProgress = task.isInProgress;

                float tAngle = (float(rand()) / RAND_MAX) * 2.0f * 3.14159f;
                taskNode.x = node.x + std::cos(tAngle) * 50.0f;
                taskNode.y = node.y + std::sin(tAngle) * 50.0f;
                taskNode.vx = taskNode.vy = 0.0f;

                nodes.push_back(taskNode);
                links.push_back({linkIdCounter++, parentId, taskNode.id});
            }
        }
    }

    // 2. Create Cross-Note Links
    std::vector<std::pair<std::string, int>> searchableTitles;
    for (const auto& insight : insights) {
        std::string title = insight.getMetadata().title;
        if (title.length() >= 4) {
            searchableTitles.push_back({title, nameToId[title]});
        }
    }

    for (const auto& insight : insights) {
        std::string content = insight.getContent();
        std::string sourceName = insight.getMetadata().id;
        int sourceId = nameToId[sourceName];

        // Explicit [[Wikilinks]]
        const_cast<domain::Insight&>(insight).parseReferencesFromContent();
        
        std::unordered_set<int> linkedNodes;
        for (const auto& ref : insight.getReferences()) {
            std::string targetName = ref;
            targetName.erase(0, targetName.find_first_not_of(" \t\n\r"));
            size_t end = targetName.find_last_not_of(" \t\n\r");
            if (end != std::string::npos) targetName.erase(end + 1);
            
            if (targetName.empty()) continue;

            int targetId = -1;
            auto it = nameToId.find(targetName);
            if (it == nameToId.end()) it = nameToId.find(targetName + ".md");

            if (it != nameToId.end()) {
                targetId = it->second;
            } else {
                // Ghost Node Creation
                GraphNode conceptNode;
                conceptNode.id = nextId++;
                conceptNode.type = NodeType::CONCEPT;
                conceptNode.title = targetName;
                
                float angle = (float(rand()) / RAND_MAX) * 2.0f * 3.14159f;
                conceptNode.x = nodes[sourceId].x + std::cos(angle) * 150.0f;
                conceptNode.y = nodes[sourceId].y + std::sin(angle) * 150.0f;
                conceptNode.vx = conceptNode.vy = 0.0f;

                nodes.push_back(conceptNode);
                targetId = conceptNode.id;
                nameToId[targetName] = targetId;
            }

            if (targetId != sourceId && targetId != -1) {
                if (linkedNodes.find(targetId) == linkedNodes.end()) {
                    links.push_back({linkIdCounter++, sourceId, targetId});
                    linkedNodes.insert(targetId);
                }
            }
        }

        // Implicit Semantic Links
        for (const auto& [title, targetId] : searchableTitles) {
            if (targetId == sourceId) continue;
            if (linkedNodes.find(targetId) != linkedNodes.end()) continue;

            if (content.find(title) != std::string::npos) {
                links.push_back({linkIdCounter++, sourceId, targetId});
                linkedNodes.insert(targetId);
            }
        }
    }
}

void GraphService::UpdatePhysics(std::vector<GraphNode>& nodes, 
                                 const std::vector<GraphLink>& links,
                                 const std::unordered_set<int>& selectedNodes) {
    const float repulsionInsight = 1000.0f; 
    const float repulsionTask = 200.0f;
    const float springLengthInsight = 300.0f;
    const float springLengthTask = 80.0f;
    const float springK = 0.06f;
    const float damping = 0.60f;
    const float centerX = 800.0f;
    const float centerY = 450.0f;

    std::vector<std::pair<float, float>> forces(nodes.size(), {0.0f, 0.0f});

    // 1. Repulsion
    for (size_t i = 0; i < nodes.size(); ++i) {
        for (size_t j = i + 1; j < nodes.size(); ++j) {
            float dx = nodes[i].x - nodes[j].x;
            float dy = nodes[i].y - nodes[j].y;
            float distSq = dx*dx + dy*dy;
            if (distSq < 400.0f) distSq = 400.0f; 

            float dist = std::sqrt(distSq);
            float r = (nodes[i].type == NodeType::INSIGHT && nodes[j].type == NodeType::INSIGHT) 
                      ? repulsionInsight : repulsionTask;
            
            float force = r / dist; 
            float fx = (dx / dist) * force;
            float fy = (dy / dist) * force;

            forces[i].first += fx;
            forces[i].second += fy;
            forces[j].first -= fx;
            forces[j].second -= fy;
        }
    }

    // 2. Attraction
    for (const auto& link : links) {
        if (link.startNode >= (int)nodes.size() || link.endNode >= (int)nodes.size()) continue;

        GraphNode& n1 = nodes[link.startNode];
        GraphNode& n2 = nodes[link.endNode];

        float dx = n2.x - n1.x;
        float dy = n2.y - n1.y;
        float dist = std::sqrt(dx*dx + dy*dy);
        if (dist < 1.0f) dist = 1.0f;

        float targetLen = (n1.type == NodeType::TASK || n2.type == NodeType::TASK) 
                          ? springLengthTask : springLengthInsight;
        
        float force = (dist - targetLen) * springK;
        float fx = (dx / dist) * force;
        float fy = (dy / dist) * force;

        forces[link.startNode].first += fx;
        forces[link.startNode].second += fy;
        forces[link.endNode].first -= fx;
        forces[link.endNode].second -= fy;
    }

    // 3. Center Gravity
    for (size_t i = 0; i < nodes.size(); ++i) {
        if (nodes[i].type == NodeType::TASK) continue;

        float dx = centerX - nodes[i].x;
        float dy = centerY - nodes[i].y;
        float dist = std::sqrt(dx*dx + dy*dy);
        
        if (dist > 10.0f) {
            float strength = 0.02f; 
            forces[i].first += (dx / dist) * dist * strength; 
            forces[i].second += (dy / dist) * dist * strength;
        }
    }

    // 4. Update Positions
    for (size_t i = 0; i < nodes.size(); ++i) {
        auto& node = nodes[i];
        
        node.vx = (node.vx + forces[i].first) * damping;
        node.vy = (node.vy + forces[i].second) * damping;

        float v = std::sqrt(node.vx*node.vx + node.vy*node.vy);
        const float maxV = 8.0f;
        if (v > maxV) {
            node.vx = (node.vx / v) * maxV;
            node.vy = (node.vy / v) * maxV;
        }

        if (selectedNodes.find(node.id) != selectedNodes.end()) {
            node.vx = 0;
            node.vy = 0;
            continue; 
        }

        node.x += node.vx;
        node.y += node.vy;

        // 5. Bounds Clamping
        if (node.x < 50.0f) { node.x = 50.0f; node.vx *= -0.5f; }
        if (node.x > 1550.0f) { node.x = 1550.0f; node.vx *= -0.5f; }
        if (node.y < 50.0f) { node.y = 50.0f; node.vy *= -0.5f; }
        if (node.y > 850.0f) { node.y = 850.0f; node.vy *= -0.5f; }
    }
}

void GraphService::CenterGraph(std::vector<GraphNode>& nodes) {
    for (auto& node : nodes) {
        node.x = 800.0f + (rand() % 100 - 50);
        node.y = 450.0f + (rand() % 100 - 50);
        node.vx = 0;
        node.vy = 0;
    }
}

} // namespace ideawalker::application
