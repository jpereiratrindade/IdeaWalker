# IdeaWalker (IW) — ADR_INDEX

Este documento cataloga todos os Architecture Decision Records (ADR) ativos do IdeaWalker.

---

## Status Legend

| Status | Meaning |
|--------|---------|
| `Proposed` | Draft under review |
| `Accepted` | Normatively binding |
| `Superseded` | Replaced by another ADR |
| `Deprecated` | No longer applicable |

---

## ADR Catalog

| ID | Title | Status | File |
|----|-------|--------|------|
| ADR-000 | Governance Model Overview | Accepted | `IW_ADR-000_Governance_Model_Overview_v0_1.md` |
| ADR-001 | Narrative Observation is Non-Inferential | Accepted | `ADR-001_Narrative_Observation_Non_Inferential.md` |
| ADR-002 | NarrativeVector as Versioned DTO | Accepted | `ADR-002_NarrativeVector_Versioning.md` |
| ADR-003 | Epistemic Regime Explicit Declaration | Accepted | `ADR-003_Epistemic_Regime_Explicit.md` |
| ADR-004 | Deterministic Extraction Baseline | Accepted | `ADR-004_Deterministic_Extraction.md` |
| ADR-005 | Narrative Bundle Export Contract | Accepted | `ADR-005_Narrative_Bundle_Export_Contract.md` |
| ADR-006 | IW ↔ STRATA Integration Contract | Accepted | `ADR-006_IW_STRATA_Integration_Contract.md` |
| ADR-007 | Bifásic Scientific Ingestion Pipeline | Accepted | `ADR-007_Bifasic_Scientific_Ingestion.md` |
| ADR-008 | Structural Exclusion Rule | Accepted | `ADR-008_Structural_Exclusion_Rule.md` |
| ADR-009 | Autonomous Cognitive Orchestrator | Accepted | `ADR-009_Autonomous_Cognitive_Orchestrator.md` |
| ADR-010 | Async Task Manager | Accepted | `ADR-010_Async_Task_Manager.md` |
| ADR-011 | Contextual RAG Architecture | Accepted | `ADR-011_Contextual_RAG_Architecture.md` |

---

## Governance Rules

1. Toda decisão arquitetural relevante exige novo ADR.
2. Mudanças estruturais em DTOs exigem novo ADR.
3. Supersession deve atualizar este índice.
4. ADRs não são removidos; apenas marcados como `Superseded`.
5. O índice deve refletir apenas arquivos existentes na pasta `adr/`.

---

## Change Control

| Evento | Ação |
|--------|------|
| Criado | Adicionar neste índice |
| Atualizado | Registrar nova versão no próprio ADR |
| Substituído | Marcar como `Superseded` e apontar substituto |

---

*Fim do Documento*
