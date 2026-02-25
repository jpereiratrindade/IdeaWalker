# IdeaWalker (IW)
## Invariant Matrix

**Data:** 2026-02-25  
**Status:** Active governance artifact (F1 concluido)

Este documento consolida invariantes derivados dos ADRs e mapeia evidencia executavel.

| Invariante | Origem (ADR) | Evidencia executavel | Gate CI / Auditoria | Status |
|---|---|---|---|---|
| IW e exclusivamente observacional (sem inferencia causal/decisional). | ADR-001 | Regras de prompt + contrato de export | Revisao de ADR + testes de regressao de bundle | 🟡 Parcial |
| `NarrativeVectorDTO` deve ter `schemaVersion` valido. | ADR-002 | `src/test/NarrativeBundleTest.cpp` | `run-tests` (`ideawalker_bundle_test`) | ✅ Protegido |
| `narrativeRegime` e obrigatorio em observacoes narrativas. | ADR-003 | `src/test/NarrativeBundleTest.cpp` | `run-tests` (`ideawalker_bundle_test`) | ✅ Protegido |
| Metadados de toolchain sao obrigatorios na extracao. | ADR-004 | `src/test/NarrativeBundleTest.cpp` | `run-tests` (`ideawalker_bundle_test`) | ✅ Protegido |
| Exportacao ocorre somente via `NarrativeBundle`. | ADR-005 | `src/test/NarrativeBundleTest.cpp` (F1.A4) | `run-tests` (`ideawalker_bundle_test`) | ✅ Protegido |
| Integracao IW->STRATA preserva contrato sem texto cru. | ADR-006 | Estrutura de artefatos consumiveis | Validacao de pipeline cientifico | 🟡 Parcial |
| Ingestao cientifica e bifasica (narrativa + discursiva) com 2 chamadas por documento. | ADR-007 | `src/test/NarrativeBundleTest.cpp` | `run-tests` (`ideawalker_bundle_test`) | ✅ Protegido |
| Exclusao estrutural usa threshold nomeado (sem magic number). | ADR-008 | `src/infrastructure/ContentExtractor.hpp` | `Invariant Audit` (F1.B1) | ✅ Protegido |
| Trabalho assincorno nao pode criar `std::thread` direto em servicos/UI fora do gerenciador central. | ADR-010 | `src/application/ConversationService.cpp` migrado para `AsyncTaskManager` | `Invariant Audit` (F1.D3) | ✅ Protegido |
| Contexto e retrieval seguem papeis explicitos (RAG contextual). | ADR-011 | `ContextAssembler` + contratos de contexto | Revisao de arquitetura e testes de contexto | ✅ Verificado |
| Metadados semanticos de estado de extracao e LAA sao separados do bundle institucional. | ADR-012 | `ScientificIngestionService` + logs `.exclusions.log` | `ScientificResilienceTest` (F1.B2/F1.C2) | ✅ Protegido |
| `ADR_INDEX.md` reflete exatamente arquivos do diretorio `adr/`. | ADR-000 | `scripts/audit_adr_index.sh` | `Invariant Audit` (F1.D4) | ✅ Protegido |
| Catalogo ADR gerado esta sincronizado com os ADRs canonicos. | ADR-000 | `scripts/build_adr_catalog.py` | `Invariant Audit` (F1.D5) | ✅ Protegido |

## Artefatos de Governanca Relacionados

- `adr/ADR_INDEX.md`
- `adr/ADR_TEMPLATE.md`
- `scripts/audit_adr_index.sh`
- `scripts/build_adr_catalog.py`
- `reports/architecture/ArchitectureDecisionIndex.latest.json`
- `reports/architecture/ArchitectureDecisionIndex.latest.md`

## Nota de Operacao

Invariante declarado so e considerado protegido quando existe evidencia executavel recorrente (teste/gate CI), nao apenas documentacao.
