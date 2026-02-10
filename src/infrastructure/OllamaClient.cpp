#include "infrastructure/OllamaClient.hpp"
#include <httplib.h>
#include <iostream>

namespace ideawalker::infrastructure {

using json = nlohmann::json;

namespace {
constexpr double kDeterministicTemperature = 0.0;
constexpr double kDeterministicTopP = 1.0;
constexpr int kDeterministicSeed = 42;
}

OllamaClient::OllamaClient(const std::string& host, int port)
    : m_host(host), m_port(port) {}

std::optional<std::string> OllamaClient::generate(const std::string& model, 
                                                const std::string& system, 
                                                const std::string& prompt, 
                                                bool forceJson) {
    httplib::Client cli(m_host, m_port);
    cli.set_read_timeout(600); // 10 min

    json requestData = {
        {"model", model},
        {"prompt", system + "\n\nTexto:\n" + prompt},
        {"stream", false},
        {"options", {
            {"temperature", kDeterministicTemperature},
            {"top_p", kDeterministicTopP},
            {"seed", kDeterministicSeed}
        }}
    };
    if (forceJson) {
        requestData["format"] = "json";
    }

    auto res = cli.Post("/api/generate", requestData.dump(), "application/json");
    if (res && res->status == 200) {
        try {
            auto body = json::parse(res->body);
            if (body.contains("response")) {
                return body["response"].get<std::string>();
            }
        } catch (const std::exception& e) {
            std::cerr << "[OllamaClient] JSON Parse Error: " << e.what() << std::endl;
        }
    } else {
        if (res) {
            std::cerr << "[OllamaClient] HTTP Error " << res->status << ": " << res->body << std::endl;
        } else {
            std::cerr << "[OllamaClient] Connection failed: " << static_cast<int>(res.error()) << std::endl;
        }
    }
    return std::nullopt;
}

std::optional<std::string> OllamaClient::chat(const std::string& model, 
                                           const nlohmann::json& messages, 
                                           bool stream,
                                           bool forceJson) {
    httplib::Client cli(m_host, m_port);
    cli.set_read_timeout(600);

    json requestData = {
        {"model", model},
        {"messages", messages},
        {"stream", stream},
        {"options", {
            {"temperature", kDeterministicTemperature},
            {"top_p", kDeterministicTopP},
            {"seed", kDeterministicSeed}
        }}
    };
    if (forceJson) {
        requestData["format"] = "json";
    }

    auto res = cli.Post("/api/chat", requestData.dump(), "application/json");
    if (res && res->status == 200) {
        try {
            auto body = json::parse(res->body);
            if (body.contains("message") && body["message"].contains("content")) {
                return body["message"]["content"].get<std::string>();
            }
        } catch (const std::exception& e) {
            std::cerr << "[OllamaClient] Chat JSON Parse Error: " << e.what() << std::endl;
        }
    }
    return std::nullopt;
}

std::vector<float> OllamaClient::getEmbedding(const std::string& model, const std::string& text) {
    httplib::Client cli(m_host, m_port);
    cli.set_read_timeout(180);

    json requestData = {
        {"model", model},
        {"prompt", text}
    };

    auto res = cli.Post("/api/embeddings", requestData.dump(), "application/json");
    if (res && res->status == 200) {
        try {
            auto body = json::parse(res->body);
            if (body.contains("embedding") && body["embedding"].is_array()) {
                return body["embedding"].get<std::vector<float>>();
            }
        } catch (...) {}
    }
    return {};
}

std::vector<std::string> OllamaClient::getAvailableModels() {
    httplib::Client cli(m_host, m_port);
    cli.set_read_timeout(5);
    
    auto res = cli.Get("/api/tags");
    std::vector<std::string> models;
    if (res && res->status == 200) {
        try {
            auto body = json::parse(res->body);
            if (body.contains("models") && body["models"].is_array()) {
                for (const auto& item : body["models"]) {
                    if (item.contains("name")) {
                        models.push_back(item["name"].get<std::string>());
                    }
                }
            }
        } catch (...) {}
    }
    return models;
}

} // namespace ideawalker::infrastructure
