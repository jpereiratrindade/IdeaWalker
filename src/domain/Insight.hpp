#pragma once
#include <string>
#include <vector>
#include <chrono>
#include <sstream>
#include "Actionable.hpp"

namespace ideawalker::domain {

class Insight {
public:
    struct Metadata {
        std::string id;
        std::string title;
        std::string date;
        std::vector<std::string> tags;
    };

    Insight(const Metadata& meta, const std::string& content)
        : m_metadata(meta), m_content(content) {}

    const Metadata& getMetadata() const { return m_metadata; }
    const std::string& getContent() const { return m_content; }
    const std::vector<Actionable>& getActionables() const { return m_actionables; }

    void addActionable(const Actionable& actionable) {
        m_actionables.push_back(actionable);
    }

    void parseActionablesFromContent() {
        m_actionables.clear();
        std::stringstream ss(m_content);
        std::string line;
        while (std::getline(ss, line)) {
            size_t pos = line.find("- [ ]");
            if (pos != std::string::npos) {
                std::string desc = line.substr(pos + 5);
                // Trim leading spaces
                desc.erase(0, desc.find_first_not_of(" \t"));
                if (!desc.empty()) {
                    m_actionables.emplace_back(desc);
                }
            }
        }
    }

    void setContent(const std::string& content) { m_content = content; }

    void toggleActionable(size_t index) {
        if (index >= m_actionables.size()) return;

        std::stringstream ss(m_content);
        std::stringstream out;
        std::string line;
        size_t currentIdx = 0;
        
        while (std::getline(ss, line)) {
            size_t pos = line.find("- [");
            if (pos != std::string::npos && (line.find("- [ ]") != std::string::npos || line.find("- [x]") != std::string::npos)) {
                if (currentIdx == index) {
                    if (line.find("- [ ]") != std::string::npos) {
                        line.replace(line.find("- [ ]"), 5, "- [x]");
                        m_actionables[index].isCompleted = true;
                    } else if (line.find("- [x]") != std::string::npos) {
                        line.replace(line.find("- [x]"), 5, "- [ ]");
                        m_actionables[index].isCompleted = false;
                    }
                }
                currentIdx++;
            }
            out << line << "\n";
        }
        m_content = out.str();
    }

private:
    Metadata m_metadata;
    std::string m_content;
    std::vector<Actionable> m_actionables;
};

} // namespace ideawalker::domain
