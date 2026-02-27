# ADR Index

This directory is the canonical registry for Architecture Decision Records (ADRs) in IdeaWalker.

Rules:

- ADRs are immutable historical records.
- ADR files are numbered sequentially: `ADR-000`, `ADR-001`, ...
- ADRs are never deleted.
- If replaced, mark status as `Superseded` and reference the newer ADR.
- Keep one ADR per architectural decision.

Status vocabulary:

- `Proposed`
- `Accepted`
- `Rejected`
- `Superseded`
- `Deprecated`

Current ADRs:

- [ADR-000_Governance_Model_Overview.md](ADR-000_Governance_Model_Overview.md)
- [ADR-001_Narrative_Observation_Non_Inferential.md](ADR-001_Narrative_Observation_Non_Inferential.md)
- [ADR-002_NarrativeVector_Versioning.md](ADR-002_NarrativeVector_Versioning.md)
- [ADR-003_Epistemic_Regime_Explicit.md](ADR-003_Epistemic_Regime_Explicit.md)
- [ADR-004_Deterministic_Extraction.md](ADR-004_Deterministic_Extraction.md)
- [ADR-005_Narrative_Bundle_Export_Contract.md](ADR-005_Narrative_Bundle_Export_Contract.md)
- [ADR-006_IW_STRATA_Integration_Contract.md](ADR-006_IW_STRATA_Integration_Contract.md)
- [ADR-007_Bifasic_Scientific_Ingestion.md](ADR-007_Bifasic_Scientific_Ingestion.md)
- [ADR-008_Structural_Exclusion_Rule.md](ADR-008_Structural_Exclusion_Rule.md)
- [ADR-009_Autonomous_Cognitive_Orchestrator.md](ADR-009_Autonomous_Cognitive_Orchestrator.md)
- [ADR-010_Async_Task_Manager.md](ADR-010_Async_Task_Manager.md)
- [ADR-011_Contextual_RAG_Architecture.md](ADR-011_Contextual_RAG_Architecture.md)
- [ADR-012_Semantic_States_and_Contracts.md](ADR-012_Semantic_States_and_Contracts.md)
- [ADR-013_DocOps_Governed_Document_Workbench.md](ADR-013_DocOps_Governed_Document_Workbench.md)
- [ADR-014_Internal_Evaluations_Registry.md](ADR-014_Internal_Evaluations_Registry.md)
- [ADR-015_DocOps_Workspace_Contract.md](ADR-015_DocOps_Workspace_Contract.md)
- [ADR-016_DocOps_Prompt_Profile_Registry.md](ADR-016_DocOps_Prompt_Profile_Registry.md)
- [ADR-017_DocOps_Controlled_Edit_Protocol.md](ADR-017_DocOps_Controlled_Edit_Protocol.md)

Normative governance reference:

- [ADR-000_Governance_Model_Overview.md](ADR-000_Governance_Model_Overview.md)

ADR metadata extraction (analysis layer, read-only):

- Command: `python3 scripts/build_adr_catalog.py`
- Outputs:
  - `reports/architecture/ArchitectureDecisionIndex.latest.json`
  - `reports/architecture/ArchitectureDecisionIndex.latest.md`
- Optional timestamped outputs:
  - `python3 scripts/build_adr_catalog.py --with-stamped`

ADR inclusion workflow (aligned with SisterSTRATA):

1. Create a new ADR file from `adr/ADR_TEMPLATE.md` using the next sequential ID.
2. Set `Status` to `Proposed` while the decision is under review.
3. Add the new ADR entry to `adr/ADR_INDEX.md`.
4. Run `bash scripts/audit_adr_index.sh` (disk/index bidirectional consistency gate).
5. Run `python3 scripts/build_adr_catalog.py` (regenerate governance catalog artifacts).
6. Once approved by maintainers, change `Status` to `Accepted`.
