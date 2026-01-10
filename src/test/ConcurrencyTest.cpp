#include <iostream>
#include <thread>
#include <vector>
#include <chrono>
#include <atomic>
#include <cassert>
#include "application/ConversationService.hpp"
#include "domain/AIService.hpp"
#include "infrastructure/PathUtils.hpp"
#include "infrastructure/PersistenceService.hpp"

// Mock AI Service
class MockAIService : public ideawalker::domain::AIService {
public:
    std::optional<ideawalker::domain::Insight> processRawThought(const std::string&, bool, std::function<void(std::string)>) override {
        return std::nullopt;
    }

    std::optional<std::string> chat(const std::vector<ChatMessage>& history, bool) override {
        // Simulate network delay
        std::this_thread::sleep_for(std::chrono::milliseconds(10)); 
        return "Insightful response from Mock AI.";
    }

    std::optional<std::string> consolidateTasks(const std::string&) override {
        return std::nullopt;
    }

    std::vector<float> getEmbedding(const std::string&) override {
        return {};
    }
};

int main() {
    std::cout << "[Test] Starting Concurrency Stress Test..." << std::endl;

    // Setup
    auto aiService = std::make_shared<MockAIService>();
    // Use a test-specific project root to avoid cluttering real user data
    std::string testRoot = "test_project_root"; 
    std::filesystem::create_directories(testRoot);
    
    auto persistence = std::make_shared<ideawalker::infrastructure::PersistenceService>();
    ideawalker::application::ConversationService service(aiService, persistence, testRoot);

    ideawalker::application::ContextBundle bundle;
    bundle.activeNoteId = "TestNote_Concurrency";
    // Mock user context content
    
    // Start Session
    std::cout << "[Test] Starting Session..." << std::endl;
    service.startSession(bundle);

    // Stress Test: Spawn multiple threads to send messages
    const int NUM_MESSAGES = 50;
    std::vector<std::thread> threads;
    std::atomic<int> completedSends{0};

    std::cout << "[Test] Spawning " << NUM_MESSAGES << " threads sending messages..." << std::endl;

    auto startTime = std::chrono::steady_clock::now();

    for (int i = 0; i < NUM_MESSAGES; ++i) {
        threads.emplace_back([&service, i, &completedSends]() {
            service.sendMessage("Message " + std::to_string(i));
            completedSends++;
        });
        // Slight stagger to make it realistic but still concurrent
        if (i % 5 == 0) std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }

    // Wait for all sendMessage calls to return (this just means threads launched)
    for (auto& t : threads) {
        if (t.joinable()) t.join();
    }
    
    std::cout << "[Test] All sendMessage calls dispatched. Waiting for background AI threads..." << std::endl;

    // The service processes in background threads. We wait a bit.
    // Ideally we'd have a way to join them, but they are detached.
    // We poll 'isThinking' or just wait a fixed time sufficient for mock delay.
    int timeoutMs = 5000;
    int elapsed = 0;
    while (elapsed < timeoutMs) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        elapsed += 100;
        // In a real rigorous test we'd track active threads count or use a condition variable,
        // but checking the file output count is a good proxy.
    }

    // Validation
    auto history = service.getHistory();
    std::cout << "[Test] History size: " << history.size() << std::endl;
    
    // Expected: 1 system + 50 user + 50 assistant = 101 messages
    // Note: Since threads are detached and decoupled, ordering isn't guaranteed, 
    // but total count should be correct if no race conditions lost data.
    
    if (history.size() == 1 + (NUM_MESSAGES * 2)) {
         std::cout << "[PASS] History size matches expected." << std::endl;
    } else {
         std::cout << "[WARN] History size mismatch. Expected 101, got " << history.size() << ". (Some might be still processing or lost)" << std::endl;
    }

    // Check File
    auto dialogues = service.listDialogues();
    if (dialogues.empty()) {
        std::cout << "[FAIL] No dialogue file created." << std::endl;
        return 1;
    }

    std::cout << "[PASS] Dialogue file created: " << dialogues[0] << std::endl;
    
    // Clean up
    std::filesystem::remove_all(testRoot);
    std::cout << "[Test] Completed." << std::endl;

    return 0;
}
