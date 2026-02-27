# DOCOPS-000 — Vision and Boundary (DocOpsContext)

**Status:** Active
**Data:** 2026-02-27
**Owner:** IW / STRATA ecosystem

---

## 1) Visão (o que é)

**DocOpsContext** é a camada de **produção de artefatos documentais governados** no ecossistema IW/STRATA: executar checks, gerar releases (ex.: PDF), manter rastreabilidade operacional e reduzir fricção de “engenharia documental”.

DocOps não é ontologia (ORT), não é extração evidencial (IW) e não é modelagem/simulação (STRATA). DocOps é **infraestrutura operacional** para transformar decisões e textos em artefatos auditáveis.

---

## 2) Invariantes (regras de ouro)

1. **DocOps executa regras; não decide regras.** As regras normativas vivem nos ADRs e documentos do projeto-alvo (ORT, IW, STRATA, etc.).
2. **DocOps não valida verdade.** Ele valida estrutura, rastreabilidade e reprodutibilidade (governança), não suficiência empírica.
3. **DocOps não redefine teoria.** Ele organiza produção, não altera ontologias, modelos ou regimes epistemológicos.
4. **Todo efeito deve ser auditável.** Cada operação deve deixar trilha (log, ponteiros LATEST, artefatos, exit codes).
5. **Sem “rigor performativo”.** Nada pode parecer mais verdadeiro só porque foi bem formatado; o sistema deve evitar automatismos que criem aparência de evidência.

---

## 3) Escopo e não-escopo

### Escopo (DocOps-lite)

- Selecionar um *workspace* (diretório-alvo).
- Executar comandos de tooling declarados pelo usuário (presets) no workspace.
- Capturar stdout/stderr e exit code.
- Facilitar workflows como:
  - `make check`
  - `make release-*`
  - inspeção de status (`git status --porcelain`)

### Não-escopo (proibido nesta fase)

- Classificar “força de claim” (empírico vs hipótese) automaticamente.
- Verificar validade de citação bibliográfica como “verdade”.
- Bloquear releases por “inconsistência epistemológica” (sem ADR próprio e sem regime explícito).
- Reescrever documentos por conta própria.

---

## 4) Hierarquia correta (para evitar inversão)

```
ORT     → produz teoria / ontologia e governa compromissos
DocOps  → organiza produção de artefatos (checks/releases/logs)
IW      → extrai e estrutura evidência (não-inferencial por padrão)
STRATA  → modela, integra, simula e operacionaliza contratos
```

DocOps é infraestrutura. ORT/IW/STRATA são fontes normativas do “o que significa”.

---

## 5) Bounded Context (interfaces)

DocOpsContext deve ser tratado como um Bounded Context separado para evitar acoplamento com o core do IW.

Interfaces mínimas:

- **Entrada:** caminho do workspace + comando/preset.
- **Saída:** log de execução + exit code + (quando aplicável) caminho do artefato gerado.

Integrações futuras (somente com ADR):

- catálogo de workspaces (presets por projeto);
- logs estruturados (JSON) em vez de apenas texto;
- exportação de “proveniência operacional” para STRATA.

---

## 6) Riscos reconhecidos (e mitigação)

1. **Centralização normativa silenciosa:** DocOps virar árbitro epistemológico.
   - Mitigação: manter invariantes e exigir ADR específico para qualquer regra semântica.
2. **Confusão governança vs verdade:** checks estruturais virarem “validação científica”.
   - Mitigação: linguagem explícita na UI e documentação; logs informam “passou no check”, não “está correto”.
3. **Crescimento oculto:** “só um tab” virar motor epistêmico sem regime.
   - Mitigação: faseamento + ADR-gates para evolução (seção 7).

---

## 7) Faseamento de maturidade (disciplina incremental)

### Fase 1 — DocOps-lite (agora)

- Execucao governada de checks/releases com logs e rastreabilidade operacional.
- Nenhuma validação epistemológica do conteúdo.
- Workspace contract minimo (`docops/docops.yaml` + `profiles` + `templates`).

### Fase 2 — DocOps com contratos de artefato (somente com ADR)

- Definir contratos “machine-readable” por tipo de artefato (ex.: release PDF, changelog, ponteiros LATEST).
- Checks adicionais apenas quando houver especificação explícita no workspace.

### Fase 3 — DocOps epistêmico (somente com ADR próprio)

Se (e somente se) o ecossistema decidir avancar para tipagem de assertivas e estados de claim, criar um ADR dedicado (ex.: `ADR-018_Assertive_Typing_and_Citation_States.md`) explicitando:

- tipos de assertiva (definição, hipótese, claim empírico, política interna, derivado de ADR);
- status de citação (verificada, sugerida, ausente);
- limites de automação (o que o sistema pode marcar vs o que exige validação humana).

---

## 8) Exemplos de uso (workspace)

- ORT (ontologia/documentos): rodar `make check`, gerar `make release-rtv-pdf`, atualizar ponteiros `LATEST-*`.
- Projetos científicos: rodar checagens formais, build de LaTeX, e gerar releases reprodutíveis.

---

## 9) Referências

- ADR (IW): `adr/ADR-013_DocOps_Governed_Document_Workbench.md`
- Contratos DocOps:
  - `adr/ADR-015_DocOps_Workspace_Contract.md`
  - `adr/ADR-016_DocOps_Prompt_Profile_Registry.md`
  - `adr/ADR-017_DocOps_Controlled_Edit_Protocol.md`
- Diretrizes de implementação: `👉 IW_Implementation_Guidelines_Post_Methodology.md`
- Doc referência de governança: `adr/ADR-000_Governance_Model_Overview.md`
