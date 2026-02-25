# IdeaWalker (IW)

## ADR-011 — Contextual RAG Architecture

**Status:** Accepted  
**Date:** 2026-02-25  
**Related Document:** `DDD_CONTEXTUAL_RAG_IDEAWALKER.md`  

---

## Context

Many RAG (Retrieval-Augmented Generation) systems rely on **vector similarity search** to
retrieve chunks of text that are then injected raw into an LLM prompt. This approach has
inherent problems for a structured epistemic tool:

1. **Implicit context**: the model receives undifferentiated text without knowing the role or
   ontological status of each piece of information.
2. **Loss of structure**: semantic similarity does not respect domain boundaries (e.g., a
   task item and a narrative observation may appear similar to an embedding model but play
   completely different epistemic roles).
3. **Debugging difficulty**: when the model produces incorrect output, it is impossible to
   determine which retrieved chunk caused it without inspecting the embedding space.

---

## Decision

The IdeaWalker implements a **Structural Contextual RAG** model instead of vector-based RAG:

1. Context is **explicitly typed and labeled** before being sent to the LLM.
2. Each context block carries a semantic role identifier (e.g., `ACTIVE_NOTE`,
   `BACKLINKS`, `TASK_STATE`, `NARRATIVE_OBSERVATION`).
3. No text is sent to the LLM without an explicit role label.
4. Context assembly is performed by a dedicated `ContextAssembler` component, which
   selects, composes, and delivers a typed `ContextBundle`.
5. The LLM (Qwen via Ollama) acts strictly as a **semantic interpreter and transformer** —
   it does not select context, navigate the domain, or persist state.

---

## Context Roles (Canonical)

| Role | Source | Access |
|------|--------|--------|
| `ACTIVE_NOTE` | `notas/*.md` | Read / Write |
| `BACKLINKS` | Project structural scan | Read-only |
| `TASK_STATE` | Markdown checkboxes | Read / Write |
| `NARRATIVE_OBSERVATION` | `observations/*.md` | Read-only |
| `UNIFIED_KNOWLEDGE` | Multi-context aggregation | Read-only (experimental) |

---

## Justification

1. **Epistemic clarity**: labeled context prevents the model from conflating observational
   content with actionable task state or narrative with factual note.
2. **Debuggability**: any unexpected model output can be traced to a specific labeled block.
3. **Alignment with ADR-001**: the model never governs its own context — the domain does.
4. **Scalability without vectors**: structured retrieval is deterministic and does not require
   embedding infrastructure, aligning with the local-first / privacy-preserving stance of IW.

---

## Consequences

- The `ContextAssembler` is the authoritative assembly point — no service may build context
  ad hoc and send it directly to `AIService`.
- `ContextBundle` is an **immutable value object** (assembled once per request).
- Vector embeddings (if used in future) are permitted only as an **auxiliary ranking index**
  for selecting which structural contexts to include — never as raw retrieval output.

---

## Invariants Derived

- Every LLM invocation must receive a labeled `ContextBundle`.
- No raw text may be forwarded to the LLM without a role label.
- `ContextAssembler` is always upstream of `AIService`.
- The LLM does not decide which context is relevant — the domain does.

---

## Implementation Notes

- **Context Assembler**: `src/application/ContextAssembler.hpp`
- **Bundle type**: `ContextBundle` (Value Object — immutable after construction)
- **Labeled sections**: rendered as structured text blocks with section headers
  (e.g., `=== ACTIVE_NOTE ===`) before being passed to the prompt.
- **Default selection policy**: Active Note + Backlinks for standard chat;
  Narrative Observation added for scientific/observational queries.

---

*End of Document*
