IdeaWalker (IW)

IW_INVARIANT_MATRIX

Este documento consolida invariantes derivados dos ADRs ativos.

============================================================ 1.
Invariantes Epistemológicos
============================================================

I-001: IW é exclusivamente observacional. I-002: IW não realiza
inferência causal. I-003: IW não gera recomendações. I-004: IW não
altera estado externo.

============================================================ 2.
Invariantes Estruturais
============================================================

S-001: Todo NarrativeVector possui schemaVersion obrigatório. S-002:
NarrativeVectorDTO é imutável após exportação. S-003: narrativeRegime é
campo obrigatório. S-004: toolchain metadata é obrigatório. S-005:
Exportação ocorre apenas via NarrativeBundle.

============================================================ 3.
Invariantes de Integração
============================================================

X-001: STRATA deve validar schemaVersion antes de ingestão. X-002:
STRATA não processa texto cru do IW. X-003: STRATA não altera regime
epistemológico declarado.

============================================================ 4.
Enforcement Recomendado

-   Validação de schema automática.
-   Testes unitários para exportação de Bundle.
-   Check CI para presença de subvetor epistemic.
-   Registro obrigatório de toolchain metadata.

============================================================ Fim do
Documento
