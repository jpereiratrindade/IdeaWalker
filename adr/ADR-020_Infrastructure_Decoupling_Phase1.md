# ADR-020: Infrastructure Decoupling Phase 1

**Status:** Accepted
**Date:** 2026-05-26
**Decision Type:** Architectural / Technical Debt Reduction
**Supersedes:** (none)

## Context

The IdeaWalker codebase historically centralized several infrastructural and UI responsibilities, leading to a tightly coupled architecture. Notable issues included:
1. Hardcoded logging via `std::cout` dispersed across domain services, making it impossible to capture and display logs in the application's UI.
2. The `DocOpsPanel` acting as a "God object", holding over 1500 lines of code and mixing low-level file, date, and shell utilities with ImGui rendering logic.
3. System prompts tightly coupled in compiled C++ code, forcing a full rebuild for any persona or orchestration iteration.
4. Unsafe shell command execution (lack of standard escaping for `popen` arguments).

## Decision

We enacted a multi-phased technical debt reduction targeting Infrastructure and UI Decoupling (Phase 1):

1. **Centralized Abstract Logger (`ILogger`)**: Replaced direct standard output usage with an injected `ILogger` interface. `AppLogger` implements this to stream data into the UI, while `TestLogger` is provided for tests.
2. **Modularization of Utilities (`DocOpsUtils.hpp/cpp`)**: Extracted all pure functions (string parsing, timestamps, shell operations, Git diff validation) from `DocOpsPanel` into isolated infrastructure utility modules. 
3. **External Prompt Catalog with Fallback**: Modified `PromptCatalog` to resolve prompts dynamically via filesystem files (`.md`). The application first queries a configured external directory and, if unavailable, falls back gracefully to the compiled defaults.
4. **Safe Shell Utilities (`ShellUtils.hpp`)**: Introduced deterministic single-quote escaping mechanisms (`ShellEscape`) to guarantee safe sub-shell command executions (`ContentExtractor` and `DocOpsUtils`).

## Scope / Boundaries

- The `ILogger` abstraction applies strictly to application and domain services logging. It does not replace error-handling mechanisms (like `std::optional` or exceptions for critical failures).
- Prompt files are optional overrides. The system remains fully operational without them.

## Allowed and Prohibited Flows

- **Allowed**: Injecting `ILogger` into services that need telemetry.
- **Allowed**: Extending `DocOpsUtils` for any standard path or shell normalization.
- **Prohibited**: Direct usage of `std::cout` or `std::cerr` within Domain or UI code.
- **Prohibited**: Hardcoding new multi-line personas without an external `.md` fallback counterpart.

## Consequences and Trade-offs

- **Positive**: Testing is simpler, and log capture inside the application UI is fully supported.
- **Positive**: Iterating on AI Prompts is instant (no compilation needed).
- **Negative**: There is a slight overhead reading prompts from the disk, but it is negligible given the nature of the application.

## Validation Criteria

- **Automated Tests**: Unit tests must use mock/null loggers to avoid stdout pollution.
- **Code Review**: Any PR introducing `std::cout` to the Domain must be rejected.
