IdeaWalker (IW)

ADR Base Documentation Pack

======================================== 1. ADR TEMPLATE (Padrão Oficial IW) ========================================

ADR-XXX — 

Status

Proposed | Accepted | Superseded | Deprecated

Contexto

Descrever o problema arquitetural ou epistemológico que exige decisão
explícita.

Decisão

Descrever a decisão tomada de forma normativa e inequívoca.

Justificativa

Explicar por que esta decisão foi tomada e quais alternativas foram
consideradas.

Consequências

Listar impactos técnicos, epistemológicos e organizacionais.

Invariantes Derivadas

Listar regras que passam a valer a partir desta decisão.

Notas de Implementação

Se aplicável, indicar módulos, DTOs ou pipelines afetados.

------------------------------------------------------------------------

======================================== 2. ADR-001 — Narrative Observation is Non-Inferential ========================================

Status

Accepted

Contexto

O IW extrai vetores narrativos estruturais a partir de documentos.

Decisão

O IW opera exclusivamente no domínio observacional. Nenhum módulo pode
realizar inferência causal ou decisória.

Justificativa

Preservar separação epistemológica entre IW e STRATA.

Consequências

-   Proibição de recomendação automatizada.
-   Proibição de alteração de estado externo.
-   IW atua apenas como caracterizador estrutural.

Invariantes Derivadas

-   NarrativeVectorDTO é descritivo.
-   Não há motor decisório no IW.

------------------------------------------------------------------------

======================================== 3. ADR-002 — NarrativeVector as Versioned DTO ========================================

Status

Accepted

Contexto

O NarrativeVectorDTO é o núcleo estrutural do IW.

Decisão

Deve ser imutável, versionado e exportado como bundle fechado.

Justificativa

Garantir rastreabilidade histórica e compatibilidade futura.

Consequências

-   schemaVersion obrigatório.
-   Mudança estrutural exige novo ADR.

Invariantes Derivadas

-   DTO não pode sofrer mutação downstream.

------------------------------------------------------------------------

======================================== 4. ADR-003 — Epistemic Regime Explicit Declaration ========================================

Status

Accepted

Contexto

Narrativas operam sob regimes epistemológicos distintos.

Decisão

Todo NarrativeVectorDTO deve conter subvetor epistemic.

Justificativa

Evitar mistura entre ficcional, factual e testemunhal.

Consequências

-   Extração inválida sem regime explícito.

Invariantes Derivadas

-   narrativeRegime é campo obrigatório.

------------------------------------------------------------------------

======================================== 5. ADR-004 — Deterministic Extraction Baseline ========================================

Status

Accepted

Contexto

Modelos LLM podem introduzir estocasticidade.

Decisão

Registrar modelo, versão, parâmetros e data. Aplicar seed fixa quando
possível.

Justificativa

Garantir reprodutibilidade institucional.

Consequências

-   Extrações devem ser auditáveis.

Invariantes Derivadas

-   toolchain metadata obrigatório.

------------------------------------------------------------------------

======================================== 6. ADR-005 — Narrative Bundle Export Contract ========================================

Status

Proposed

Contexto

Vetores narrativos serão exportados para sistemas downstream.

Decisão

Exportação deve ocorrer via NarrativeBundle fechado contendo: -
NarrativeVectorDTO - rawExcerptRef - extractionNotes - toolchain
metadata

Justificativa

Evitar ingestão de texto cru por sistemas externos.

Consequências

-   STRATA recebe apenas bundles estruturados.

Invariantes Derivadas

-   Não é permitido exportar texto sem vetor.

------------------------------------------------------------------------

======================================== 7. Estrutura Recomendada do Diretório ADR ========================================

IdeaWalker/ ├─ adr/ │ ├─
ADR-001_Narrative_Observation_Non_Inferential.md │ ├─
ADR-002_NarrativeVector_Versioning.md │ ├─
ADR-003_Epistemic_Regime_Explicit.md │ ├─
ADR-004_Deterministic_Extraction.md │ ├─
ADR-005_Narrative_Bundle_Export_Contract.md │ └─ ADR_TEMPLATE.md

------------------------------------------------------------------------

Fim do Documento
