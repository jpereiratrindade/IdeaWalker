#pragma once
#include <string>
#include <vector>
#include <chrono>
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

private:
    Metadata m_metadata;
    std::string m_content;
    std::vector<Actionable> m_actionables;
};

} // namespace ideawalker::domain
