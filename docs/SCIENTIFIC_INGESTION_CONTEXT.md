# DDD — SCIENTIFIC INGESTION CONTEXT
## Ingestão científica e geração de consumíveis STRATA

**VERSÃO:** 1.0  
**STATUS:** Implementado (MVP governado)  
**NATUREZA:** Observacional / Declarativa / Read-only  
**ESCOPO:** Contexto Auxiliar (NÃO Core Domain)

---

## 1. Objetivo

Adaptar a ingestão de documentos para artigos científicos sem transformar o IdeaWalker em um “parser de STRATA”.
O sistema gera artefatos cognitivos explicitamente tipados e, a partir deles, produz consumíveis estáveis para o STRATA.

---

## 2. Princípios

1. **Separar o que o artigo diz do que sugere.**
2. **Evitar normatividade** (sem recomendações, sem conclusões prescritivas).
3. **Gerar artefatos auditáveis**, com fonte, escopo e incerteza.
4. **Consumíveis STRATA são derivados**, não o texto cru do artigo.
5. **Declaração ≠ Ontologia**: O IdeaWalker exporta *candidatos* (strings), o STRATA decide a ontologia (enums).

---

## 3. Pastas e fluxo

- Inbox científica: `inbox/scientific/`
- Bundles brutos (auditoria): `observations/scientific/`
- Consumíveis STRATA: `strata/consumables/<artifactId>/`

Fluxo:
1. Scanner detecta artigos na inbox científica.
2. Conteúdo é extraído (PDF/LaTeX/Markdown/Text).
3. IA gera bundle JSON com artefatos cognitivos.
4. Bundle é validado e salvo.
5. Consumíveis STRATA são exportados por artefato.

---

## 4. Esquema (schemaVersion=4)

**Bundle JSON base (salvo em `observations/scientific/`):**

```json
{
  "schemaVersion": 4,
  "sourceProfile": {
    "studyType": "experimental|observational|review|theoretical|simulation|mixed|unknown",
    "temporalScale": "short|medium|long|multi|unknown",
    "ecosystemType": "terrestrial|aquatic|urban|agro|industrial|social|digital|mixed|unknown",
    "evidenceType": "empirical|theoretical|mixed|unknown",
    "transferability": "high|medium|low|contextual|unknown",
    "contextNotes": "texto curto ou unknown",
    "limitations": "texto curto ou unknown"
  },
  "narrativeObservations": [
    {
      "observation": "...",
      "context": "...",
      "limits": "...",
      "confidence": "low|medium|high|unknown",
      "evidence": "direct|inferred|unknown",
      "evidenceSnippet": "trecho curto do artigo",
      "sourceSection": "Results|Discussion|Methods|Unknown",
      "pageRange": "pp. 3-4",
      "contextuality": "site-specific|conditional|comparative|non-universal"
    }
  ],
  "allegedMechanisms": [
    {
      "mechanism": "...",
      "status": "tested|inferred|speculative|unknown",
      "context": "...",
      "limitations": "...",
      "evidenceSnippet": "trecho curto do artigo",
      "sourceSection": "Results|Discussion|Methods|Unknown",
      "pageRange": "pp. 5-6",
      "contextuality": "site-specific|conditional|comparative|non-universal"
    }
  ],
  "temporalWindowReferences": [
    {
      "timeWindow": "...",
      "changeRhythm": "...",
      "delaysOrHysteresis": "...",
      "context": "...",
      "evidenceSnippet": "trecho curto do artigo",
      "sourceSection": "Results|Discussion|Methods|Unknown",
      "pageRange": "pp. 7-9"
    }
  ],
  "baselineAssumptions": [
    {
      "baselineType": "fixed|dynamic|multiple|none|unknown",
      "description": "...",
      "context": "..."
    }
  ],
  "trajectoryAnalogies": [
    {
      "analogy": "...",
      "scope": "...",
      "justification": "..."
    }
  ],
  "interpretationLayers": {
    "observedStatements": ["..."],
    "authorInterpretations": ["..."],
    "possibleReadings": ["..."]
  },
  "discursiveContext": {
    "frames": [
      {
        "label": "...",
        "description": "...",
        "valence": "normative|descriptive|critical|implicit",
        "evidenceSnippet": "..."
      }
    ],
    "epistemicRole": "discursive-reading"
  }
}
```

**Consumíveis STRATA (exportados em `strata/consumables/<artifactId>/`):**
- `SourceProfile.json`
- `NarrativeObservation.json`
- `AllegedMechanisms.json`
- `TemporalWindowReference.json`
- `BaselineAssumptions.json`
- `TrajectoryAnalogies.json`
- `InterpretationLayers.json`
- `DiscursiveContext.json` (opcional)
- `Manifest.json`
Opcional (se houver conteúdo):
- `DiscursiveSystem.json`

Todos os consumíveis incluem:
- `schemaVersion`
- `source` (com `artifactId`, `path`, `contentHash`, `ingestedAt`, `model`)

Observação:
- `NarrativeObservation.json` e `DiscursiveSystem.json` só são gerados quando houver conteúdo (não são emitidos vazios).

### 4.1 Regra de ancoragem (obrigatória)
Os itens de `narrativeObservations`, `allegedMechanisms` e `temporalWindowReferences`
devem conter:
- `evidenceSnippet`: trecho curto do artigo
- `sourceSection`: seção de origem (ex.: `Results`, `Discussion`, `Methods`, `Unknown`)
- `pageRange`: intervalo de páginas (ex.: `pp. 3-4`)

Itens sem ancoragem são descartados durante a ingestão.

---

## 6. Validador Epistemológico (VE-IW)

Antes da exportação para STRATA, o bundle passa pelo VE-IW:

**Checks mínimos**
- Contextualidade explícita em observações e mecanismos.
- Baseline coerente com a escala temporal.
- Janelas temporais explícitas e sem vagueza.
- Linguagem sem normatividade implícita.
- Mecanismos com status e limitações.
- Camada correta (InterpretationLayers nunca para STRATA-Core).

**Artefatos gerados**
- `EpistemicValidationReport.json`
- `ExportSeal.json` (com `exportAllowed` e `allowedTargets`)

**Visibilidade na UI**
- A aba `Dashboard & Inbox` mostra o status da última validação.
- O botão **Ver Relatório no Log** imprime o JSON completo no System Log.

Sem selo, nada entra no STRATA.

---

## 5. Governança

Regra de ouro:
- Se o artefato ainda admite debate, ele permanece no IdeaWalker.
- Se é referência explícita e estável, ele pode alimentar o STRATA.

Essa separação mantém auditabilidade e reduz risco de normatividade indevida.
