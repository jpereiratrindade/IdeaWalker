# IdeaWalker (IW)

## ADR-010 — Async Task Manager

**Status:** Accepted  
**Date:** 2026-02-25  
**Related Changelog:** v0.1.8-beta  

---

## Context

Prior to v0.1.8, background operations (audio transcription, document indexing, AI processing)
were dispatched directly from UI event handlers or service methods without centralized
lifecycle management.

This caused several structural problems:

1. **UI blocking**: long-running operations froze the render loop.
2. **No unified status**: each background task managed its own status reporting independently,
   producing inconsistent user feedback.
3. **Resource conflicts**: concurrent background tasks could access shared state without
   coordination, leading to race conditions.
4. **No cancellation**: tasks could not be safely interrupted.

---

## Decision

All background tasks in the IdeaWalker must be dispatched through a centralized
**`AsyncTaskManager`** component.

The `AsyncTaskManager` is responsible for:

1. **Submission**: receiving any callable task and assigning it a typed handle.
2. **Execution**: running tasks in a managed thread pool.
3. **Status reporting**: providing a unified `TaskStatus` (Pending / Running / Done / Failed)
   queryable from the UI.
4. **Lifecycle control**: ensuring tasks can be monitored and that resources are released
   correctly on completion or failure.

---

## Justification

- Centralizing task management eliminates ad-hoc threading scattered across service layers.
- A unified status model enables a single UI component to display all background activity
  consistently (progress bars, status labels).
- Isolation of background execution from UI handlers preserves ImGui's single-threaded
  rendering contract.
- This pattern is a prerequisite for adding task cancellation in future versions.

---

## Consequences

- No module may spawn a raw `std::thread` or `std::async` for background work without
  routing through `AsyncTaskManager`.
- The UI must query task status exclusively through `AsyncTaskManager::getStatus()`.
- Services (`AIProcessingService`, `ScientificIngestionService`, transcription adapters) must
  submit work via `AsyncTaskManager` rather than managing their own threads.

---

## Invariants Derived

- `AsyncTaskManager` is the **single point of entry** for all background task submission.
- No direct thread management in application or UI layers.
- Task status is always available for UI inspection without polling the task itself.

---

## Implementation Notes

- **Module**: `src/application/AsyncTaskManager.hpp` / `.cpp`
- **Thread pool size**: configurable; default = system hardware concurrency.
- **UI integration**: `src/ui/` panels query `AsyncTaskManager` during render loop for
  status display.
- **Task types managed**: Transcription, DocumentIndexing, AIProcessing, ScientificIngestion.

---

*End of Document*
