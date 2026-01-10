/**
 * @file PersistenceService.cpp
 * @brief Implementation of PersistenceService.
 */

#include "infrastructure/PersistenceService.hpp"
#include <filesystem>
#include <fstream>
#include <iostream>

namespace ideawalker::infrastructure {

namespace fs = std::filesystem;

PersistenceService::PersistenceService() : m_running(true) {
    m_worker = std::thread(&PersistenceService::workerLoop, this);
}

PersistenceService::~PersistenceService() {
    stop();
}

void PersistenceService::stop() {
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (!m_running) return;
        m_running = false;
    }
    m_cv.notify_all();
    
    if (m_worker.joinable()) {
        m_worker.join();
    }
}

void PersistenceService::saveTextAsync(const std::string& filename, const std::string& content) {
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_queue.push(SaveTask{filename, content});
    }
    m_cv.notify_one();
}

void PersistenceService::workerLoop() {
    while (true) {
        SaveTask task;
        
        {
            std::unique_lock<std::mutex> lock(m_mutex);
            m_cv.wait(lock, [this] {
                return !m_queue.empty() || !m_running;
            });

            if (!m_running && m_queue.empty()) {
                return; // Exit point
            }

            if (m_queue.empty()) {
                continue; // Spurious wake up
            }

            task = m_queue.front();
            m_queue.pop();
        }

        // Process outside lock
        performAtomicWrite(task);
    }
}

void PersistenceService::performAtomicWrite(const SaveTask& task) {
    fs::path finalPath = task.filename;
    
    // Create unique temp path: filename.<timestamp>.tmp
    // We use a simplified timestamp here just to be unique per operation
    auto timestamp = std::chrono::steady_clock::now().time_since_epoch().count();
    fs::path tempPath = finalPath;
    tempPath += "." + std::to_string(timestamp) + ".tmp";

    // 1. Ensure directory exists
    try {
        if (finalPath.has_parent_path() && !fs::exists(finalPath.parent_path())) {
            fs::create_directories(finalPath.parent_path());
        }
    } catch (const std::exception& e) {
        std::cerr << "[PersistenceService] Error creating directories: " << e.what() << std::endl;
        return;
    }

    // 2. Write to Temp
    {
        std::ofstream ofs(tempPath);
        if (!ofs.is_open()) {
            std::cerr << "[PersistenceService] Failed to open temp file: " << tempPath << std::endl;
            return;
        }
        ofs << task.content;
        if (ofs.fail()) {
            std::cerr << "[PersistenceService] Write failed during output: " << tempPath << std::endl;
            try { fs::remove(tempPath); } catch (...) {}
            return;
        }
        ofs.flush();
    } // Close happens here automatically

    // 3. Atomic Rename
    try {
        fs::rename(tempPath, finalPath);
    } catch (const std::filesystem::filesystem_error& e) {
        std::cerr << "[PersistenceService] Rename failed: " << e.what() << std::endl;
        // Attempt cleanup
        try { fs::remove(tempPath); } catch (...) {}
    }
}

} // namespace ideawalker::infrastructure
