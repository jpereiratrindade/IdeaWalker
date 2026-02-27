# ADR-017: DocOps Controlled Edit Protocol

Status: Accepted
Date: 2026-02-27
Decision Type: Hardening
Supersedes: (none)
Superseded by: (none)

## Context

A ideia de assistencia textual com LLM no DocOps exige uma fronteira clara entre sugestao e alteracao real de arquivos. Precisamos evitar escrita opaca "direto no documento" sem trilha de revisao.

Sem protocolo de edicao:

- alteracoes ficam sem rastreabilidade de intencao;
- aumenta risco de mudanca silenciosa em documentos normativos;
- reduz auditabilidade de autoria e revisao.

## Decision

Adotar protocolo de edicao controlada em quatro etapas:

1. **Plan**: definir alvo, objetivo e perfil.
2. **Propose**: gerar proposta textual com diff/patch.
3. **Review**: revisao humana explicita.
4. **Apply**: aplicar alteracao somente apos aprovacao.

Regras normativas:

- nenhuma alteracao persistente sem aprovacao humana;
- mudancas estruturais exigem referencia a ADR aplicavel;
- toda acao deve registrar log operacional.

## Scope / Boundaries

Aplica-se a:

- fluxos de assistencia de escrita do DocOps.

Nao se aplica a:

- execucao de comandos de check/release sem edicao de arquivo;
- decisao final de aceitacao cientifica/epistemologica de conteudo.

## Allowed and Prohibited Flows

- Allowed:
  - propor alteracoes como patch revisavel;
  - rejeitar aplicacao sem perda de historico da proposta.

- Prohibited:
  - alteracao direta silenciosa por LLM;
  - merge automatico de patch sem confirmacao humana.

## Consequences and Trade-offs

- Beneficios:
  - rastreabilidade completa de edicoes assistidas;
  - menor risco de alteracoes indevidas;
  - alinhamento com governanca ADR-first.

- Custos:
  - fluxo de escrita mais deliberado (menos imediato);
  - necessidade de UI/UX clara para revisao de patch.

## Validation Criteria

- protocolo declarado em `docops/docops.yaml` (`edit_protocol`).
- template de solicitacao de edicao presente em `docops/templates/`.
- auditoria de contrato valida campos de protocolo.

## Related References

- Related ADRs:
  - ADR-013 - DocOps Governed Document Workbench
  - ADR-015 - DocOps Workspace Contract
  - ADR-016 - DocOps Prompt Profile Registry
- Related docs:
  - `docs/DOCOPS-000_Vision_and_Boundary.md`
