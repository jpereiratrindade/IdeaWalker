# IdeaWalker (IW)

## ADR-007 — Bifásic Scientific Ingestion Pipeline

**Status:** Accepted  
**Date:** 2026-02-25  
**Related Changelog:** v0.1.13-beta  

---

## Context

The `ScientificIngestionService` was originally designed with a single AI turn per document:
a combined prompt asking for both **narrative observation** (facts, theories) and
**discursive context** (frames, rhetoric, problem systems) simultaneously.

With smaller local models (7B parameter class, e.g., Qwen 2.5 7B), this combined single-turn
prompt produced unreliable outputs — the model frequently ignored one dimension entirely,
collapsing either the narrative or discursive analysis.

---

## Decision

Scientific ingestion is split into **two distinct cognitive turns**:

1. **Narrative Turn** — focuses exclusively on factual observations, theories, and
   empirical claims. Produces a `NarrativeObservation` artifact.

2. **Discursive Turn** — focuses exclusively on discursive frames, rhetorical structure,
   valence, and problem-effect-action systems. Produces a `DiscursiveContext` artifact.

Each turn is an independent LLM invocation with its own system prompt, context, and output
schema. The two outputs are then validated and bundled by the `ScientificIngestionService`.

---

## Justification

1. **Attention scope**: smaller models cannot maintain dual epistemic focus over long prompts.
   Splitting the task into single-objective turns eliminates the attention collapse problem.

2. **Schema fidelity**: each turn can be validated against its own schema without pollution
   from the other epistemic layer.

3. **Debuggability**: failures in narrative extraction are isolated from failures in discursive
   extraction, enabling precise diagnostics.

4. **Alignment with ADR-001**: the bifásic design reinforces the principle that IW operates
   in the observational domain — each turn observes a **distinct epistemic layer** of the
   document.

---

## Consequences

- `ScientificIngestionService` must always perform exactly two AI turns per ingestão.
- A single-turn scientific ingestion is non-conformant.
- Narrative and discursive outputs must be persisted as separate artifacts
  (`NarrativeObservation.json` and `DiscursiveContext.json`).
- Both artifacts are bundled into the `NarrativeBundle` for export (ADR-005).

---

## Invariants Derived

- Narrative and discursive layers are **never merged** in a single AI call.
- Each layer has an independent validation step before bundling.
- Empty discursive output is not an error (no frames detected is a valid result).

---

## Implementation Notes

- **Module affected**: `src/application/ScientificIngestionService.cpp`
- **Artifacts produced**:
  - `observations/NarrativeObservation.json`
  - `observations/DiscursiveContext.json`
- **Validation**: Epistemic Validator (VE-IW) runs after each turn independently.

---

*End of Document*
