#pragma once

#include "ui/AppState.hpp"
#include <string>

namespace ideawalker::ui {

/**
 * @struct NodeSizeResult
 * @brief Simple container for adaptive node dimensions and wrap width.
 */
struct NodeSizeResult { float w, h, wrap; };

/**
 * @brief Heuristic to find a balanced node size (width vs height).
 */
NodeSizeResult EstimateNodeSizeAdaptive(const std::string& text, float minWrap, float maxWrap, float step, float padX, float padY);

/**
 * @brief Renders a Markdown preview in an ImGui context.
 */
void DrawMarkdownPreview(AppState& app, const std::string& content, bool staticMermaidPreview);

/**
 * @brief Draws a static preview of a Mermaid graph.
 */
void DrawStaticMermaidPreview(const ideawalker::domain::writing::PreviewGraphState& graph);

} // namespace ideawalker::ui
