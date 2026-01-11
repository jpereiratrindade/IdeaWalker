/**
 * @file WritingIntent.hpp
 * @brief Value Object encapsulating the strategic intent of the writing.
 */

#pragma once

#include <string>
#include <stdexcept>

namespace ideawalker::domain::writing {

/**
 * @struct WritingIntent
 * @brief Defines the 'Why' and 'Who' of the text.
 * 
 * Invariant: Purpose and Audience must not be empty.
 */
class WritingIntent {
public:
    std::string purpose;         ///< E.g., "to persuade", "to report".
    std::string audience;        ///< E.g., "technical team", "general public".
    std::string coreClaim;       ///< The central thesis or research question.
    std::string constraints;     ///< E.g., "max 500 words", "ABNT format".

    WritingIntent() = default;

    WritingIntent(std::string p, std::string a, std::string c = "", std::string constr = "")
        : purpose(std::move(p)), audience(std::move(a)), coreClaim(std::move(c)), constraints(std::move(constr)) {
        validate();
    }

    void validate() const {
        if (purpose.empty()) {
            throw std::invalid_argument("WritingIntent: Purpose cannot be empty.");
        }
        if (audience.empty()) {
            throw std::invalid_argument("WritingIntent: Audience cannot be empty.");
        }
    }

    bool isValid() const {
        return !purpose.empty() && !audience.empty();
    }
    
    bool operator==(const WritingIntent& other) const {
        return purpose == other.purpose && 
               audience == other.audience && 
               coreClaim == other.coreClaim && 
               constraints == other.constraints;
    }
};

} // namespace ideawalker::domain::writing
