# ADR-018: ORT Core and Dimensions Boundary (RTV Decoupling)

Status: Accepted
Date: 2026-03-02
Decision Type: Domain boundary
Supersedes: (none)
Superseded by: (none)

## Context

A evolucao do trabalho em ORT mostrou um risco de acoplamento conceitual: tratar RTV como se fosse a propria ontologia-base.

Esse acoplamento gera problemas de governanca:

- confunde hipotese/dimensao com compromisso ontologico basal;
- permite que artefatos de exploracao alterem semantica de base sem deliberacao explicita;
- dificulta consumo seletivo no ecossistema STRATA.

Precisamos explicitar fronteira para manter falsificabilidade, rastreabilidade e evolucao modular.

## Decision

Definir ORT como arquitetura de duas camadas:

1. **ORT-Core**: compromissos ontologicos minimos de territorio/territorialidade.
2. **ORT-Dimensions**: dimensoes emergentes (ex.: RTV, Pecuaria Familiar), cada uma com escopo, criterios e evidencias declaradas.

Regras normativas:

- RTV nao e sinonimo de ORT.
- nenhuma dimensao altera o ORT-Core sem ADR especifico.
- artefatos de aplicacao (relatos, seeds, exploracoes) podem gerar hipoteses, mas nao mudam ontologia automaticamente.
- consumidores (incluindo STRATA) devem depender de contratos de dimensao, nao de uma hipotese unica hardcoded.

## Scope / Boundaries

Aplica-se a:

- documentos e contratos que referenciam ORT no ecossistema IW/DocOps/STRATA.
- configuracao de workflows e presets de release para evitar acoplamento a RTV.

Nao se aplica a:

- validacao empirica de uma dimensao especifica;
- escolha de quais dimensoes serao aceitas (isso depende de ADRs proprios por dimensao).

## Allowed and Prohibited Flows

- Allowed:
  - adicionar novas dimensoes via documento ORT/ADR dedicado;
  - manter targets de release genericos ou por dimensao explicita no workspace.

- Prohibited:
  - tratar RTV como base obrigatoria do ORT;
  - alterar ORT-Core por inferencia automatica de artefatos;
  - hardcode de pipeline assumindo RTV como dependencia estrutural unica.

## Consequences and Trade-offs

- Beneficios:
  - melhora clareza epistemica entre base e hipotese;
  - aumenta modularidade para integracao STRATA;
  - reduz risco de lock-in conceitual em uma unica dimensao.

- Custos:
  - exige disciplina de governanca por dimensao (mais ADRs e contratos);
  - pode aumentar overhead inicial de documentacao.

## Validation Criteria

- `docs/DOCOPS-000_Vision_and_Boundary.md` explicita ORT como estrutura geral e RTV como dimensao emergente.
- presets/referencias padrao nao devem depender de `release-rtv-pdf` como caminho canonico.
- auditoria de indice ADR deve permanecer consistente (`scripts/audit_adr_index.sh`).
- catalogo de ADR deve incluir este registro (`scripts/build_adr_catalog.py`).

## Related References

- Related ADRs:
  - ADR-013 - DocOps Governed Document Workbench
  - ADR-015 - DocOps Workspace Contract
  - ADR-017 - DocOps Controlled Edit Protocol
- Related docs:
  - `docs/DOCOPS-000_Vision_and_Boundary.md`
