#include "ui/UiMarkdownRenderer.hpp"
#include "domain/writing/MermaidParser.hpp"
#include "imgui.h"
#include "imnodes.h"
#include <algorithm>
#include <cmath>
#include <cfloat>
#include <cstring>
#include <sstream>
#include <vector>

namespace ideawalker::ui {

namespace {
    constexpr float NODE_MAX_WIDTH = 250.0f;

bool StartsWith(const std::string& text, const char* prefix) {
    return text.rfind(prefix, 0) == 0;
}

} // namespace

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

void DrawStaticMermaidPreview(const ideawalker::domain::writing::PreviewGraphState& graph) {
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
    const float scale = 1.0f; 

    // Define the Content Area for the parent scrollable child
    ImVec2 canvasSize(std::floor(graphW * scale + padding * 2), std::floor(graphH * scale + padding * 2));
    ImVec2 startPos = ImGui::GetCursorScreenPos();
    
    float offX = std::floor(std::max(0.0f, (avail.x - canvasSize.x) * 0.5f));
    float offY = std::floor(std::max(0.0f, (avail.y - canvasSize.y) * 0.5f));
    
    ImVec2 offset(std::floor(startPos.x + padding + offX - minX),
                  std::floor(startPos.y + padding + offY - minY));
                       
    ImGui::Dummy(canvasSize);

    // Draw Links
    ImU32 linkColor = ImGui::GetColorU32(ImVec4(0.45f, 0.65f, 0.95f, 0.65f));
    using domain::writing::LayoutOrientation;
    for (const auto& link : graph.links) {
        const ideawalker::domain::writing::GraphNode* start = nullptr;
        const ideawalker::domain::writing::GraphNode* end = nullptr;
        
        auto itA = graph.nodeById.find(link.startNode);
        auto itB = graph.nodeById.find(link.endNode);
        
        if (itA != graph.nodeById.end()) start = &graph.nodes[itA->second];
        if (itB != graph.nodeById.end()) end = &graph.nodes[itB->second];
        if (!start || !end) continue;
        
        ImVec2 p1(offset.x + start->x * scale, offset.y + start->y * scale);
        ImVec2 p2(offset.x + end->x * scale, offset.y + end->y * scale);
        
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
    using domain::writing::NodeShape;

    for (const auto& node : graph.nodes) {
        ImVec2 center(offset.x + node.x * scale, offset.y + node.y * scale);
        float w = node.w * scale;
        float h = node.h * scale;
        
        ImVec2 min(center.x - w * 0.5f, center.y - h * 0.5f);
        ImVec2 max(center.x + w * 0.5f, center.y + h * 0.5f);

        float dx = node.x - centerX;
        float dy = node.y - centerY;
        float angle = std::atan2(dy, dx); 
        float hue = (angle + 3.14159f) / (2.0f * 3.14159f);
        float r, g, b;
        ImGui::ColorConvertHSVtoRGB(hue, 0.6f, 0.3f, r, g, b); 
        ImU32 nodeBg = ImGui::GetColorU32(ImVec4(r, g, b, 1.0f));
        ImGui::ColorConvertHSVtoRGB(hue, 0.6f, 0.5f, r, g, b); 
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
                    float indent = 10.0f;
                    drawList->AddLine(ImVec2(min.x + indent, min.y), ImVec2(min.x + indent, max.y), nodeBorder);
                    drawList->AddLine(ImVec2(max.x - indent, min.y), ImVec2(max.x - indent, max.y), nodeBorder);
                }
                break;
            case NodeShape::CYLINDER:
                {
                    float rx = (max.x - min.x) * 0.5f;
                    float ry = 5.0f;
                    ImVec2 topCenter(center.x, min.y + ry);
                    ImVec2 bottomCenter(center.x, max.y - ry);
                    drawList->AddRectFilled(ImVec2(min.x, min.y + ry), ImVec2(max.x, max.y - ry), nodeBg);
                    drawList->AddEllipseFilled(topCenter, ImVec2(rx, ry), nodeBg);
                    drawList->AddEllipse(topCenter, ImVec2(rx, ry), nodeBorder);
                    drawList->AddEllipseFilled(bottomCenter, ImVec2(rx, ry), nodeBg);
                    drawList->AddEllipse(bottomCenter, ImVec2(rx, ry), nodeBorder);
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
                    p[2] = ImVec2(max.x + indent, center.y); 
                    p[3] = ImVec2(max.x - indent, max.y);
                    p[4] = ImVec2(min.x + indent, max.y);
                    p[5] = ImVec2(min.x - indent, center.y); 
                    drawList->AddConvexPolyFilled(p, 6, nodeBg);
                    drawList->AddPolyline(p, 6, nodeBorder, ImDrawFlags_Closed, 1.0f);
                }
                break;
            case NodeShape::RHOMBUS:
                {
                    ImVec2 p[4];
                    p[0] = ImVec2(center.x, min.y - 5.0f);
                    p[1] = ImVec2(max.x + 10.0f, center.y);
                    p[2] = ImVec2(center.x, max.y + 5.0f);
                    p[3] = ImVec2(min.x - 10.0f, center.y);
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
                    p[2] = ImVec2(max.x, center.y);
                    p[3] = ImVec2(max.x - indent, max.y);
                    p[4] = ImVec2(min.x, max.y);
                    drawList->AddConvexPolyFilled(p, 5, nodeBg);
                    drawList->AddPolyline(p, 5, nodeBorder, ImDrawFlags_Closed, 1.0f);
                }
                break;
            default:
                 drawList->AddRectFilled(min, max, nodeBg, 8.0f);
                 drawList->AddRect(min, max, nodeBorder, 8.0f);
                 break;
        }
        
        ImVec2 textSize = ImGui::CalcTextSize(node.title.c_str(), nullptr, false, node.wrapW);
        ImVec2 textPos(std::floor(center.x - textSize.x * 0.5f), std::floor(center.y - textSize.y * 0.5f));
        drawList->AddText(ImGui::GetFont(), ImGui::GetFontSize(), textPos, textColor, node.title.c_str(), nullptr, node.wrapW);
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

    while (std::getline(ss, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();

        if (StartsWith(line, "```")) {
            if (inCodeBlock) {
                int blockId = codeBlockIdCounter++;
                ImGui::PushID(blockId);
                if (codeBlockLanguage == "mermaid") {
                    int mermaidBlockId = static_cast<int>(std::hash<std::string>{}(codeBlockContent)) % 10000;
                    auto& graph = app.previewGraphs[mermaidBlockId];
                    
                    bool newLayout = ideawalker::domain::writing::MermaidParser::Parse(
                        codeBlockContent, 
                        graph,
                        [](const std::string& text) {
                            NodeSizeResult res = EstimateNodeSizeAdaptive(text, 160.0f, 420.0f, 40.0f, 30.0f, 20.0f);
                            return ideawalker::domain::writing::MermaidParser::NodeSize{res.w, res.h, res.wrap};
                        },
                        10000 + (mermaidBlockId * 10)
                    );

                    if (newLayout && !staticMermaidPreview) {
                        ImNodes::EditorContextSet((ImNodesEditorContext*)app.previewGraphContext);
                        for (const auto& node : graph.nodes) {
                            ImNodes::SetNodeGridSpacePos(node.id, ImVec2(node.x, node.y));
                        }
                    }

                    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.12f, 0.14f, 0.18f, 1.0f));

                    if (staticMermaidPreview) {
                        ImGui::BeginChild("##mermaid_graph", ImVec2(0, 700), true);
                        DrawStaticMermaidPreview(graph);
                        ImGui::EndChild();
                        ImGui::PopStyleColor();
                    } else {
                        if (ImGui::Button("Fit to Screen") || newLayout) {
                            ImNodes::EditorContextSet((ImNodesEditorContext*)app.previewGraphContext);
                            ImVec2 min(FLT_MAX, FLT_MAX), max(-FLT_MAX, -FLT_MAX);
                            if (graph.nodes.empty()) { min = max = ImVec2(0,0); }
                            else {
                                for(auto& n : graph.nodes) {
                                    if(n.x < min.x) min.x = n.x; if(n.y < min.y) min.y = n.y;
                                    if(n.x > max.x) max.x = n.x; if(n.y > max.y) max.y = n.y;
                                }
                            }
                            ImVec2 center = ImVec2((min.x + max.x)*0.5f, (min.y + max.y)*0.5f);
                            ImVec2 canvasSize(ImGui::GetContentRegionAvail().x, 400.0f);
                            ImVec2 pan = ImVec2(canvasSize.x * 0.5f - center.x, canvasSize.y * 0.5f - center.y);
                            ImNodes::EditorContextResetPanning(pan);
                        }
                        
                        ImGui::BeginChild("##mermaid_graph", ImVec2(0, 700), true);
                        ImNodes::EditorContextSet((ImNodesEditorContext*)app.previewGraphContext);
                        ImNodes::PushColorStyle(ImNodesCol_GridBackground, ImGui::GetColorU32(ImVec4(0.12f, 0.14f, 0.18f, 1.0f)));
                        ImNodes::PushColorStyle(ImNodesCol_GridLine, ImGui::GetColorU32(ImVec4(0.2f, 0.2f, 0.2f, 0.5f)));
                        ImNodes::BeginNodeEditor();

                        for (auto& node : graph.nodes) {
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

                            ImNodes::BeginNode(node.id);
                            ImNodes::BeginNodeTitleBar();
                            ImGui::PushTextWrapPos(ImGui::GetCursorPos().x + NODE_MAX_WIDTH);
                            ImGui::TextUnformatted(node.title.c_str());
                            ImGui::PopTextWrapPos();
                            ImNodes::EndNodeTitleBar();
                            ImNodes::BeginOutputAttribute(node.id << 8); ImNodes::EndOutputAttribute();
                            ImNodes::BeginInputAttribute((node.id << 8) + 1); ImNodes::EndInputAttribute();
                            ImNodes::EndNode();
                            ImNodes::PopColorStyle(); ImNodes::PopColorStyle(); ImNodes::PopColorStyle();
                        }
                        
                        for (const auto& link : graph.links) {
                            ImNodes::Link(link.id, link.startNode << 8, (link.endNode << 8) + 1);
                        }
                        ImNodes::EndNodeEditor();
                        ImNodes::PopColorStyle(); ImNodes::PopColorStyle();
                        ImGui::EndChild();
                        ImGui::PopStyleColor();
                    }
                } else {
                    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.1f, 0.1f, 0.1f, 1.0f));
                    ImGui::BeginChild("##code", ImVec2(0, 0), true, ImGuiWindowFlags_AlwaysAutoResize);
                    if (!codeBlockLanguage.empty()) {
                         ImGui::TextDisabled("%s", codeBlockLanguage.c_str()); ImGui::Separator();
                    }
                    ImGui::PushFont(ImGui::GetIO().Fonts->Fonts[0]); 
                    ImGui::TextUnformatted(codeBlockContent.c_str());
                    ImGui::PopFont();
                    ImGui::EndChild();
                    ImGui::PopStyleColor();
                }
                ImGui::PopID();
                inCodeBlock = false; codeBlockContent = "";
            } else {
                inCodeBlock = true;
                if (line.length() > 3) {
                    codeBlockLanguage = line.substr(3);
                    auto start = codeBlockLanguage.find_first_not_of(" \t");
                    if (start != std::string::npos) codeBlockLanguage = codeBlockLanguage.substr(start);
                    auto end = codeBlockLanguage.find_last_not_of(" \t");
                    if (end != std::string::npos) codeBlockLanguage = codeBlockLanguage.substr(0, end + 1);
                } else { codeBlockLanguage = ""; }
            }
            continue;
        }

        if (inCodeBlock) {
            codeBlockContent += line + "\n";
            continue;
        }

        size_t firstChar = line.find_first_not_of(" \t");
        if (firstChar == std::string::npos) { ImGui::Spacing(); continue; }
        std::string indent = line.substr(0, firstChar);
        std::string trimmed = line.substr(firstChar);
        
        if (StartsWith(trimmed, "# ")) {
            ImGui::TextColored(ImVec4(0.4f, 0.7f, 1.0f, 1.0f), "%s", trimmed.substr(2).c_str()); ImGui::Separator();
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
            if (!indent.empty()) { ImGui::TextUnformatted(indent.c_str()); ImGui::SameLine(0, 0); }
            size_t lastPos = 0;
            size_t startPos = trimmed.find("[[");
            while (startPos != std::string::npos) {
                if (startPos > lastPos) {
                    ImGui::TextWrapped("%s", trimmed.substr(lastPos, startPos - lastPos).c_str()); ImGui::SameLine(0, 0);
                }
                size_t endPos = trimmed.find("]]", startPos + 2);
                if (endPos != std::string::npos) {
                    std::string linkName = trimmed.substr(startPos + 2, endPos - startPos - 2);
                    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.3f, 0.5f, 1.0f));
                    if (ImGui::SmallButton(linkName.c_str())) { app.selectedFilename = linkName + ".md"; }
                    ImGui::PopStyleColor(); ImGui::SameLine(0, 0);
                    lastPos = endPos + 2; startPos = trimmed.find("[[", lastPos);
                } else { break; }
            }
            if (lastPos < trimmed.size()) { ImGui::TextWrapped("%s", trimmed.substr(lastPos).c_str()); }
        }
    }
}

} // namespace ideawalker::ui
