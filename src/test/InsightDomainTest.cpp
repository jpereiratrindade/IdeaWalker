/**
 * @file InsightDomainTest.cpp
 * @brief Unit tests for the Insight domain entity.
 *
 * Tests the core domain logic: actionable parsing, toggle cycle,
 * wikilink extraction, and snapshot history.
 * No external dependencies (no Ollama, no filesystem).
 */

#include <cassert>
#include <iostream>
#include <string>
#include "domain/Insight.hpp"

using namespace ideawalker::domain;

// ─── Helpers ────────────────────────────────────────────────────────────────

static Insight makeInsight(const std::string& content) {
    Insight::Metadata meta;
    meta.id = "test-note";
    meta.title = "Test Note";
    return Insight(meta, content);
}

static void passMsg(const char* name) {
    std::cout << "[PASS] " << name << "\n";
}

// ─── Tests ──────────────────────────────────────────────────────────────────

void Test_ParseActionables_AllStates() {
    const std::string content =
        "# Note\n"
        "- [ ] Task todo\n"
        "- [x] Task done\n"
        "- [/] Task in progress\n"
        "Normal line\n";

    auto insight = makeInsight(content);
    insight.parseActionablesFromContent();

    const auto& tasks = insight.getActionables();
    assert(tasks.size() == 3 && "Expected 3 tasks");
    assert(!tasks[0].isCompleted && !tasks[0].isInProgress && "Task 0 should be todo");
    assert(tasks[1].isCompleted   && !tasks[1].isInProgress && "Task 1 should be done");
    assert(!tasks[2].isCompleted  && tasks[2].isInProgress  && "Task 2 should be in-progress");
    assert(tasks[0].description == "Task todo");
    assert(tasks[1].description == "Task done");
    assert(tasks[2].description == "Task in progress");
    passMsg("ParseActionables_AllStates");
}

void Test_ParseActionables_EmptyContent() {
    auto insight = makeInsight("No tasks here, just prose.");
    insight.parseActionablesFromContent();
    assert(insight.getActionables().empty() && "Expected no tasks in prose-only content");
    passMsg("ParseActionables_EmptyContent");
}

void Test_ToggleActionable_Cycle() {
    // Cycle: todo → in-progress → done → todo
    const std::string content = "- [ ] Cycle task\n";
    auto insight = makeInsight(content);
    insight.parseActionablesFromContent();
    assert(insight.getActionables().size() == 1);

    // todo → in-progress
    insight.toggleActionable(0);
    assert(!insight.getActionables()[0].isCompleted);
    assert(insight.getActionables()[0].isInProgress);
    assert(insight.getContent().find("- [/]") != std::string::npos);

    // in-progress → done
    insight.toggleActionable(0);
    assert(insight.getActionables()[0].isCompleted);
    assert(!insight.getActionables()[0].isInProgress);
    assert(insight.getContent().find("- [x]") != std::string::npos);

    // done → todo
    insight.toggleActionable(0);
    assert(!insight.getActionables()[0].isCompleted);
    assert(!insight.getActionables()[0].isInProgress);
    assert(insight.getContent().find("- [ ]") != std::string::npos);

    passMsg("ToggleActionable_Cycle");
}

void Test_ToggleActionable_OutOfBounds() {
    auto insight = makeInsight("- [ ] Only task\n");
    insight.parseActionablesFromContent();
    // Should not crash or modify anything
    insight.toggleActionable(99);
    assert(insight.getActionables().size() == 1);
    passMsg("ToggleActionable_OutOfBounds");
}

void Test_FilterByStatus() {
    const std::string content =
        "- [ ] Todo 1\n"
        "- [x] Done 1\n"
        "- [x] Done 2\n"
        "- [/] Progress 1\n";
    auto insight = makeInsight(content);
    insight.parseActionablesFromContent();

    auto done = insight.getTasksByStatus(true);
    auto pending = insight.getTasksByStatus(false);

    assert(done.size() == 2   && "Expected 2 completed tasks");
    assert(pending.size() == 2 && "Expected 2 non-completed tasks (todo + in-progress)");
    passMsg("FilterByStatus");
}

void Test_ParseReferences_WikiLinks() {
    const std::string content =
        "See [[ConceptA]] and [[ConceptB]] for more.\n"
        "Also [[Nested Link with spaces]].\n"
        "This [[is unclosed";  // malformed: should be ignored

    auto insight = makeInsight(content);
    insight.parseReferencesFromContent();

    const auto& refs = insight.getReferences();
    assert(refs.size() == 3 && "Expected 3 valid wikilinks");
    assert(refs[0] == "ConceptA");
    assert(refs[1] == "ConceptB");
    assert(refs[2] == "Nested Link with spaces");
    passMsg("ParseReferences_WikiLinks");
}

void Test_ParseReferences_Empty() {
    auto insight = makeInsight("No links here.");
    insight.parseReferencesFromContent();
    assert(insight.getReferences().empty() && "Expected no references");
    passMsg("ParseReferences_Empty");
}

void Test_CognitiveSnapshots() {
    auto insight = makeInsight("Content");
    assert(insight.getSnapshots().empty());

    CognitiveSnapshot snap;
    snap.persona = AIPersona::AnalistaCognitivo;
    snap.state = CognitiveState::Convergent;
    snap.textInput = "raw";
    snap.textOutput = "structured";
    snap.timestamp = "2026-05-26 17:00:00";

    insight.addSnapshot(snap);
    assert(insight.getSnapshots().size() == 1);
    assert(insight.getSnapshots()[0].persona == AIPersona::AnalistaCognitivo);
    passMsg("CognitiveSnapshots");
}

void Test_SetContent_UpdatesInPlace() {
    auto insight = makeInsight("- [ ] Old task\n");
    insight.parseActionablesFromContent();
    assert(insight.getActionables().size() == 1);

    insight.setContent("- [ ] New task A\n- [x] New task B\n");
    insight.parseActionablesFromContent();
    assert(insight.getActionables().size() == 2);
    assert(insight.getActionables()[0].description == "New task A");
    passMsg("SetContent_UpdatesInPlace");
}

// ─── Main ────────────────────────────────────────────────────────────────────

int main() {
    std::cout << "=== InsightDomainTest ===\n";

    Test_ParseActionables_AllStates();
    Test_ParseActionables_EmptyContent();
    Test_ToggleActionable_Cycle();
    Test_ToggleActionable_OutOfBounds();
    Test_FilterByStatus();
    Test_ParseReferences_WikiLinks();
    Test_ParseReferences_Empty();
    Test_CognitiveSnapshots();
    Test_SetContent_UpdatesInPlace();

    std::cout << "\nAll tests passed.\n";
    return 0;
}
