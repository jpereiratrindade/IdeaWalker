# ADR-021: AppState Decomposition (AppController and ViewModel)

**Status:** Accepted
**Date:** 2026-05-26
**Decision Type:** Architectural / UI Decoupling
**Supersedes:** (none)

## Context

The `AppState` structure was originally designed as a global state bag to facilitate ImGui rendering. However, as the application grew, it became a "God Object" holding:
1. Purely reactive UI states (e.g., active tabs, modal visibility).
2. Domain representations (e.g., the neural web graph, active insights).
3. Business logic orchestrations (e.g., `AnalyzeSuggestions()`, `RebuildGraph()`).
4. Hard dependencies on Application Services (`AppServices`).

This monolithic structure caused tight coupling, making unit testing impossible for UI layers and forcing all ImGui panels to have unrestricted access to execute core infrastructure logic.

## Decision

We have decoupled `AppState` into two distinct structural components following an MVC/MVVM paradigm:

### 1. ViewModel (`src/ui/ViewModel.hpp`)
- Contains strictly the data structures needed for UI rendering (`ProjectState`, `NeuralWebState`, `UiState`).
- Stripped of all logic methods.
- Includes basic thread-safe wrappers for appending logs and updating processing statuses.
- **Rule**: The UI can only read from or mutate the `ViewModel`. It cannot execute logic directly.

### 2. AppController (`src/application/AppController.hpp`)
- Acts as the central orchestrator and logic container for the application.
- Holds the single source of truth for the `ViewModel` (`m_viewModel`) and the injected services (`m_services`).
- Encapsulates all side-effecting operations (e.g., `OpenProject`, `AnalyzeSuggestions`, `CheckForUpdates`).
- ImGui panels now receive `AppController& controller` and must use `controller.GetViewModel()` for reading/writing view data, and call explicit controller methods to trigger business logic.

## Scope / Boundaries

- The UI layer (`src/ui/panels/`) is strictly restricted to manipulating the `ViewModel` and calling high-level `AppController` commands.
- The `AppController` abstracts the raw `AppServices`, minimizing the risk of a UI panel accidentally performing an unauthorized domain operation.

## Allowed and Prohibited Flows

- **Allowed**: `AppController` updating the `ViewModel` via background threads.
- **Allowed**: `AppController` delegating complex logic to `KnowledgeService` or `AsyncTaskManager`.
- **Prohibited**: Passing `AppServices` directly into an ImGui panel drawing function.
- **Prohibited**: Mixing UI modal logic inside Domain Services.

## Consequences and Trade-offs

- **Positive**: Clear separation of concerns between UI representation and Domain logic.
- **Positive**: Substantially reduces the blast radius of UI changes.
- **Negative**: Adds a layer of indirection (typing `controller.GetViewModel().ui.X` instead of `app.ui.X`).

## Validation Criteria

- **Automated Tests**: Any UI logic testing can now mock the `ViewModel` safely without triggering `AppServices`.
- **Code Review**: `AppServices` must never be injected directly into a `.cpp` file inside `src/ui/panels/` without going through the `AppController`.
