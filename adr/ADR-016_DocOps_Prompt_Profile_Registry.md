# ADR-016: DocOps Prompt Profile Registry

Status: Accepted
Date: 2026-02-27
Decision Type: Contract
Supersedes: (none)
Superseded by: (none)

## Context

O objetivo do DocOps e apoiar criacao e revisao de documentos com LLM, mantendo governanca e rastreabilidade. Para isso, precisamos de perfis de prompt versionados e auditaveis.

Sem registro formal de perfis:

- o comportamento do assistente varia de forma opaca;
- nao ha rastreio de "quem instruiu o modelo a fazer o que";
- revisoes futuras ficam sem baseline institucional.

## Decision

Instituir um registro canonico de perfis em `docops/profiles/*.yaml`, com campos minimos obrigatorios e fronteiras explicitas.

Campos minimos por perfil:

- `id`
- `name`
- `mode`
- `objective`
- `inputs`
- `allowed_actions`
- `prohibited_actions`
- `prompt_template`

Perfis iniciais:

- `architect`
- `author`
- `editor`
- `reviewer`

## Scope / Boundaries

Aplica-se a:

- comportamento de assistencia textual no DocOps;
- governanca de perfis/prompt base.

Nao se aplica a:

- validacao automatica de verdade empirica;
- substituicao de revisao humana final.

## Allowed and Prohibited Flows

- Allowed:
  - selecionar perfil explicito para cada operacao de assistencia;
  - evoluir perfis por revisao versionada no repositorio.

- Prohibited:
  - executar assistencia sem perfil declarado;
  - permitir perfil sem limites de acao/proibicoes.

## Consequences and Trade-offs

- Beneficios:
  - previsibilidade do comportamento de assistencia;
  - menor risco de deriva semantica e escopo;
  - trilha de auditoria de intencao operacional.

- Custos:
  - manutencao recorrente dos perfis;
  - necessidade de calibracao entre perfis.

## Validation Criteria

- `docops/profiles/` contem perfis canonicamente nomeados.
- cada perfil possui os campos obrigatorios.
- `scripts/audit_docops_contract.sh` valida perfis.

## Related References

- Related ADRs:
  - ADR-013 - DocOps Governed Document Workbench
  - ADR-015 - DocOps Workspace Contract
- Related docs:
  - `docs/DOCOPS-000_Vision_and_Boundary.md`
