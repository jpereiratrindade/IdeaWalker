# DocOps Workspace Contract

Este diretorio define o contrato minimo para operacao governada de documentos no IW.

## Estrutura canonica

- `docops/docops.yaml` - configuracao normativa do workspace DocOps.
- `docops/profiles/` - perfis de prompt versionados.
- `docops/templates/` - templates operacionais (ex.: solicitacao de edicao).

## Objetivo

Garantir que assistencia de escrita com LLM seja:

- rastreavel;
- versionavel;
- limitada por regras explicitas (ADR-first).

## Validacao

Execute:

```bash
bash scripts/audit_docops_contract.sh
```

Esse comando valida a estrutura minima e campos obrigatorios.
