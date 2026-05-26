# ADR-019: ESTRAT Integration Protocol

Status: Accepted
Date: 2026-04-10
Decision Type: Contract
Supersedes: (none)

## Context

The IdeaWalker organizes raw thoughts into structured Markdown notes and exports consumables
to STRATA. However, there is no formal contract for exporting validated epistemic artifacts
to downstream conceptual engines (CCE) or to the research governance layer (LabGP).

The Epistemic Validator (VE-IW), already present in the system, enforces anti-teleology,
anti-normative, and anti-linear-causality checks before any STRATA export. These checks
are precisely aligned with the invariants declared by CCE as preconditions for ingestion.

## Decision

The IdeaWalker adopts the `ESTRAT` protocol (Epistemic Structured Transfer and Traceability
Protocol) as the official mechanism for exporting validated epistemic seeds to the ecosystem.

The output artifact is `ESTRAT-SEED`, produced only after VE-IW approval, containing:
- `insight_title` and `insight_summary`
- `raw_intuition`: the unprocessed source text
- `structured_note_path`: path to the output Markdown note
- `task_actions`: extracted action items
- `connections`: active semantic backlinks
- `epistemic_flags`: explicit declaration of absence of teleology, normative claims,
  and linear causality
- `ve_iw_approved: true` + `ve_iw_seal` (content hash)

## Scope / Boundaries

- Applies to: all exports from IdeaWalker to CCE or LabGP.
- Does NOT apply to: internal notes, DocOps workspaces, writing trajectories.
- The ESTRAT-SEED must carry the raw intuition WITHOUT premature normalization.
  Over-structuring before export would violate CCE's first canonical step.

## Allowed and Prohibited Flows

- Allowed:
  - Exporting ESTRAT-SEED after VE-IW gate passes
  - IdeaWalker ingesting DAI feedback from LabGP as new inbox entries
  - Referencing STRATA consumables from ESTRAT-SEED artifacts

- Prohibited:
  - Exporting ESTRAT-SEED when VE-IW flags are not all false
  - Autonomous agent writing ESTRAT-SEED without human review
  - ESTRAT-SEED carrying conclusions without raw_intuition reference

## Consequences and Trade-offs

- The VE-IW gate becomes the formal boundary between internal processing and
  inter-project output.
- The raw_intuition field ensures CCE can apply its own tension phase without
  receiving a pre-tensioned input.
- Adding the export step adds a review moment that benefits conceptual integrity.

## Validation Criteria

- `ve_iw_approved` must be `true` before file is written to `strata/consumables/estrat/`
- `epistemic_flags` must all be `false`
- `ve_iw_seal` must match SHA-256 hash of `structured_note_path` content

## Related References

- Related ADRs: ADR-006 (IW-STRATA Integration Contract), ADR-011 (Contextual RAG), ADR-012 (Semantic States)
- Related docs: DDD_CONTEXTUAL_RAG_IDEAWALKER.md, 👉 IW_Implementation_Guidelines_Post_Methodology.md
