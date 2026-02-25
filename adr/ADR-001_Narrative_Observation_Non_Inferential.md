ADR-001 — Narrative Observation is Non-Inferential

Status

Accepted

Context

The IdeaWalker (IW) extracts structured narrative vectors from
documents.

Decision

IW operates exclusively in the observational domain. It describes
narrative structure and does not perform causal, ontological, or
decisional inference.

Justification

To preserve epistemological separation between IW (observational) and
downstream systems such as STRATA (ontological/simulational).

Consequences

-   No module may generate recommendations.
-   No component may assign decisional weight.
-   IW cannot alter external system state.

Invariants Derived

-   NarrativeVectorDTO is strictly descriptive.
-   No decision engine exists within IW.
