IdeaWalker (IW)

ADR-006 — IW ↔ STRATA Integration Contract

Status Accepted

------------------------------------------------------------------------

Contexto

O IdeaWalker (IW) é um sistema observacional responsável por
caracterizar estruturas narrativas e produzir NarrativeBundles
versionados.

O STRATA é um sistema ontológico/simulacional responsável por modelagem
ecológica e decisão sistêmica.

É necessário formalizar o contrato de integração entre os dois sistemas
para evitar acoplamento implícito e deriva epistemológica.

------------------------------------------------------------------------

Decisão

A integração IW ↔ STRATA obedecerá às seguintes regras:

1.  IW exporta exclusivamente NarrativeBundle estruturado.
2.  STRATA não processa texto cru proveniente do IW.
3.  STRATA não pode reinterpretar regime epistemológico declarado pelo
    IW.
4.  STRATA deve validar schemaVersion antes de ingestão.
5.  IW não realiza qualquer cálculo de peso decisório.

------------------------------------------------------------------------

Justificativa

Preservar separação Observação × Ontologia. Evitar inferência mascarada.
Garantir contrato institucional explícito entre sistemas.

------------------------------------------------------------------------

Consequências

-   Ingestão no STRATA exige validação estrutural.
-   Mudanças no NarrativeVector exigem coordenação formal.
-   Integração passa a ser governada por contrato versionado.

------------------------------------------------------------------------

Invariantes Derivadas

-   IW nunca decide.
-   STRATA nunca reinterpreta regime narrativo.
-   Bundle é unidade mínima de intercâmbio.

------------------------------------------------------------------------

Fim do Documento
