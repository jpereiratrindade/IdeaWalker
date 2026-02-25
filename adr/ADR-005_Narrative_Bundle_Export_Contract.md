ADR-005 — Narrative Bundle Export Contract

Status

Accepted

Context

Narrative vectors are exported to downstream systems.

Decision

Export must occur exclusively through a structured NarrativeBundle
containing: - NarrativeVectorDTO - rawExcerptRef - extractionNotes -
toolchain metadata

Raw text may not be exported independently.

Justification

To avoid informal ingestion of unstructured text.

Consequences

-   Downstream systems receive only structured bundles.

Invariants Derived

-   No text export without associated vector.
