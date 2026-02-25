# IdeaWalker (IW)

## ADR-009 — Autonomous Cognitive Orchestrator

**Status:** Accepted  
**Date:** 2026-02-25  
**Related Changelog:** v0.1.3-beta, v0.1.4-beta  

---

## Context

In earlier versions (< v0.1.3), the IdeaWalker required the user to manually select an
AI persona (Brainstormer, Analista Cognitivo, Secretário Executivo) before processing a
raw thought or audio transcription.

This design had two problems:

1. **Cognitive overhead for the user**: Someone with ADHD-profile cognitive needs should not
   be required to also diagnose their own processing need before asking for help.

2. **Fixed sequence fallacy**: The optimal sequence of cognitive operators varies per input.
   A raw audio dump may need Brainstormer first, then Analista; a structured draft may only
   need Secretário.

---

## Decision

The manual persona selection mechanism is **permanently removed**. The IW pipeline adopts an
**Autonomous Cognitive Orchestrator** that:

1. **Diagnoses** the cognitive state of the input text (raw, structured, chaotic, analytical).
2. **Selects** the optimal sequence of cognitive operators dynamically.
3. **Executes** the operators in sequence: Brainstormer → Analista → Secretário (or any
   applicable subset), providing real-time status feedback to the user.

---

## Justification

- The orchestrator reduces the user's cognitive load to zero for pipeline selection.
- Diagnosis is performed by the same LLM (Qwen via Ollama), operating as a
  **meta-cognitive supervisor** before any content transformation.
- The explicit sequence and status display ("Running Brainstormer...", "Running Analista...")
  preserves user transparency and control.
- This aligns with ADR-001: the user remains informed; the system observes and structures,
  but does not decide on behalf of the user substantively.

---

## Consequences

- The `AIProcessingService` implements the orchestrator loop.
- No UI component may expose a raw persona selector to the user.
- The cognitive operator sequence must be logged explicitly in the note metadata.
- The orchestrator is allowed to skip operators that are not applicable to a given input.

---

## Invariants Derived

- Persona selection is internal to the orchestrator — never user-facing.
- The applied operator sequence is always recorded (auditability).
- The orchestrator may not alter the substance of the input — only structure it.

---

## Implementation Notes

- **Module affected**: `src/application/AIProcessingService.cpp`
- **Cognitive operators**: `Brainstormer`, `AnalistaCognitivo`, `SecretárioExecutivo`
- **Status feedback**: Status messages are shown in the UI status bar during execution.
- **Profile**: Optimized for ADHD-profile users (primary target audience of IdeaWalker).

---

*End of Document*
