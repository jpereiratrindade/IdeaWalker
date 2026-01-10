/**
 * @file PersistenceService.hpp
 * @brief Centralized service for serialized, atomic file I/O operations.
 */

#pragma once
#include <string>
#include <vector>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <thread>
#include <atomic>
#include <optional>
#include <functional>

namespace ideawalker::infrastructure {

/**
 * @struct SaveTask
 * @brief Represents a single file write operation.
 */
struct SaveTask {
    std::string filename;
    std::string content;
    bool isUrgent = false; // Could be used to prioritize (not implemented yet)
};

/**
 * @class PersistenceService
 * @brief Manages a background thread that performs atomic file writes sequentially.
 * 
 * This service eliminates race conditions on file writing by ensuring that
 * all write operations pass through a single serialized queue.
 */
class PersistenceService {
public:
    PersistenceService();
    ~PersistenceService();

    /**
     * @brief Asynchronously queues a text content to be saved to a file.
     * @param filename Absolute path to the file.
     * @param content The string content to write.
     */
    void saveTextAsync(const std::string& filename, const std::string& content);

    /**
     * @brief Stops the worker thread and ensures all pending tasks are processed.
     */
    void stop();

private:
    /**
     * @brief The main loop running in the background thread.
     */
    void workerLoop();

    /**
     * @brief Performs the actual atomic write (temp -> rename).
     */
    void performAtomicWrite(const SaveTask& task);

    // Thread Safety
    std::queue<SaveTask> m_queue;
    std::mutex m_mutex;
    std::condition_variable m_cv;
    
    // Worker Control
    std::thread m_worker;
    std::atomic<bool> m_running;
};

} // namespace ideawalker::infrastructure
