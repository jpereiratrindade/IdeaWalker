/**
 * @file AsyncTaskManager.hpp
 * @brief Centralized management for background tasks.
 */

#pragma once

#include <string>
#include <vector>
#include <functional>
#include <future>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <memory>
#include <algorithm>

namespace ideawalker::application {

/**
 * @enum TaskType
 * @brief Categories of background work.
 */
enum class TaskType {
    AI_Processing,
    Indexing,
    Transcription,
    Export,
    UpdateCheck
};

/**
 * @struct TaskStatus
 * @brief Information about a running or completed task.
 */
struct TaskStatus {
    int id;
    TaskType type;
    std::string description;
    std::atomic<float> progress{0.0f};
    std::atomic<bool> isCompleted{false};
    std::atomic<bool> failed{false};
    std::string errorMessage;
};

/**
 * @class AsyncTaskManager
 * @brief Manages background execution and provides unified status tracking.
 */
class AsyncTaskManager {
public:
    AsyncTaskManager() = default;
    ~AsyncTaskManager() {
        // In a real implementation, we would wait for tasks or cancel them.
    }

    /** @brief Submits a new task to be executed in the background. */
    template<typename F, typename... Args>
    std::shared_ptr<TaskStatus> SubmitTask(TaskType type, const std::string& description, F&& f, Args&&... args) {
        auto status = std::make_shared<TaskStatus>();
        status->id = m_nextId++;
        status->type = type;
        status->description = description;

        {
            std::lock_guard<std::mutex> lock(m_tasksMutex);
            m_activeTasks.push_back(status);
        }

        // C++17 compatible thread launch
        std::thread([this, status](auto userFunc, auto... userArgs) {
            try {
                // Call the user function with status as first arg, followed by other args
                userFunc(status, std::move(userArgs)...);
                status->isCompleted = true;
                status->progress = 1.0f;
            } catch (const std::exception& e) {
                status->failed = true;
                status->errorMessage = e.what();
                status->isCompleted = true;
            } catch (...) {
                status->failed = true;
                status->errorMessage = "Unknown error during task execution.";
                status->isCompleted = true;
            }
            CleanupCompletedTasks();
        }, std::forward<F>(f), std::forward<Args>(args)...).detach();

        return status;
    }

    /** @brief Returns snapshots of all active tasks. */
    std::vector<std::shared_ptr<TaskStatus>> GetActiveTasks() {
        std::lock_guard<std::mutex> lock(m_tasksMutex);
        return m_activeTasks;
    }

private:
    void CleanupCompletedTasks() {
        std::lock_guard<std::mutex> lock(m_tasksMutex);
        m_activeTasks.erase(
            std::remove_if(m_activeTasks.begin(), m_activeTasks.end(), 
                [](const auto& s) { return s->isCompleted.load(); }),
            m_activeTasks.end()
        );
    }

    std::atomic<int> m_nextId{0};
    std::vector<std::shared_ptr<TaskStatus>> m_activeTasks;
    std::mutex m_tasksMutex;
};

} // namespace ideawalker::application
