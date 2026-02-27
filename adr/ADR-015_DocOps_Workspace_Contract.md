# ADR-015: DocOps Workspace Contract

Status: Accepted
Date: 2026-02-27
Decision Type: Contract
Supersedes: (none)
Superseded by: (none)

## Context

O tab `DocOps` no IW executa comandos em um workspace selecionado pelo usuario. Para transformar isso em operacao governada (e nao em shell runner generico), precisamos de um contrato minimo de estrutura para projetos orientados a documentos.

Sem contrato de workspace:

- cada projeto organiza arquivos de forma diferente;
- comandos/presets ficam ambiguos;
- verificacoes automatizadas nao conseguem validar conformidade basica.

## Decision

Definir um contrato de workspace DocOps baseado em `docops/docops.yaml` e uma estrutura minima de diretorios.

Contrato minimo:

1. Arquivo canonico: `docops/docops.yaml`.
2. Diretorio de perfis: `docops/profiles/`.
3. Diretorio de templates operacionais: `docops/templates/`.
4. Todo workspace deve declarar:
   - versao do contrato;
   - estrutura minima esperada;
   - protocolo de edicao controlada;
   - diretorio de logs operacionais.

## Scope / Boundaries

Aplica-se a:

- projetos que desejam usar DocOps com governanca estruturada;
- validacoes automatizadas de conformidade de workspace.

Nao se aplica a:

- repositorios que usam IW sem DocOps;
- definicao de regras epistemologicas de conteudo.

## Allowed and Prohibited Flows

- Allowed:
  - executar DocOps em workspace que atenda ao contrato;
  - evoluir contrato via novo ADR.

- Prohibited:
  - usar DocOps governado sem `docops/docops.yaml`;
  - tratar contrato de workspace como substituto de ADR.

## Consequences and Trade-offs

- Beneficios:
  - previsibilidade operacional entre projetos;
  - onboarding mais simples;
  - base para automacao de release e trilha de auditoria.

- Custos:
  - carga inicial de setup do workspace;
  - manutencao de contrato por projeto.

## Validation Criteria

- `docops/docops.yaml` presente no repositorio.
- `docops/profiles/` e `docops/templates/` presentes.
- `scripts/audit_docops_contract.sh` valida o contrato.
- CI executa auditoria de contrato DocOps no job `invariant-audit`.

## Related References

- Related ADRs:
  - ADR-013 - DocOps Governed Document Workbench
  - ADR-014 - Internal Evaluations Registry
- Related docs:
  - `docs/DOCOPS-000_Vision_and_Boundary.md`
