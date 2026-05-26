/**
 * @file Logger.hpp
 * @brief Centralized, thread-safe logging interface and implementations.
 *
 * Replaces all std::cout / std::cerr calls in the application and
 * infrastructure layers with a structured, injectable logger.
 *
 * Usage in services:
 * @code
 *   m_logger->log(LogLevel::Info, "[AI] Starting processing for: " + filename);
 * @endcode
 *
 * Usage in tests (no UI dependency):
 * @code
 *   auto logger = std::make_shared<TestLogger>();
 *   MyService svc(logger, ...);
 *   // assert on logger->captured
 * @endcode
 */

#pragma once

#include <functional>
#include <mutex>
#include <sstream>
#include <string>
#include <vector>

namespace ideawalker::infrastructure {

/**
 * @enum LogLevel
 * @brief Severity level for log messages.
 */
enum class LogLevel {
    Debug,   ///< Detailed diagnostic info (disabled in release builds)
    Info,    ///< Normal operational messages
    Warning, ///< Recoverable issues
    Error    ///< Failures that affect functionality
};

/**
 * @class ILogger
 * @brief Abstract interface for application-wide logging.
 */
class ILogger {
public:
    virtual ~ILogger() = default;

    /** @brief Logs a message at the given severity level. */
    virtual void log(LogLevel level, const std::string& message) = 0;

    // Convenience helpers
    void debug(const std::string& msg)   { log(LogLevel::Debug,   msg); }
    void info(const std::string& msg)    { log(LogLevel::Info,    msg); }
    void warning(const std::string& msg) { log(LogLevel::Warning, msg); }
    void error(const std::string& msg)   { log(LogLevel::Error,   msg); }
};

/**
 * @class AppLogger
 * @brief Production logger that routes messages to the AppState log via callback.
 *
 * The callback is typically AppState::AppendLog — but the logger itself has
 * no dependency on AppState or any UI header.
 */
class AppLogger : public ILogger {
public:
    /**
     * @brief Constructs an AppLogger with a sink callback.
     * @param sink Callable that accepts a formatted log line (thread-safe).
     */
    explicit AppLogger(std::function<void(const std::string&)> sink)
        : m_sink(std::move(sink)) {}

    void log(LogLevel level, const std::string& message) override {
        if (!m_sink) return;
        const char* prefix = levelPrefix(level);
        m_sink(std::string(prefix) + message + "\n");
    }

private:
    std::function<void(const std::string&)> m_sink;

    static const char* levelPrefix(LogLevel level) {
        switch (level) {
            case LogLevel::Debug:   return "[DEBUG] ";
            case LogLevel::Info:    return "[INFO] ";
            case LogLevel::Warning: return "[WARN] ";
            case LogLevel::Error:   return "[ERROR] ";
        }
        return "[LOG] ";
    }
};

/**
 * @class TestLogger
 * @brief Test-only logger that captures all messages in a vector.
 *
 * Allows unit tests to assert on log output without any UI or filesystem dependency.
 */
class TestLogger : public ILogger {
public:
    void log(LogLevel level, const std::string& message) override {
        std::lock_guard<std::mutex> lock(m_mutex);
        captured.push_back({level, message});
    }

    /** @brief Checks if any captured message contains the given substring. */
    bool contains(const std::string& substring) const {
        std::lock_guard<std::mutex> lock(m_mutex);
        for (const auto& entry : captured) {
            if (entry.message.find(substring) != std::string::npos) return true;
        }
        return false;
    }

    /** @brief Clears all captured messages. */
    void clear() {
        std::lock_guard<std::mutex> lock(m_mutex);
        captured.clear();
    }

    struct LogEntry {
        LogLevel level;
        std::string message;
    };

    std::vector<LogEntry> captured;

private:
    mutable std::mutex m_mutex;
};

/**
 * @class NullLogger
 * @brief No-op logger for use in headless/benchmark contexts.
 */
class NullLogger : public ILogger {
public:
    void log(LogLevel, const std::string&) override {}
};

} // namespace ideawalker::infrastructure
