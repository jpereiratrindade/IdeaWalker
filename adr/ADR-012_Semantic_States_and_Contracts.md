ADR-012 — Semantic States and Consumption Contracts

Status

Accepted

Context

A fase F1 de Hardening introduziu mecanismos de resiliência (extração parcial) e auditoria (logs de exclusão estrutural). Sem formalização, essas adições criam dois riscos institucionais:
1. Deriva Semântica: Adicionar campos de "status" na raiz do bundle altera o schema versionado sem governança.
2. Shadow Metadata: Logs de auditoria locais podem ser confundidos com parte do bundle institucional (contrato STRATA).

Decision

1. Estados de Extração: Todo bundle gerado pelo IdeaWalker deve declarar seu estado de completude. Para evitar quebra do schema raiz, esses metadados devem residir obrigatoriamente no objeto `source`.
   - `extractionStatus`: [complete | partial-narrative | failed]
   - `isPartial`: boolean

2. Local Audit Artifacts (LAA): Arquivos gerados para depuração e auditoria técnica (ex: `.exclusions.log`) são classificados como LAA.
   - LAAs NÃO fazem parte do bundle institucional.
   - LAAs NÃO devem ser trafegados para sistemas downstream (STRATA).
   - LAAs servem apenas para governança local e melhoria do pipeline.

Justification

Mover o status para o objeto `source` protege a estabilidade da raiz do bundle, permitindo que sistemas downstream continuem processando os DTOs narrativos/discursivos sem falhas de parsing de campos desconhecidos. A distinção LAA vs Bundle protege a pureza epistemológica dos artefatos cognitivos entregues.

Consequences

- Mudança no `ScientificIngestionService` para injetar status no envelope `source`.
- Testes de integridade devem validar a presença desses campos no local correto.

Invariants Derived

- S-005: Todo bundle deve conter `source.extractionStatus`.
- G-002: Artefatos com extensão `.log` no diretório de observações são estritamente Locais (LAA).

Implementation Notes

- Refatoração de `ScientificIngestionService::attachSourceMetadata`.
- Atualização de `ScientificResilienceTest`.
