ADR-003 — Epistemic Regime Explicit Declaration

Status

Accepted

Context

Narratives operate under different epistemic regimes.

Decision

Every NarrativeVectorDTO must contain an epistemic subvector including:

-   narrativeRegime
-   truthCommitment
-   evidentialDensity
-   allowsInference

Justification

To prevent mixing fictional, testimonial, and factual regimes.

Consequences

-   Extraction is invalid without explicit regime declaration.

Invariants Derived

-   narrativeRegime is mandatory.
