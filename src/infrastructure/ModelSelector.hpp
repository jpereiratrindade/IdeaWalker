/**
 * @file ModelSelector.hpp
 * @brief Utility for selecting the best AI model from available options.
 */

#pragma once
#include <string>
#include <vector>

namespace ideawalker::infrastructure {

/**
 * @class ModelSelector
 * @brief Separates model selection policy from adapter I/O.
 */
class ModelSelector {
public:
    /** @brief Selects the first available model that matches the priority list. */
    static std::string SelectBest(const std::vector<std::string>& availableModels, 
                                 const std::string& currentDefault = "qwen2.5:7b") {
        if (availableModels.empty()) {
            return currentDefault;
        }

        // 1. If the current model is available, KEEP IT.
        for (const auto& model : availableModels) {
            if (model == currentDefault) {
                return currentDefault;
            }
        }

        const std::vector<std::string> priorities = {
            "qwen2.5:7b",
            "qwen2.5", 
            "llama3", 
            "mistral",
            "gemma",
            "deepseek-coder"
        };

        for (const auto& priority : priorities) {
            for (const auto& model : availableModels) {
                if (model.find(priority) != std::string::npos) {
                    return model;
                }
            }
        }

        return availableModels[0];
    }
};

} // namespace ideawalker::infrastructure
