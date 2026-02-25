# IdeaWalker (IW)

## ADR-000 — Governance Model Overview

**Status:** Accepted  
**Version:** v0.1  
**Date:** 2026-02-25  

---

## Context

The IdeaWalker (IW) is a C++20 epistemic tool designed to extract, structure, and export
narrative observations from heterogeneous documents (text, PDF, audio). It operates as an
observational system within a larger ecosystem that includes the STRATA simulational system.

As the project grew beyond a prototype, architectural and epistemological decisions began to
accumulate without formal documentation, creating risk of drift, inconsistency, and loss of
institutional memory.

A governance mechanism is required to:

- Document all structural and epistemological decisions permanently.
- Prevent implicit or undocumented changes to contracts and invariants.
- Enable future contributors to understand the rationale behind established choices.

---

## Decision

The IdeaWalker project adopts a **formal Architecture Decision Record (ADR) governance model**
as its institutional memory mechanism.

The model has two scopes:

1. **Architectural decisions** — structural, infrastructural, or design decisions affecting
   the codebase organization, pipelines, or module contracts.

2. **Epistemological decisions** — decisions about what IW is allowed to observe, infer,
   export, or delegate, and the regime under which it operates.

---

## Governance Rules

1. Every architecturally or epistemologically significant decision requires a new ADR.
2. Structural changes to DTOs (data transfer objects) require a new ADR.
3. ADRs are **immutable** once set to `Accepted`.
4. ADRs are **never deleted** — only marked `Superseded` with a pointer to replacement.
5. Supersession must update `ADR_INDEX.md`.
6. The `ADR_INDEX.md` must remain consistent with the directory at all times.

---

## ADR Lifecycle

```
Proposed → Accepted → (optionally) Superseded
```

- **Proposed**: Draft under review.
- **Accepted**: Normatively binding. All modules must conform.
- **Superseded**: Replaced by a newer ADR. Kept for traceability.
- **Deprecated**: Decision no longer applies; no replacement.

---

## Justification

Without formal ADR governance:

- Epistemological boundaries erode silently over time.
- Integration contracts between IW and STRATA become ambiguous.
- Refactoring decisions lack institutional rationale.
- Onboarding new contributors becomes dependent on oral tradition.

The ADR model provides a lightweight but durable record that survives personnel changes and
codebase evolution.

---

## Consequences

- ADR creation becomes mandatory for any structural or epistemological change.
- The `ADR_INDEX.md` catalog must be maintained by the same author who creates the ADR.
- Pull requests introducing structural changes without a corresponding ADR are non-conformant.

---

## Invariants Derived

- No epistemological contract may change without a new ADR.
- No DTO schema may change without a new ADR.
- The ADR directory is an append-only log.

---

## Implementation Notes

- **Directory**: `adr/`
- **Template**: `adr/ADR_TEMPLATE.md`
- **Index**: `adr/ADR_INDEX.md`
- **Scope**: Covers all modules under `src/` and integration contracts with STRATA.

---

*End of Document*
