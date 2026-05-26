/**
 * @file ShellUtils.hpp
 * @brief Utilities for safe construction of shell command strings.
 *
 * All popen/system calls that interpolate user-supplied paths MUST
 * use ShellEscape() to prevent command injection and to handle
 * filenames containing spaces, parentheses or special characters.
 *
 * Example:
 * @code
 *   std::string cmd = "pdftotext " + ShellEscape(inputPath) + " -";
 * @endcode
 */

#pragma once

#include <filesystem>
#include <string>

namespace ideawalker::infrastructure {

/**
 * @brief Wraps a filesystem path in single quotes, escaping any embedded
 *        single quotes with the POSIX-safe sequence: '\''
 *
 * This is safe for use with popen() / system() on POSIX systems (Linux/macOS).
 * It does NOT support Windows cmd.exe semantics.
 *
 * @param path The path to escape.
 * @return A shell-safe single-quoted string (e.g., '/home/user/My File (1).pdf')
 */
inline std::string ShellEscape(const std::filesystem::path& path) {
    std::string s = path.string();
    std::string result;
    result.reserve(s.size() + 2);
    result.push_back('\'');
    for (char c : s) {
        if (c == '\'') {
            // End single-quote, escape the quote, restart single-quote
            result.append("'\\''");
        } else {
            result.push_back(c);
        }
    }
    result.push_back('\'');
    return result;
}

/**
 * @brief Overload for std::string paths.
 * @param pathStr The path string to escape.
 */
inline std::string ShellEscape(const std::string& pathStr) {
    return ShellEscape(std::filesystem::path(pathStr));
}

} // namespace ideawalker::infrastructure
