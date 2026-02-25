ADR-004 — Deterministic Extraction Baseline

Status

Accepted

Context

LLM-based extraction may introduce stochastic variation.

Decision

Each extraction must record: - Model name - Model version - Parameters -
Extraction date - Seed (when applicable)

Justification

To guarantee institutional reproducibility and auditability.

Consequences

-   Toolchain metadata is mandatory.

Invariants Derived

-   Extraction must be reproducible when seed is fixed.
