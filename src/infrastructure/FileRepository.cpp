/**
 * @file FileRepository.cpp
 * @brief Implementation of the FileRepository class.
 */
#include "infrastructure/FileRepository.hpp"
#include <filesystem>
#include <fstream>
#include <sstream>
#include <chrono>
#include <ctime>
#include <algorithm>
#include "infrastructure/ContentExtractor.hpp"
#include <nlohmann/json.hpp>

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

FileRepository::FileRepository(const std::string& inboxPath, const std::string& notesPath, const std::string& historyPath)
    : m_inboxPath(inboxPath), m_notesPath(notesPath), m_historyPath(historyPath) {
    if (!fs::exists(m_inboxPath)) fs::create_directories(m_inboxPath);
    if (!fs::exists(m_notesPath)) fs::create_directories(m_notesPath);
    if (!fs::exists(m_historyPath)) fs::create_directories(m_historyPath);
}

std::vector<domain::RawThought> FileRepository::fetchInbox() {
    std::vector<domain::RawThought> thoughts;
    for (const auto& entry : fs::directory_iterator(m_inboxPath)) {
        if (entry.is_regular_file()) {
            std::string ext = entry.path().extension().string();
            std::transform(ext.begin(), ext.end(), ext.begin(), [](unsigned char c){ return std::tolower(c); });
            
            if (ext == ".txt" || ext == ".md" || ext == ".pdf" || ext == ".tex") {
                std::string content = ContentExtractor::Extract(entry.path().string());
                thoughts.push_back({entry.path().filename().string(), content});
            }
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
    
    // BACKUP LOGIC (Line of Life)
    if (fs::exists(outPath)) {
        try {
            auto now = std::chrono::system_clock::now();
            std::time_t tt = std::chrono::system_clock::to_time_t(now);
            std::tm tm = ToLocalTime(tt);
            char dateBuf[32];
            std::strftime(dateBuf, sizeof(dateBuf), "_%Y%m%d_%H%M%S.md", &tm);
            
            std::string baseName = filename;
            size_t dotPos = baseName.find_last_of('.');
            if (dotPos != std::string::npos) baseName = baseName.substr(0, dotPos);
            
            std::string backupName = baseName + std::string(dateBuf);
            fs::path backupPath = fs::path(m_historyPath) / backupName;
            
            fs::copy_file(outPath, backupPath, fs::copy_options::overwrite_existing);
        } catch (...) {
            // Ignore backup failures for now, don't block saving
        }
    }

    std::ofstream file(outPath);
    file << insight.getContent();
    logActivity();
}

void FileRepository::updateNote(const std::string& filename, const std::string& content) {
    fs::path outPath = fs::path(m_notesPath) / filename;
    if (fs::exists(outPath)) {
        try {
            auto now = std::chrono::system_clock::now();
            std::time_t tt = std::chrono::system_clock::to_time_t(now);
            std::tm tm = ToLocalTime(tt);
            char dateBuf[32];
            std::strftime(dateBuf, sizeof(dateBuf), "_%Y%m%d_%H%M%S.md", &tm);

            std::string baseName = filename;
            size_t dotPos = baseName.find_last_of('.');
            if (dotPos != std::string::npos) baseName = baseName.substr(0, dotPos);

            std::string backupName = baseName + std::string(dateBuf);
            fs::path backupPath = fs::path(m_historyPath) / backupName;
            fs::copy_file(outPath, backupPath, fs::copy_options::overwrite_existing);
        } catch (...) {
            // Ignore backup failures for now, don't block saving
        }
    }
    std::ofstream file(outPath);
    file << content;
    logActivity();
}

std::vector<domain::Insight> FileRepository::fetchHistory() {
    std::vector<domain::Insight> history;
    if (!fs::exists(m_notesPath)) return history;

    for (const auto& entry : fs::directory_iterator(m_notesPath)) {
        std::string ext = entry.path().extension().string();
        if (entry.is_regular_file() && (ext == ".md" || ext == ".txt")) {
            std::ifstream file(entry.path());
            std::stringstream buffer;
            buffer << file.rdbuf();
            
            domain::Insight::Metadata meta;
            meta.id = entry.path().filename().string();
            std::string content = buffer.str();
            
            // Try to extract title from "# Título: [Title]"
            std::string title;
            size_t titlePos = content.find("# Título:");
            if (titlePos != std::string::npos) {
                size_t start = titlePos + 10; // "# Título: " length
                size_t end = content.find("\n", start);
                if (end != std::string::npos) {
                    title = content.substr(start, end - start);
                    // Trim brackets if present: [Title]
                    if (!title.empty() && title.front() == '[') title.erase(0, 1);
                    if (!title.empty() && title.back() == ']') title.pop_back();
                    // Basic trim
                    title.erase(0, title.find_first_not_of(" \t"));
                    title.erase(title.find_last_not_of(" \t") + 1);
                }
            }
            meta.title = title;
            
            history.emplace_back(meta, content); 
        }
    }
    return history;
}

std::vector<std::string> FileRepository::getBacklinks(const std::string& filename) {
    std::vector<std::string> backlinks;
    if (!fs::exists(m_notesPath)) return backlinks;

    // 1. Get ID-based search string
    std::string idTarget = filename;
    size_t lastDot = idTarget.find_last_of(".");
    if (lastDot != std::string::npos) idTarget = idTarget.substr(0, lastDot);
    std::string searchId = "[[" + idTarget + "]]";

    // 2. Try to get Title-based search string from the file itself
    std::string searchTitle;
    {
        fs::path p = fs::path(m_notesPath) / filename;
        if (fs::exists(p)) {
            std::ifstream f(p);
            std::string line;
            while (std::getline(f, line)) {
                if (line.rfind("# Título: ", 0) == 0) {
                    searchTitle = "[[" + line.substr(10) + "]]";
                    // Trim trailing CR
                    if (!searchTitle.empty() && searchTitle.back() == '\r') searchTitle.pop_back();
                    break;
                }
            }
        }
    }

    // 3. Scan all notes
    for (const auto& entry : fs::directory_iterator(m_notesPath)) {
        std::string ext = entry.path().extension().string();
        if (entry.is_regular_file() && (ext == ".md" || ext == ".txt")) {
            if (entry.path().filename().string() == filename) continue;

            std::ifstream file(entry.path());
            std::string content((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
            
            bool found = (content.find(searchId) != std::string::npos);
            if (!found) {
                // Also try with the full filename (with extension) e.g. [[Note.md]]
                std::string searchFull = "[[" + filename + "]]";
                found = (content.find(searchFull) != std::string::npos);
            }
            if (!found && !searchTitle.empty()) {
                found = (content.find(searchTitle) != std::string::npos);
            }

            if (found) {
                backlinks.push_back(entry.path().filename().string());
            }
        }
    }
    return backlinks;
}

std::map<std::string, int> FileRepository::getActivityHistory() {
    std::map<std::string, int> history;
    
    // 1. Try to load from persistent log
    fs::path logPath = fs::path(m_notesPath).parent_path() / ".activity_log.json";
    if (fs::exists(logPath)) {
        try {
            std::ifstream file(logPath);
            nlohmann::json j;
            file >> j;
            for (auto it = j.begin(); it != j.end(); ++it) {
                history[it.key()] = it.value().get<int>();
            }
        } catch (...) {}
    }

    // 2. Fallback/Augment with current file times (ensures legacy/manual notes are counted)
    if (fs::exists(m_notesPath)) {
        for (const auto& entry : fs::directory_iterator(m_notesPath)) {
            std::string ext = entry.path().extension().string();
            if (entry.is_regular_file() && (ext == ".md" || ext == ".txt")) {
                try {
                    auto ftime = fs::last_write_time(entry);
#if defined(__cpp_lib_chrono) && __cpp_lib_chrono >= 201907L
                    auto sctp = std::chrono::clock_cast<std::chrono::system_clock>(ftime);
#else
                    auto sctp = std::chrono::time_point_cast<std::chrono::system_clock::duration>(ftime - fs::file_time_type::clock::now() + std::chrono::system_clock::now());
#endif
                    std::time_t tt = std::chrono::system_clock::to_time_t(sctp);
                    std::tm gmt = ToLocalTime(tt);
                    char buffer[11];
                    std::strftime(buffer, sizeof(buffer), "%Y-%m-%d", &gmt);
                    std::string dateKey(buffer);
                    if (history.find(dateKey) == history.end()) {
                        history[dateKey] = 1; // Basic count if not in log
                    }
                } catch (...) {}
            }
        }
    }
    return history;
}

void FileRepository::logActivity() {
    try {
        fs::path logPath = fs::path(m_notesPath).parent_path() / ".activity_log.json";
        nlohmann::json j;
        if (fs::exists(logPath)) {
            std::ifstream inFile(logPath);
            inFile >> j;
        }

        auto now = std::chrono::system_clock::now();
        std::time_t tt = std::chrono::system_clock::to_time_t(now);
        std::tm tm = ToLocalTime(tt);
        char buffer[11];
        std::strftime(buffer, sizeof(buffer), "%Y-%m-%d", &tm);
        std::string dateKey(buffer);

        if (j.contains(dateKey)) {
            j[dateKey] = j[dateKey].get<int>() + 1;
        } else {
            j[dateKey] = 1;
        }

        std::ofstream outFile(logPath);
        outFile << j.dump(4);
    } catch (...) {}
}



std::vector<std::string> FileRepository::getVersions(const std::string& noteId) {
    std::vector<std::string> versions;
    if (!fs::exists(m_historyPath)) return versions;

    std::string baseName = noteId;
    size_t dotPos = baseName.find_last_of('.');
    if (dotPos != std::string::npos) baseName = baseName.substr(0, dotPos);

    std::string prefix = "Nota_" + baseName + "_";
    if (baseName.rfind("Nota_", 0) == 0) {
        prefix = baseName + "_";
    }

    for (const auto& entry : fs::directory_iterator(m_historyPath)) {
        if (entry.is_regular_file()) {
            std::string fname = entry.path().filename().string();
            if (fname.rfind(prefix, 0) == 0) { // Starts with prefix
                versions.push_back(fname);
            }
        }
    }
    // Sort descending (newest first)
    std::sort(versions.rbegin(), versions.rend());
    return versions;
}

std::string FileRepository::getVersionContent(const std::string& versionFilename) {
    fs::path p = fs::path(m_historyPath) / versionFilename;
    if (!fs::exists(p)) return "";
    
    std::ifstream file(p);
    std::stringstream buffer;
    buffer << file.rdbuf();
    return buffer.str();
}

std::string FileRepository::getNoteContent(const std::string& filename) {
    fs::path p = fs::path(m_notesPath) / filename;
    if (!fs::exists(p)) return "";
    
    std::ifstream file(p);
    std::stringstream buffer;
    buffer << file.rdbuf();
    return buffer.str();
}

} // namespace ideawalker::infrastructure
