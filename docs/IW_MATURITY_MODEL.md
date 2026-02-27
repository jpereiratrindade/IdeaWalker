# IdeaWalker (IW)
## Modelo de Maturidade Institucional — IW Maturity Model

**Versao:** 1.4  
**Status:** Accepted  
**Data:** 2026-02-27  
**Revisao:** Atualizado para incluir DocOps-lite como grupo de hardening estrutural (F1.E).

---

## Proposito

Este documento define o modelo formal de maturidade do IdeaWalker, com criterios objetivos de entrada e saida por fase.

> Distincao fundamental:
>
> - ADR = decisao arquitetural normativa (o que e por que)
> - Fase de maturidade = nivel de enforcement executavel dessas decisoes

---

## Modelo de Fases

```
F0 -> Fundacao Normativa
F1 -> Hardening Estrutural
F2 -> Hardening Semantico e Contrato de IA
```

---

## F0 — Fundacao Normativa

### Definicao

Decisoes arquiteturais e epistemologicas formalizadas, com identidade institucional documentada.

### Criterios de Saida

| # | Criterio | Verificacao | Status |
|---|----------|-------------|--------|
| F0.1 | ADR-000 a ADR-013 existentes em `adr/` | `find adr/ -name "*.md" \| grep -v INDEX \| grep -v TEMPLATE \| grep -v README` | ✅ Verificado |
| F0.2 | `ADR_INDEX.md` lista todos os ADRs do diretorio | `bash scripts/audit_adr_index.sh` | ✅ Verificado |
| F0.3 | `IW_INVARIANT_MATRIX.md` consolida invariantes | Inspecao documental | ✅ Verificado |
| F0.4 | `ADR_TEMPLATE.md` padroniza novos ADRs | `ls adr/ADR_TEMPLATE.md` | ✅ Verificado |
| F0.5 | Estrutura DDD (`src/domain`, `src/application`, `src/infrastructure`, `src/ui`) | `ls src/` | ✅ Verificado |

### Status

✅ **F0 CONCLUIDO**

---

## F1 — Hardening Estrutural

### Definicao

Invariantes declarados nos ADRs sao executados por codigo, testes e gates de CI.

### Criterios de Saida

#### Grupo A — Integridade de DTOs

| # | Criterio | ADR | Verificacao | Status |
|---|----------|-----|-------------|--------|
| F1.A1 | `schemaVersion` presente e nao vazio em todo `NarrativeVectorDTO` exportado | ADR-002 | `NarrativeBundleTest::Test_F1_A1_SchemaVersionPresent` | ✅ Verificado |
| F1.A2 | `narrativeRegime` presente em todo `NarrativeVectorDTO` | ADR-003 | `NarrativeBundleTest::Test_F1_A2_NarrativeRegimePresent` | ✅ Verificado |
| F1.A3 | `toolchain` metadata presente em toda extracao | ADR-004 | `NarrativeBundleTest::Test_F1_A3_ToolchainSourcePresent` | ✅ Verificado |
| F1.A4 | Nenhum export sem `NarrativeBundle` encapsulando DTO | ADR-005 | `NarrativeBundleTest::Test_F1_A4_ExportOnlyStructuredBundleArtifacts` | ✅ Verificado |

#### Grupo B — Integridade de Pipeline

| # | Criterio | ADR | Verificacao | Status |
|---|----------|-----|-------------|--------|
| F1.B1 | `STRUCTURAL_EXCLUSION_THRESHOLD` e constante nomeada | ADR-008 | `grep "STRUCTURAL_EXCLUSION_THRESHOLD" src/` | ✅ Verificado |
| F1.B2 | Log de exclusao estrutural escrito em toda extracao PDF | ADR-008 | `ScientificResilienceTest::Test_F1_B2_ExclusionLogging` | ✅ Verificado |
| F1.B3 | `ContextAssembler` existe como componente isolado em `src/application/` | ADR-011 | `ls src/application/ContextAssembler.*` | ✅ Verificado |
| F1.B4 | Nenhum `std::thread` direto em servicos/UI fora `AsyncTaskManager` | ADR-010 | Gate CI F1.D3 | ✅ Verificado |

#### Grupo C — Ingestao Cientifica

| # | Criterio | ADR | Verificacao | Status |
|---|----------|-----|-------------|--------|
| F1.C1 | Pipeline bifasico executa 2 chamadas de IA por documento | ADR-007 | `NarrativeBundleTest::Test_F1_C1_BifasicTwoCallsPerDocument` | ✅ Verificado |
| F1.C2 | Ausencia de `DiscursiveContext` nao bloqueia pipeline | ADR-007 | `ScientificResilienceTest::Test_F1_C2_DiscursiveFailureNotBlocking` | ✅ Verificado |
| F1.C3 | `NarrativeObservation.json` e `DiscursiveContext.json` sempre separados | ADR-007 | `NarrativeBundleTest::Test_F1_C3_NarrativeAndDiscursiveArtifactsSeparated` | ✅ Verificado |

#### Grupo D — CI/CD Enforcement

| # | Criterio | Verificacao | Status |
|---|----------|-------------|--------|
| F1.D1 | Pipeline CI configurado | `.github/workflows/ci.yml` | ✅ Verificado |
| F1.D2 | CI executa suite de testes a cada push/PR | `build-tests` + `run-tests` | ✅ Verificado |
| F1.D3 | CI falha com `std::thread` direto em `src/application`/`src/ui` | Gate `Invariant Audit` | ✅ Verificado |
| F1.D4 | CI valida consistencia `ADR_INDEX.md` x `adr/` | `scripts/audit_adr_index.sh` | ✅ Verificado |
| F1.D5 | CI valida catalogo ADR gerado atualizado | `scripts/build_adr_catalog.py` + `git status` gate | ✅ Verificado |

#### Grupo E — DocOps-lite (Bounded Context operacional)

| # | Criterio | ADR | Verificacao | Status |
|---|----------|-----|-------------|--------|
| F1.E1 | `DocOpsContext` formalizado com fronteiras explicitas de escopo/nao-escopo | ADR-013 | `docs/DOCOPS-000_Vision_and_Boundary.md` | ✅ Verificado |
| F1.E2 | Tab `DocOps` existe na UI principal ao lado de `Scientific` | ADR-013 | `src/ui/panels/TabOrchestrator.cpp` | ✅ Verificado |
| F1.E3 | Execucao de comandos em workspace captura output e `exit code` auditavel | ADR-013 | `src/ui/panels/DocOpsPanel.cpp` | ✅ Verificado |
| F1.E4 | DocOps permanece nao-epistemico (sem validacao de verdade/claim-strength) | ADR-013 + DOCOPS-000 | Inspecao de codigo + fronteiras documentadas | ✅ Verificado |

### Status

✅ **F1 CONCLUIDO** (todos os criterios A, B, C, D e E atendidos com evidencia executavel)

---

## F2 — Hardening Semantico e Contrato de IA

### Definicao

Contrato semantico explicito no pipeline de IA, com fail-fast em export para valores invalidos.

### Criterios de Entrada

- F1 concluido (todos os grupos A, B, C, D e E satisfeitos).

### Status

🟡 **F2 DESBLOQUEADO** (entrada permitida; execucao ainda nao iniciada)

---

## Historico de Fechamento F1 (2026-02-25)

| Grupo | Item encerrado | Evidencia |
|------|----------------|-----------|
| A | Contratos de bundle (`schemaVersion`, `narrativeRegime`, `toolchain`) normalizados antes da validacao | `NarrativeBundleTest` (A1-A3) |
| A | Regressao de export consolidada para somente artefatos estruturados | `NarrativeBundleTest` (A4) |
| B | Log de exclusao estrutural tornado auditavel/testavel por API dedicada | `ScientificResilienceTest` (B2) |
| C | Pipeline bifasico estabilizado em 2 chamadas base por documento | `NarrativeBundleTest` (C1) |
| C | Resiliencia com falha discursiva sem bloquear bundle | `ScientificResilienceTest` (C2) |
| C | Garantia de separacao entre `NarrativeObservation.json` e `DiscursiveContext.json` | `NarrativeBundleTest` (C3) |

## Historico de Evolucao DocOps (2026-02-27)

| Grupo | Item encerrado | Evidencia |
|------|----------------|-----------|
| E | Tab `DocOps` introduzido como workbench operacional (workspace + presets + log/exit code) | `src/ui/panels/DocOpsPanel.cpp` |
| E | Fronteiras de Bounded Context documentadas (DocOps-lite, nao-epistemico) | `docs/DOCOPS-000_Vision_and_Boundary.md` |
| E | Governanca formal do contexto via ADR | `adr/ADR-013_DocOps_Governed_Document_Workbench.md` |

---

## Regras de Governanca

1. Nenhuma fase pode ser declarada concluida sem verificacao objetiva de todos os criterios.
2. Gates automaticos tem precedencia sobre inspecao manual.
3. Novos ADRs so contam para maturidade quando houver enforcement executavel.
4. Este documento deve ser atualizado a cada alteracao de status de criterio.

---

*Fim do Documento — v1.4*
