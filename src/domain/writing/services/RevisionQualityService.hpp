/**
 * @file RevisionQualityService.hpp
 * @brief Domain service to analyze revision quality.
 */

#pragma once

#include <string>
#include <vector>
#include <sstream>
#include <set>
#include <algorithm>
#include <cctype>

namespace ideawalker::domain::writing {

struct QualityReport {
    std::vector<std::string> warnings;
    float compressionRatio = 1.0f;
    bool passed = true;
};

class RevisionQualityService {
public:
    static QualityReport analyze(const std::string& oldContent, const std::string& newContent) {
        QualityReport report;
        
        // 1. Term Retention (Capitalized Words Heuristic)
        std::set<std::string> oldTerms = extractCapitalizedTerms(oldContent);
        std::set<std::string> newTerms = extractCapitalizedTerms(newContent);
        
        std::vector<std::string> missingTerms;
        for (const auto& term : oldTerms) {
            if (newTerms.find(term) == newTerms.end()) {
                // Double check if it exists in lowercase to avoid false positives on casing changes
                if (!containsCaseInsensitive(newContent, term)) {
                    missingTerms.push_back(term);
                }
            }
        }

        if (!missingTerms.empty()) {
            std::string msg = "Potential loss of key terms: ";
            for (size_t i = 0; i < std::min((size_t)3, missingTerms.size()); ++i) {
                msg += missingTerms[i] + (i < missingTerms.size() - 1 ? ", " : ".");
            }
            if (missingTerms.size() > 3) msg += "..";
            report.warnings.push_back(msg);
            report.passed = false;
        }

        // 2. Compression Ratio
        if (!oldContent.empty()) {
            report.compressionRatio = (float)newContent.length() / (float)oldContent.length();
            if (report.compressionRatio < 0.5f) {
                report.warnings.push_back("Significant content reduction (compression < 50%). Check for evidence loss.");
                report.passed = false;
            }
        }

        return report;
    }

private:
    static std::set<std::string> extractCapitalizedTerms(const std::string& text) {
        std::set<std::string> terms;
        std::stringstream ss(text);
        std::string word;
        while (ss >> word) {
            // cleanup punctuation
            word.erase(std::remove_if(word.begin(), word.end(), ::ispunct), word.end());
            
            if (!word.empty() && std::isupper(word[0]) && word.length() > 3) {
                // Ignore common start-of-sentence words if easy (naive implementation here)
                terms.insert(word);
            }
        }
        return terms;
    }

    static bool containsCaseInsensitive(const std::string& text, const std::string& term) {
        auto it = std::search(
            text.begin(), text.end(),
            term.begin(), term.end(),
            [](char ch1, char ch2) { return std::toupper(ch1) == std::toupper(ch2); }
        );
        return (it != text.end());
    }
};

} // namespace ideawalker::domain::writing
