#include "application/KnowledgeExportService.hpp"
#include <sstream>
#include <ctime>

namespace ideawalker::application {

using namespace ideawalker::domain::writing;

std::string KnowledgeExportService::ToMermaidMindmap(const std::vector<GraphNode>& nodes, 
                                                    const std::vector<GraphLink>& links) {
    std::stringstream ss;
    ss << "# IdeaWalker Neural Web - Exportação\n\n";
    ss << "```mermaid\n";
    ss << "mindmap\n";
    ss << "  root((IdeaWalker Neural Web))\n";

    for (const auto& node : nodes) {
        if (node.type == NodeType::INSIGHT || node.type == NodeType::HYPOTHESIS) {
            ss << "    node_" << node.id << "[" << node.title << "]\n";
            
            for (const auto& link : links) {
                if (link.startNode == node.id) {
                    const auto& targetNode = nodes[link.endNode];
                    if (targetNode.type == NodeType::TASK) {
                        const char* emoji = "📋 ";
                        if (targetNode.isCompleted) emoji = "✅ ";
                        else if (targetNode.isInProgress) emoji = "⏳ ";

                        ss << "      " << (targetNode.isCompleted ? " ((" : " (")
                           << emoji << targetNode.title << (targetNode.isCompleted ? ")) " : ") ") << "\n";
                    }
                }
            }
        }
    }

    ss << "```\n";
    return ss.str();
}

std::string KnowledgeExportService::ToFullMarkdown(const std::vector<domain::Insight>& insights,
                                                  const std::vector<GraphNode>& nodes, 
                                                  const std::vector<GraphLink>& links) {
    std::stringstream ss;
    ss << "# IdeaWalker - Exportação da Base de Conhecimento\n";
    
    std::time_t now = std::time(nullptr);
    char dateBuf[64];
    std::strftime(dateBuf, sizeof(dateBuf), "%Y-%m-%d %H:%M:%S", std::localtime(&now));
    ss << "Data: " << dateBuf << "\n\n";

    ss << "## 🕸️ Neural Web (Fluxograma Mermaid)\n\n";
    ss << "```mermaid\n";
    ss << "graph TD\n";
    for (const auto& node : nodes) {
        if (node.type == NodeType::INSIGHT || node.type == NodeType::HYPOTHESIS) {
            ss << "  N" << node.id << "[" << node.title << "]\n";
        }
    }
    for (const auto& link : links) {
        if (link.startNode < (int)nodes.size() && link.endNode < (int)nodes.size()) {
            const auto& start = nodes[link.startNode];
            const auto& end = nodes[link.endNode];
            if ((start.type == NodeType::INSIGHT || start.type == NodeType::HYPOTHESIS) && 
                (end.type == NodeType::INSIGHT || end.type == NodeType::HYPOTHESIS)) {
                ss << "  N" << link.startNode << " --> N" << link.endNode << "\n";
            }
        }
    }
    ss << "```\n\n";

    ss << "## 🧠 Mapa Mental (Tarefas e Ideias)\n\n";
    ss << ToMermaidMindmap(nodes, links) << "\n\n";

    ss << "## 📝 Conteúdo dos Documentos\n\n";
    for (const auto& insight : insights) {
        ss << "### " << (insight.getMetadata().title.empty() ? insight.getMetadata().id : insight.getMetadata().title) << "\n";
        ss << "ID: `" << insight.getMetadata().id << "`\n\n";
        ss << insight.getContent() << "\n\n";
        ss << "---\n\n";
    }

    return ss.str();
}

} // namespace ideawalker::application
