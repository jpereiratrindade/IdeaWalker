# ADR-014: Internal Evaluations Registry and Response Protocol

Status: Accepted
Date: 2026-02-27
Decision Type: Foundational governance
Supersedes: (none)
Superseded by: (none)

## Context

O IW passou a operar com maior densidade de governanca (ADR index auditavel, catalogo gerado, maturity model e invariant matrix). Faltava padronizar o tratamento de avaliacoes internas (feedback tecnico, revisoes criticas, leituras estruturais) para evitar perda de conhecimento e respostas ad hoc.

Sem protocolo formal:

- feedbacks relevantes se perdem em historico de chat/threads;
- nao ha trilha auditavel de "avaliacao recebida -> resposta institucional";
- fica dificil distinguir opiniao pontual de insumo governavel.

## Decision

Instituir um protocolo formal para avaliacoes internas no IW:

1. Avaliacoes ficam em `docs/avaliacoes/`.
2. Cada avaliacao deve usar metadados minimos obrigatorios:
   - `Data`
   - `Autor`
   - `Base`
   - `Escopo`
   - `Status`
3. Cada avaliacao deve conter:
   - secao de recomendacoes
   - secao de resposta do IW
4. Fornecer template oficial em `docs/avaliacoes/TEMPLATE.md`.
5. Enforce automatico no CI via `scripts/audit_evaluations.sh`.

## Scope / Boundaries

Aplica-se a:

- avaliacoes internas do projeto IW;
- documentos de resposta institucional vinculados a essas avaliacoes.

Nao se aplica a:

- ADRs (continuam no fluxo `adr/`);
- changelog/release notes;
- notas operacionais sem carater avaliativo.

## Allowed and Prohibited Flows

- Allowed:
  - registrar avaliacao tecnica com metadados e recomendacoes;
  - registrar resposta institucional no proprio documento;
  - evoluir template por ADR.

- Prohibited:
  - registrar avaliacao sem metadados minimos;
  - publicar avaliacao sem secao de resposta do IW;
  - usar avaliacao como substituto de ADR para decisoes arquiteturais.

## Consequences and Trade-offs

- Beneficios:
  - maior rastreabilidade de feedbacks e respostas;
  - reducao de knowledge loss;
  - melhor integracao com maturity/invariant governance.

- Custos:
  - pequena carga documental adicional;
  - necessidade de manter disciplina de preenchimento.

## Validation Criteria

- `docs/avaliacoes/README.md` e `docs/avaliacoes/TEMPLATE.md` existentes.
- `scripts/audit_evaluations.sh` retorna sucesso para estrutura valida.
- CI executa auditoria de avaliacoes no job `invariant-audit`.

## Related References

- Related ADRs:
  - ADR-000 - Governance Model Overview
  - ADR-013 - DocOps Governed Document Workbench
- Related docs:
  - `docs/IW_MATURITY_MODEL.md`
  - `docs/IW_INVARIANT_MATRIX.md`

