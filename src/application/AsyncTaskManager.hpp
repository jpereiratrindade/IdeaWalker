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
 *
 * Uses std::async() with stored futures instead of detached threads.
 * The destructor waits for all in-flight tasks to complete, preventing
 * undefined behavior from dangling references after shutdown.
 */
class AsyncTaskManager {
public:
    AsyncTaskManager() = default;

    /**
     * @brief Destructor — blocks until all active tasks have completed.
     *
     * This ensures that background lambdas (which may capture `this` pointers
     * or references to AppState members) do not execute after the owning
     * objects have been destroyed.
     */
    ~AsyncTaskManager() {
        WaitAll();
    }

    /**
     * @brief Signals all tasks to stop at the next opportunity and waits.
     *
     * Sets a cancellation flag (tasks may check IsShutdownRequested()) and then
     * blocks until all futures have resolved.
     */
    void Shutdown() {
        m_stopRequested.store(true);
        WaitAll();
    }

    /**
     * @brief Returns true if shutdown has been requested.
     *
     * Background task lambdas can poll this to exit early:
     * @code
     * if (manager->IsShutdownRequested()) return;
     * @endcode
     */
    bool IsShutdownRequested() const {
        return m_stopRequested.load();
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

        // Launch via std::async — future is stored so destructor can join.
        // This replaces the previous std::thread::detach() pattern that could
        // cause UB when tasks outlived the AppState they captured by reference.
        auto fut = std::async(std::launch::async,
            [this, status](auto userFunc, auto... userArgs) {
                try {
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
            },
            std::forward<F>(f), std::forward<Args>(args)...);

        {
            std::lock_guard<std::mutex> lock(m_futuresMutex);
            // Prune already-resolved futures before appending to keep the list small
            m_futures.erase(
                std::remove_if(m_futures.begin(), m_futures.end(),
                    [](const std::future<void>& fut) {
                        return fut.valid() &&
                               fut.wait_for(std::chrono::seconds(0)) == std::future_status::ready;
                    }),
                m_futures.end());
            m_futures.push_back(std::move(fut));
        }

        return status;
    }

    /** @brief Returns snapshots of all active tasks. */
    std::vector<std::shared_ptr<TaskStatus>> GetActiveTasks() {
        std::lock_guard<std::mutex> lock(m_tasksMutex);
        return m_activeTasks;
    }

private:
    /** @brief Blocks until all stored futures resolve. */
    void WaitAll() {
        std::lock_guard<std::mutex> lock(m_futuresMutex);
        for (auto& fut : m_futures) {
            if (fut.valid()) {
                fut.wait();
            }
        }
        m_futures.clear();
    }

    void CleanupCompletedTasks() {
        std::lock_guard<std::mutex> lock(m_tasksMutex);
        m_activeTasks.erase(
            std::remove_if(m_activeTasks.begin(), m_activeTasks.end(),
                [](const auto& s) { return s->isCompleted.load(); }),
            m_activeTasks.end()
        );
    }

    std::atomic<int> m_nextId{0};
    std::atomic<bool> m_stopRequested{false};

    std::vector<std::shared_ptr<TaskStatus>> m_activeTasks;
    std::mutex m_tasksMutex;

    std::vector<std::future<void>> m_futures;
    std::mutex m_futuresMutex;
};

} // namespace ideawalker::application
