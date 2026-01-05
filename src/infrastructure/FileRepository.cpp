#include "infrastructure/FileRepository.hpp"
#include <filesystem>
#include <fstream>
#include <sstream>

namespace fs = std::filesystem;

namespace ideawalker::infrastructure {

FileRepository::FileRepository(const std::string& inboxPath, const std::string& notesPath)
    : m_inboxPath(inboxPath), m_notesPath(notesPath) {
    if (!fs::exists(m_inboxPath)) fs::create_directories(m_inboxPath);
    if (!fs::exists(m_notesPath)) fs::create_directories(m_notesPath);
}

std::vector<domain::RawThought> FileRepository::fetchInbox() {
    std::vector<domain::RawThought> thoughts;
    for (const auto& entry : fs::directory_iterator(m_inboxPath)) {
        if (entry.is_regular_file() && entry.path().extension() == ".txt") {
            std::ifstream file(entry.path());
            std::stringstream buffer;
            buffer << file.rdbuf();
            thoughts.push_back({entry.path().filename().string(), buffer.str()});
        }
    }
    return thoughts;
}

void FileRepository::saveInsight(const domain::Insight& insight) {
    std::string filename = "Nota_" + insight.getMetadata().id + ".md";
    fs::path outPath = fs::path(m_notesPath) / filename;
    
    std::ofstream file(outPath);
    file << insight.getContent();
}

std::vector<domain::Insight> FileRepository::fetchHistory() {
    // Basic implementation: for now we just list files
    // Full implementation would parse the saved markdown files back to Insight objects
    return {}; 
}

} // namespace ideawalker::infrastructure
