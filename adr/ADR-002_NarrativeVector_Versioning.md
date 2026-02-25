ADR-002 — NarrativeVector as Versioned DTO

Status

Accepted

Context

NarrativeVectorDTO is the structural core of the IW.

Decision

NarrativeVectorDTO must be: - Immutable after export - Explicitly
versioned via schemaVersion field - Auditable - Exported only within a
structured bundle

Justification

To ensure historical traceability and future compatibility.

Consequences

-   Structural changes require a new ADR.
-   schemaVersion is mandatory.

Invariants Derived

-   DTOs cannot be mutated downstream.
