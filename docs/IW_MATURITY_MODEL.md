# IdeaWalker (IW)
## Modelo de Maturidade Institucional — IW Maturity Model

**Versao:** 1.2  
**Status:** Accepted  
**Data:** 2026-02-25  
**Revisao:** Atualizado apos hardening de governanca ADR e CI.

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
| F0.1 | ADR-000 a ADR-012 existentes em `adr/` | `find adr/ -name "*.md" \| grep -v INDEX \| grep -v TEMPLATE \| grep -v README` | ✅ Verificado |
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
| F1.A1 | `schemaVersion` presente e nao vazio em todo `NarrativeVectorDTO` exportado | ADR-002 | `NarrativeBundleTest` | 🔴 FALHA |
| F1.A2 | `narrativeRegime` presente em todo `NarrativeVectorDTO` | ADR-003 | `NarrativeBundleTest` | 🔴 FALHA |
| F1.A3 | `toolchain` metadata presente em toda extracao | ADR-004 | `NarrativeBundleTest` | 🔴 FALHA |
| F1.A4 | Nenhum export sem `NarrativeBundle` encapsulando DTO | ADR-005 | Teste de integracao dedicado | 🟡 Nao verificado |

#### Grupo B — Integridade de Pipeline

| # | Criterio | ADR | Verificacao | Status |
|---|----------|-----|-------------|--------|
| F1.B1 | `STRUCTURAL_EXCLUSION_THRESHOLD` e constante nomeada | ADR-008 | `grep "STRUCTURAL_EXCLUSION_THRESHOLD" src/` | ✅ Verificado |
| F1.B2 | Log de exclusao estrutural escrito em toda extracao PDF | ADR-008 | `ScientificResilienceTest` + fixture real PDF | 🟡 Parcial |
| F1.B3 | `ContextAssembler` existe como componente isolado em `src/application/` | ADR-011 | `ls src/application/ContextAssembler.*` | ✅ Verificado |
| F1.B4 | Nenhum `std::thread` direto em servicos/UI fora `AsyncTaskManager` | ADR-010 | Gate CI F1.D3 | ✅ Verificado |

#### Grupo C — Ingestao Cientifica

| # | Criterio | ADR | Verificacao | Status |
|---|----------|-----|-------------|--------|
| F1.C1 | Pipeline bifasico executa 2 chamadas de IA por documento | ADR-007 | `NarrativeBundleTest` | 🔴 FALHA |
| F1.C2 | Ausencia de `DiscursiveContext` nao bloqueia pipeline | ADR-007 | `ScientificResilienceTest` | 🔴 FALHA |
| F1.C3 | `NarrativeObservation.json` e `DiscursiveContext.json` sempre separados | ADR-007 | Teste de integracao de artefatos | 🟡 Nao verificado |

#### Grupo D — CI/CD Enforcement

| # | Criterio | Verificacao | Status |
|---|----------|-------------|--------|
| F1.D1 | Pipeline CI configurado | `.github/workflows/ci.yml` | ✅ Verificado |
| F1.D2 | CI executa suite de testes a cada push/PR | `build-tests` + `run-tests` | ✅ Verificado |
| F1.D3 | CI falha com `std::thread` direto em `src/application`/`src/ui` | Gate `Invariant Audit` | ✅ Verificado |
| F1.D4 | CI valida consistencia `ADR_INDEX.md` x `adr/` | `scripts/audit_adr_index.sh` | ✅ Verificado |
| F1.D5 | CI valida catalogo ADR gerado atualizado | `scripts/build_adr_catalog.py` + `git status` gate | ✅ Verificado |

### Status

🔴 **F1 EM ANDAMENTO** (gates de governanca ativos; pendencias funcionais em A/C e validacoes de B2/C3)

---

## F2 — Hardening Semantico e Contrato de IA

### Definicao

Contrato semantico explicito no pipeline de IA, com fail-fast em export para valores invalidos.

### Criterios de Entrada

- F1 concluido (todos os grupos A, B, C e D satisfeitos).

### Status

🔴 **F2 BLOQUEADO**

---

## Backlog Prioritario (F1)

| Prioridade | Grupo | Item | Criterio |
|-----------|-------|------|----------|
| 🔴 Alta | A | Corrigir contratos de bundle para passar F1.A1/F1.A2/F1.A3 | F1.A1–A3 |
| 🔴 Alta | C | Ajustar pipeline bifasico e estrategia de fallback | F1.C1 |
| 🔴 Alta | C | Garantir resiliencia: bundle mesmo com falha discursiva | F1.C2 |
| 🟡 Media | A | Adicionar teste de regressao para export fora de `NarrativeBundle` | F1.A4 |
| 🟡 Media | C | Teste automatico de separacao de artefatos narrativa/discursivo | F1.C3 |
| 🟡 Media | B | Fechar validacao automatica de log de exclusao estrutural com fixture PDF real | F1.B2 |

---

## Regras de Governanca

1. Nenhuma fase pode ser declarada concluida sem verificacao objetiva de todos os criterios.
2. Gates automaticos tem precedencia sobre inspecao manual.
3. Novos ADRs so contam para maturidade quando houver enforcement executavel.
4. Este documento deve ser atualizado a cada alteracao de status de criterio.

---

*Fim do Documento — v1.2*
