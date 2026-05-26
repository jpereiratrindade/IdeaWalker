#include <iostream>
#include <string>

#ifndef IDEAWALKER_VERSION
#define IDEAWALKER_VERSION "unknown"
#endif

int main() {
    const std::string version = IDEAWALKER_VERSION;
    if (version.empty()) {
        std::cerr << "[FAIL] IDEAWALKER_VERSION is empty\n";
        return 1;
    }

    std::cout << "[PASS] IdeaWalker smoke test initialized: " << version << "\n";
    return 0;
}
