# Avaliacoes Internas (IW)

Diretorio canonico para avaliacoes internas do IdeaWalker.

## Objetivo

Registrar feedbacks estruturados e suas respostas institucionais, preservando rastreabilidade e memoria tecnica.

## Formato obrigatorio

Cada arquivo de avaliacao deve conter:

- metadados:
  - `Data`
  - `Autor`
  - `Base`
  - `Escopo`
  - `Status`
- secao de recomendacoes
- secao de resposta do IW

Use `TEMPLATE.md` como base.

## Convencao de nomes

Sugestao:

- `AVALIACAO-<tema>-YYYY-MM-DD.md`

## Enforcement

As regras minimas de estrutura sao auditadas por:

- `scripts/audit_evaluations.sh`

E validadas no CI no job de `invariant-audit`.

