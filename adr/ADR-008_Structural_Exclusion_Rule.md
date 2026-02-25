# IdeaWalker (IW)

## ADR-008 — Structural Exclusion Rule

**Status:** Accepted  
**Date:** 2026-02-25  
**Related Changelog:** v0.1.13-beta  

> **Nota de Rastreabilidade**: O CHANGELOG v0.1.13 referencia esta decisão como "ADR 001",
> mas o ADR-001 existente trata de Non-Inferential Observation (tema distinto). Esta decisão
> é registrada aqui com o número correto na sequência: ADR-008.

---

## Context

PDF documents ingested into the IW pipeline contain editorial metadata such as journal names,
page numbers, headers, and footers that are structurally embedded in each page.

When `pdftotext` extracts text from these documents, headers and footers are included inline
with the content, polluting the corpus that is subsequently sent to the LLM for narrative
and discursive analysis.

This creates two problems:

1. **Hallucination risk**: LLMs may interpret repetitive editorial markers as content.
2. **Context waste**: editorial noise consumes context window space, reducing effective content.

---

## Decision

A **Structural Exclusion Layer** is added as a preprocessing step in `ContentExtractor`
**before** any text is sent to the AI pipeline.

The rule is:

> A text line that appears in **more than 60% of pages** of the document and occupies a
> **structurally consistent position** (first or last N lines of a page block) is classified
> as structural noise and excluded from the extracted corpus.

---

## Algorithm

1. Extract all text pages via `pdftotext`.
2. For each candidate line (first 3 and last 3 lines of each page block):
   - Track frequency across pages.
3. Lines with frequency > 60% of total pages → marked `STRUCTURAL`.
4. Marked lines are removed before the text is forwarded to any AI service.
5. Exclusion log is written to the extraction metadata (for auditability).

---

## Justification

- The 60% threshold balances suppression of genuine headers/footers against false positives
  (e.g., a phrase that legitimately appears on most pages of a technical document).
- Positional filtering (first/last N lines) prevents deletion of content-bearing repetitions
  (e.g., a recurring hypothesis stated in the body of each section).
- Keeping an exclusion log preserves auditability (ADR-004 invariant).

---

## Consequences

- No AI service receives raw PDF text without first passing through the Structural Exclusion
  Layer.
- Extraction metadata must record the list of excluded structural lines.
- The threshold (60%) is a configurable parameter, not a hard-coded magic number.

---

## Invariants Derived

- `ContentExtractor` must apply Structural Exclusion before forwarding to AI.
- The exclusion log is mandatory.
- The threshold must be explicit and documented at the extraction site.

---

## Implementation Notes

- **Module affected**: `src/infrastructure/ContentExtractor.hpp` / `.cpp`
- **Threshold constant**: `STRUCTURAL_EXCLUSION_THRESHOLD = 0.60f`
- **Positional window**: first 3 and last 3 lines of each page block

---

*End of Document*
