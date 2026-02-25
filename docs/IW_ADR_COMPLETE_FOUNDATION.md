IdeaWalker (IW)

ADR COMPLETE FOUNDATION PACK

============================================================ ADR-000 —
Governance Model Overview
============================================================

Status Accepted

Contexto O IdeaWalker (IW) é um sistema observacional orientado à
caracterização estrutural de narrativas e discursos. Ele não realiza
inferência causal, ontológica ou decisória.

Decisão Adotar Architecture Decision Records (ADR) como mecanismo
institucional obrigatório para registrar decisões arquiteturais e
contratos epistemológicos.

Consequências - Toda decisão estrutural relevante exige ADR. - IW mantém
identidade observacional formalizada. - Integração futura com STRATA
ocorre sob contrato explícito.

Invariantes Derivadas 1. IW é observacional. 2. IW é não-inferencial. 3.
IW é versionado estruturalmente. 4. IW exporta apenas bundles
estruturados.

============================================================ ADR-001 —
Narrative Observation is Non-Inferential
============================================================

Status Accepted

Contexto O IW extrai vetores narrativos estruturais de documentos.

Decisão O IW descreve forma narrativa e não interpreta consequências
sistêmicas.

Consequências - Nenhum módulo pode gerar recomendações. - Nenhum
componente pode alterar estado externo. - Não há motor decisório no IW.

Invariantes Derivadas NarrativeVectorDTO é estritamente descritivo.

============================================================ ADR-002 —
NarrativeVector as Versioned DTO
============================================================

Status Accepted

Contexto O NarrativeVectorDTO é o núcleo estrutural do IW.

Decisão Deve ser: - Imutável - Versionado (schemaVersion obrigatório) -
Auditável - Exportável como bundle fechado

Consequências - Mudança estrutural exige novo ADR. - Compatibilidade
futura com sistemas downstream.

Invariantes Derivadas DTO não pode sofrer mutação downstream.

============================================================ ADR-003 —
Epistemic Regime Explicit Declaration
============================================================

Status Accepted

Contexto Narrativas operam sob regimes epistemológicos distintos.

Decisão Todo NarrativeVectorDTO deve conter subvetor:

epistemic { narrativeRegime truthCommitment evidentialDensity
allowsInference }

Consequências - Extração inválida sem regime explícito. - Permite
governança downstream.

Invariantes Derivadas narrativeRegime é campo obrigatório.

============================================================ ADR-004 —
Deterministic Extraction Baseline
============================================================

Status Accepted

Contexto Modelos LLM podem introduzir estocasticidade.

Decisão Toda extração deve registrar: - Modelo - Versão - Parâmetros -
Data - Seed (quando aplicável)

Consequências - Reprodutibilidade institucional. - Comparabilidade
longitudinal.

Invariantes Derivadas toolchain metadata é obrigatório.

============================================================ ADR-005 —
Narrative Bundle Export Contract
============================================================

Status Accepted

Contexto Vetores narrativos serão exportados para sistemas downstream
como STRATA.

Decisão Exportação ocorre exclusivamente via NarrativeBundle contendo: -
NarrativeVectorDTO - rawExcerptRef - extractionNotes - toolchain
metadata

Texto cru não pode ser exportado isoladamente.

Consequências - STRATA recebe apenas bundles estruturados. - Elimina
ingestão informal de texto.

Invariantes Derivadas Não é permitido exportar texto sem vetor
associado.

============================================================
ADR_TEMPLATE.md
============================================================

ADR-XXX —

Status Proposed | Accepted | Superseded | Deprecated

Contexto Descrever o problema arquitetural ou epistemológico.

Decisão Descrever decisão normativa.

Justificativa Explicar motivação e alternativas consideradas.

Consequências Listar impactos técnicos e institucionais.

Invariantes Derivadas Regras obrigatórias decorrentes da decisão.

Notas de Implementação Módulos ou pipelines afetados.

============================================================ Estrutura
Oficial do Diretório
============================================================

IdeaWalker/ ├─ adr/ │ ├─ ADR-000_Governance_Model_Overview.md │ ├─
ADR-001_Narrative_Observation_Non_Inferential.md │ ├─
ADR-002_NarrativeVector_Versioning.md │ ├─
ADR-003_Epistemic_Regime_Explicit.md │ ├─
ADR-004_Deterministic_Extraction.md │ ├─
ADR-005_Narrative_Bundle_Export_Contract.md │ └─ ADR_TEMPLATE.md

============================================================ Fim do
Documento
