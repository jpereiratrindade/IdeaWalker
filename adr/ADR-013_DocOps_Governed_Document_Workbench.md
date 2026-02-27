# ADR-013: DocOps — Governed Document Workbench (Tab)

Status: Accepted
Date: 2026-02-27
Decision Type: Other
Supersedes: (none)
Superseded by: (none)

## Context

O IdeaWalker (IW) é document-driven e opera com governança epistemológica explícita. No ecossistema (IW/STRATA), existe uma necessidade crescente de:

- desenvolver documentos (MD/LaTeX/relatórios/ADRs) com rastreabilidade;
- executar checks (enforcement) e gerar releases (ex.: PDFs) de forma reprodutível;
- usar LLMs como interface de redação, sem permitir deriva ou invenção de evidência/bibliografia.

O módulo `Scientific` cobre ingestão científica e contratos. Faltava um espaço na UI para workflows de “DocOps” (document operations) governados: rodar checks e releases em workspaces, capturar logs e apoiar disciplina de redação sem confundir teoria com execução.

## Decision

Adicionar um novo tab na UI principal chamado **DocOps**, ao lado de `Scientific`, com objetivo inicial de:

1. selecionar um *workspace* (diretório-alvo);
2. executar comandos (presets e comando custom) no workspace;
3. registrar a saída de execução como log auditável no IW.

O DocOps atua como **orquestrador de tooling**, não como agente que altera documentos automaticamente.

## Scope / Boundaries

Aplica-se a:

- UI (painel/tab) e orquestração de execução de comandos;
- suporte operacional para documentos governados (checks/releases).

Não se aplica a:

- redefinição de teoria, ontologia ou regime epistemológico (fora de escopo);
- ingestão científica (já coberta por `ScientificIngestionService`).

## Allowed and Prohibited Flows

- Allowed:
  - executar comandos explícitos do usuário no workspace (ex.: `make check`, `make release-pdf`);
  - capturar stdout/stderr e registrar no log do IW.

- Prohibited:
  - alterar arquivos do workspace sem ação explícita do usuário;
  - gerar/atribuir bibliografia/fonte como “fato” sem referência verificável (quando/ao integrar LLM).

## Consequences and Trade-offs

- Benefícios:
  - introduz um ponto único de operação para checks/releases de projetos documentais;
  - reduz fricção para adotar enforcement em projetos teóricos (ex.: ORT) sem acoplar o IW à ontologia.

- Custos:
  - execução via shell depende do ambiente (tooling instalado no sistema);
  - saída é textual e pode exigir evolução para logs estruturados no futuro.

## Validation Criteria

- Compilação do IW inclui `src/ui/panels/DocOpsPanel.cpp`.
- O tab `DocOps` aparece na UI e permite executar pelo menos `make check` em um workspace válido.
- Saída de execução é apresentada no painel e registrada no log do IW.

## Related References

- Related ADRs:
  - ADR-010 — Async Task Manager
- Related docs:
  - `docs/DOCOPS-000_Vision_and_Boundary.md`
  - `👉 IW_Implementation_Guidelines_Post_Methodology.md`
