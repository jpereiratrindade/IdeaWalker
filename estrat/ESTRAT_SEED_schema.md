# ESTRAT-SEED Schema v1.0

Artefato produzido pelo **IdeaWalker** após aprovação do VE-IW.

## Campos Obrigatórios

```yaml
# ESTRAT-SEED v1.0
type: seed                         # fixo
protocol_version: "1.0"
id: "SEED-<timestamp_unix>"        # ex: SEED-1744310400
source_project: "IdeaWalker"       # fixo
ve_iw_approved: true               # DEVE ser true — qualquer false bloqueia a exportação
ve_iw_seal: "<sha256_hex>"         # SHA-256 do conteúdo de structured_note_path

insight_title: ""                  # título extraído da nota estruturada
insight_summary: ""                # parágrafo de síntese (1 parágrafo)
raw_intuition: ""                  # texto bruto ORIGINAL do inbox — sem normalização
structured_note_path: ""           # caminho relativo para a nota .md em notas/
task_actions: []                   # lista de strings: ações extraídas pela IA
connections: []                    # lista de strings: backlinks semânticos ativos

epistemic_flags:
  has_teleology: false             # DEVE ser false
  has_normative_claim: false       # DEVE ser false
  has_linear_causality: false      # DEVE ser false
```

## Destino de Saída

```
strata/consumables/estrat/SEED-<timestamp>.yaml
```

## Regras de Validação

| Campo | Regra |
|---|---|
| `ve_iw_approved` | Deve ser `true`. Se `false`, o arquivo não é gravado. |
| `epistemic_flags.*` | Todos devem ser `false`. Qualquer `true` bloqueia exportação. |
| `ve_iw_seal` | SHA-256 do arquivo apontado por `structured_note_path`. |
| `raw_intuition` | Não pode estar vazio. Deve ser o texto bruto do inbox, não reprocessado. |
| `structured_note_path` | Deve apontar para arquivo existente em `notas/`. |

## Invariante de Design

> O `raw_intuition` deve chegar ao CCE sem normalização prematura.  
> Se o IdeaWalker sobreestruturar antes de exportar, viola o primeiro passo do CCE.

## Referências

- ADR: `adr/ADR-019_ESTRAT_Integration_Protocol.md`
- Policy: `policies/estrat_integration.md`
- CCE consumer schema: `cce/estrat/ESTRAT_CONCEPT_schema.md`
