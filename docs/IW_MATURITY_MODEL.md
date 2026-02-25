# IdeaWalker (IW)
## Modelo de Maturidade Institucional — IW Maturity Model

**Versão:** 1.1  
**Status:** Accepted  
**Data:** 2026-02-25  
**Revisão:** Incorpora auditoria concreta de 2026-02-25 e feedback de review institucional.

---

## Propósito

Este documento define o **modelo formal de maturidade do IdeaWalker (IW)**, com critérios
objetivos e verificáveis de entrada e saída para cada fase.

> **Distinção fundamental**
>
> - **ADR** = decisão arquitetural normativa (o quê e por quê)
> - **Fase de Maturidade** = nível de enforcement dessas decisões
>
> ADRs existentes não implicam maturidade alcançada.
> Maturidade é verificada por enforcement, não por declaração.

---

## Política de Declaração de Fase

> Uma fase só pode ser declarada **concluída** após verificação objetiva de **todos** os
> critérios de saída. Declaração baseada em intenção é inválida.
>
> Critérios com verificação automática têm precedência sobre inspeção manual.
> Inspeção manual é aceitável em F0; não é aceitável em F1+.

---

## Modelo de Fases

```
F0 → Fundação Normativa
F1 → Hardening Estrutural
F2 → Hardening Semântico e Contrato de IA
```

---

## F0 — Fundação Normativa

### Definição

Decisões arquiteturais e epistemológicas formalizadas. Sistema com identidade documentada.
Sem enforcement automático (aceitável nesta fase).

### Critérios de Saída

| # | Critério | Verificação | Status |
|---|----------|-------------|--------|
| F0.1 | ADR-000 a ADR-011 existem na pasta `adr/` com status `Accepted` | `find adr/ -name "*.md" \| grep -v INDEX \| grep -v TEMPLATE \| grep -v README` | ✅ Verificado |
| F0.2 | `ADR_INDEX.md` lista todos os ADRs do diretório | Inspeção manual | ✅ Verificado |
| F0.3 | `IW_INVARIANT_MATRIX.md` lista invariantes derivados dos ADRs | Inspeção manual | ✅ Verificado |
| F0.4 | `ADR_TEMPLATE.md` padroniza criação de novos ADRs | `ls adr/ADR_TEMPLATE.md` | ✅ Verificado |
| F0.5 | Estrutura DDD: `src/domain`, `src/application`, `src/infrastructure`, `src/ui` existem | `ls src/` | ✅ Verificado |

### ⚠️ Anomalia Registrada

O arquivo `ADR-000` usa nomenclatura diferente dos demais:
`IW_ADR-000_Governance_Model_Overview_v0_1.md` vs. padrão `ADR-XXX_*.md`.

**Impacto:** F0 permanece concluído (arquivo existe e está `Accepted`), mas a inconsistência
de nomenclatura é um débito de F1 — o `ADR_INDEX.md` deve referenciar o nome exato.

### ✅ Status: **F0 CONCLUÍDO** (auditado em 2026-02-25)

---

## F1 — Hardening Estrutural

### Definição

Invariantes declarados nos ADRs têm enforcement verificável no código e em testes.
Nenhum invariante pode ser violado silenciosamente.

### Critérios de Entrada

- F0 concluído e auditado. ✅

### Critérios de Saída

#### Grupo A — Integridade de DTOs

| # | Critério | ADR | Verificação | Status |
|---|----------|-----|-------------|--------|
| F1.A1 | `schemaVersion` presente e não vazio em todo `NarrativeVectorDTO` exportado | ADR-002 | Teste unitário | 🔴 Ausente |
| F1.A2 | `narrativeRegime` presente em todo `NarrativeVectorDTO` | ADR-003 | Teste unitário | 🔴 Ausente |
| F1.A3 | `toolchain` metadata presente em toda extração | ADR-004 | Teste unitário | 🔴 Ausente |
| F1.A4 | Nenhum export sem `NarrativeBundle` encapsulando o DTO | ADR-005 | Teste de integração | 🔴 Ausente |

#### Grupo B — Integridade de Pipeline

| # | Critério | ADR | Verificação | Status |
|---|----------|-----|-------------|--------|
| F1.B1 | `STRUCTURAL_EXCLUSION_THRESHOLD` é constante nomeada (não magic number) | ADR-008 | `grep "STRUCTURAL_EXCLUSION_THRESHOLD" src/` | 🔴 Não verificado |
| F1.B2 | Log de exclusão estrutural escrito em toda extração de PDF | ADR-008 | Teste de integração | 🔴 Ausente |
| F1.B3 | `ContextAssembler` existe como componente isolado em `src/application/` | ADR-011 | `ls src/application/ContextAssembler.*` | ✅ **Verificado** |
| F1.B4 | Nenhum `std::thread` direto em camadas de serviço ou UI | ADR-010 | `grep -r "std::thread" src/application src/ui` → vazio | 🔴 **FALHA** — 3 ocorrências |

> **F1.B4 — Violações concretas identificadas:**
> - `src/ui/AppState.cpp` linhas 180 e 259
> - `src/application/ConversationService.cpp` linha 60
>
> `PersistenceService` e `WhisperCppAdapter` (infraestrutura) são exceções aceitáveis
> (adapters de I/O têm permissão de gerenciar seu próprio worker thread).

#### Grupo C — Ingestão Científica

| # | Critério | ADR | Verificação | Status |
|---|----------|-----|-------------|--------|
| F1.C1 | `ScientificIngestionService` executa exatamente 2 invocações de IA por documento | ADR-007 | Teste unitário + log de auditoria | 🔴 Ausente |
| F1.C2 | Ausência de `DiscursiveContext` não bloqueia pipeline | ADR-007 | Teste com fixture sem frames | 🔴 Ausente |
| F1.C3 | `NarrativeObservation.json` e `DiscursiveContext.json` são sempre artefatos separados | ADR-007 | Inspeção de output | 🔴 Não verificado |

#### Grupo D — CI/CD Enforcement

| # | Critério | Verificação | Status |
|---|----------|-------------|--------|
| F1.D1 | Pipeline CI configurado (GitHub Actions ou equivalente) | `.github/workflows/` existe | 🔴 **AUSENTE** |
| F1.D2 | CI executa suite de testes a cada push | Inspeção do workflow | 🔴 Ausente |
| F1.D3 | CI falha se `grep -r "std::thread" src/application src/ui` retorna resultado | Script no workflow | 🔴 Ausente |
| F1.D4 | CI valida estrutura da pasta `adr/` (todos os ADRs do INDEX existem como arquivo) | Script de auditoria ADR | 🔴 Ausente |

> **Nota crítica:** Sem CI, F1 é **F1 conceitual** — depende do desenvolvedor lembrar de
> rodar testes. Enforcement real exige mecanismo institucional automático.

### Status: 🔴 **F1 EM ANDAMENTO** (declarado em 2026-02-25)

---

## F2 — Hardening Semântico e Contrato de IA

### Definição

O pipeline de IA é governado por contrato semântico explícito.
Valores inválidos são **rejeitados** antes do export. O LLM não pode introduzir
corrupção estrutural silenciosa.

### Critérios de Entrada

- F1 concluído (todos os grupos A, B, C e D satisfeitos).

### Critérios de Saída

#### Grupo E — Contrato Semântico

| # | Critério | Verificação |
|---|----------|-------------|
| F2.E1 | ADR-012 (AI Prompt Semantic Contract) criado e `Accepted` | Arquivo em `adr/` |
| F2.E2 | Prompts exigem escolha única para campos enum (sem `\|`, sem listas) | Inspeção de prompts |
| F2.E3 | `contextuality` é campo obrigatório em `narrativeObservations` | Validação pré-export |

#### Grupo F — Fail-Fast no Export (Modo Estrito)

> **Política de dois modos:**
> - **Modo estrito** (padrão científico): bundle com enum inválido é **rejeitado** com erro explícito.
> - **Modo tolerante** (exploratório): enum inválido é normalizado para `unknown` com warning
>   registrado e bundle marcado como `COERCED`.
>
> O modo estrito é obrigatório para export destinado ao STRATA.
> O modo tolerante é permitido apenas para uso exploratório local.
> **Nunca selecionar silenciosamente o primeiro valor.**

| # | Critério | Verificação |
|---|----------|-------------|
| F2.F1 | Detector de enum inválido implementado (valor contendo `\|`) | Teste: `env: "a\|b"` → erro explícito em modo estrito |
| F2.F2 | Modo tolerante normaliza `\|` → `unknown` + warning no log + marca `COERCED` | Teste unitário |
| F2.F3 | Modo estrito rejeita bundle; modo tolerante emite warning — nunca coerção silenciosa | Teste unitário |
| F2.F4 | Teste que **falha** se qualquer campo enum contiver valor fora do domínio | CI/CD bloqueante |
| F2.F5 | STRATA nunca recebe bundle com enum malformado | Teste end-to-end |

### Status: 🔴 **F2 BLOQUEADO** (aguarda F1)

---

## Diagrama de Transições

```
┌─────────────────────────────────────────────────────────┐
│  F0 ──────────────────────────────────────── CONCLUÍDO  │
│  Fundação Normativa                                     │
│  Auditado em: 2026-02-25                               │
└─────────────────────────────────────────────────────────┘
                          │
                          ▼
┌─────────────────────────────────────────────────────────┐
│  F1 ────────────────────────────────── EM ANDAMENTO     │
│  Hardening Estrutural                                   │
│  Iniciado em: 2026-02-25                               │
│  Pendências: F1.A1–A4, F1.B1/B2/B4, F1.C1–C3,         │
│              F1.D1–D4 (CI)                             │
└─────────────────────────────────────────────────────────┘
                          │
                          ▼
┌─────────────────────────────────────────────────────────┐
│  F2 ─────────────────────────────────────── BLOQUEADO   │
│  Hardening Semântico e Contrato de IA                   │
│  Bloqueio: F1 deve estar 100% concluído                │
└─────────────────────────────────────────────────────────┘
```

---

## Backlog de F1 — Priorizado

| Prioridade | Grupo | Item | Critério |
|-----------|-------|------|----------|
| 🔴 Alta | D | Configurar CI (GitHub Actions) com execução de testes | F1.D1–D2 |
| 🔴 Alta | B | Migrar `std::thread` direto de `AppState.cpp` e `ConversationService.cpp` para `AsyncTaskManager` | F1.B4 |
| 🔴 Alta | A | Testes unitários: `NarrativeBundle` (schemaVersion, narrativeRegime, toolchain) | F1.A1–A3 |
| 🔴 Alta | D | Script CI: audit de `std::thread` em serviços e UI | F1.D3 |
| � Média | B | Nomear `STRUCTURAL_EXCLUSION_THRESHOLD` como constante | F1.B1 |
| 🟡 Média | C | Teste: ingestão bifásica executa exatamente 2 invocações | F1.C1 |
| 🟡 Média | D | Script CI: valida consistência de `ADR_INDEX.md` com arquivos em `adr/` | F1.D4 |
| 🟢 Baixa | B | Log de exclusão estrutural em PDF | F1.B2 |
| 🟢 Baixa | C | Teste: pipeline tolerante a `DiscursiveContext` vazio | F1.C2 |

---

## Regras de Governança

1. Nenhuma fase é declarada concluída sem que **todos** os critérios estejam verificados.
2. Critérios automáticos têm precedência sobre inspeção manual.
3. Novos ADRs não alteram a fase atual; seu enforcement é incorporado na fase corrente ou na próxima.
4. Este documento é revisado e versionado a cada transição de fase.
5. Transição de fase é **decisão institucional explícita**, não estado emergente.

---

*Fim do Documento — v1.1*
