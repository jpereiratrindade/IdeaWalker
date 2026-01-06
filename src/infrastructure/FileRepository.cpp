#include "infrastructure/FileRepository.hpp"
#include <filesystem>
#include <fstream>
#include <sstream>
#include <chrono>
#include <ctime>

namespace fs = std::filesystem;

namespace ideawalker::infrastructure {

namespace {

std::tm ToLocalTime(std::time_t tt) {
    std::tm tm = {};
#if defined(_WIN32)
    localtime_s(&tm, &tt);
#else
    localtime_r(&tt, &tm);
#endif
    return tm;
}

} // namespace

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

bool FileRepository::shouldProcess(const domain::RawThought& thought, const std::string& insightId) {
    fs::path inboxFile = fs::path(m_inboxPath) / thought.filename;
    fs::path noteFile = fs::path(m_notesPath) / ("Nota_" + insightId + ".md");
    if (!fs::exists(noteFile)) return true;
    if (!fs::exists(inboxFile)) return true;

    try {
        auto inboxTime = fs::last_write_time(inboxFile);
        auto noteTime = fs::last_write_time(noteFile);
        return inboxTime > noteTime;
    } catch (...) {
        return true;
    }
}

void FileRepository::saveInsight(const domain::Insight& insight) {
    std::string filename = "Nota_" + insight.getMetadata().id + ".md";
    fs::path outPath = fs::path(m_notesPath) / filename;
    
    std::ofstream file(outPath);
    file << insight.getContent();
}

void FileRepository::updateNote(const std::string& filename, const std::string& content) {
    fs::path outPath = fs::path(m_notesPath) / filename;
    std::ofstream file(outPath);
    file << content;
}

std::vector<domain::Insight> FileRepository::fetchHistory() {
    std::vector<domain::Insight> history;
    if (!fs::exists(m_notesPath)) return history;

    for (const auto& entry : fs::directory_iterator(m_notesPath)) {
        if (entry.is_regular_file() && entry.path().extension() == ".md") {
            std::ifstream file(entry.path());
            std::stringstream buffer;
            buffer << file.rdbuf();
            
            domain::Insight::Metadata meta;
            meta.id = entry.path().filename().string();
            history.emplace_back(meta, buffer.str()); 
        }
    }
    return history;
}

std::vector<std::string> FileRepository::getBacklinks(const std::string& filename) {
    std::vector<std::string> backlinks;
    if (!fs::exists(m_notesPath)) return backlinks;

    std::string target = filename;
    size_t lastDot = target.find_last_of(".");
    if (lastDot != std::string::npos) target = target.substr(0, lastDot);
    std::string searchStr = "[[" + target + "]]";

    for (const auto& entry : fs::directory_iterator(m_notesPath)) {
        if (entry.is_regular_file() && entry.path().extension() == ".md") {
            if (entry.path().filename().string() == filename) continue;

            std::ifstream file(entry.path());
            std::string content((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
            if (content.find(searchStr) != std::string::npos) {
                backlinks.push_back(entry.path().filename().string());
            }
        }
    }
    return backlinks;
}

std::map<std::string, int> FileRepository::getActivityHistory() {
    std::map<std::string, int> history;
    if (!fs::exists(m_notesPath)) return history;

    for (const auto& entry : fs::directory_iterator(m_notesPath)) {
        if (entry.is_regular_file() && entry.path().extension() == ".md") {
            try {
                auto ftime = fs::last_write_time(entry);
#if defined(__cpp_lib_chrono) && __cpp_lib_chrono >= 201907L
                auto sctp = std::chrono::clock_cast<std::chrono::system_clock>(ftime);
#else
                // Fallback for older C++ standards if needed
                auto sctp = std::chrono::time_point_cast<std::chrono::system_clock::duration>(ftime - fs::file_time_type::clock::now() + std::chrono::system_clock::now());
#endif
                std::time_t tt = std::chrono::system_clock::to_time_t(sctp);
                std::tm gmt = ToLocalTime(tt);
                char buffer[11];
                std::strftime(buffer, sizeof(buffer), "%Y-%m-%d", &gmt);
                history[std::string(buffer)]++;
            } catch (...) {}
        }
    }
    return history;
}

} // namespace ideawalker::infrastructure
