#!/usr/bin/env bash
# export_estrat_seed.sh — IdeaWalker ESTRAT-SEED Exporter
# Exporta um ESTRAT-SEED após aprovação do VE-IW.
# Uso: ./scripts/export_estrat_seed.sh <caminho_nota.md> [--output <dir>]
#
# Referência:
#   - ADR: adr/ADR-019_ESTRAT_Integration_Protocol.md
#   - Policy: policies/estrat_integration.md
#   - Schema: estrat/ESTRAT_SEED_schema.md

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
TIMESTAMP=$(date +%s)
SEED_ID="SEED-${TIMESTAMP}"
OUTPUT_DIR="${PROJECT_ROOT}/strata/consumables/estrat"

usage() {
  echo "Uso: $0 <caminho_nota.md> [--output <dir>]"
  echo ""
  echo "Exporta um ESTRAT-SEED a partir de uma nota validada pelo VE-IW."
  echo "A nota deve estar em notas/ e o VE-IW deve ter aprovado a exportação."
  exit 1
}

if [[ $# -lt 1 ]]; then
  usage
fi

NOTE_PATH="$1"
shift

while [[ $# -gt 0 ]]; do
  case "$1" in
    --output) OUTPUT_DIR="$2"; shift 2 ;;
    *) echo "Argumento desconhecido: $1"; usage ;;
  esac
done

# Verificar que a nota existe
if [[ ! -f "$NOTE_PATH" ]]; then
  echo "[ESTRAT] ERRO: Nota não encontrada: $NOTE_PATH"
  exit 1
fi

# Verificar que a nota está em notas/
if [[ "$NOTE_PATH" != */notas/* ]]; then
  echo "[ESTRAT] AVISO: A nota não está no diretório notas/. Caminho: $NOTE_PATH"
  echo "[ESTRAT] Confirmar exportação? [s/N]"
  read -r confirm
  if [[ "$confirm" != "s" && "$confirm" != "S" ]]; then
    echo "[ESTRAT] Exportação cancelada."
    exit 0
  fi
fi

# Gate VE-IW: verificação de flags epistêmicas básicas no conteúdo da nota
echo "[ESTRAT] Executando gate VE-IW..."

HAS_TELEOLOGY="false"
HAS_NORMATIVE="false"
HAS_LINEAR="false"

# Heurísticas simples — em produção, substituir pelo VE-IW real do IdeaWalker
if grep -qi "deve necessariamente\|irá inevitavelmente\|é destinado a\|o objetivo final é" "$NOTE_PATH"; then
  HAS_TELEOLOGY="true"
fi
if grep -qi "é preciso que\|todos deveriam\|é obrigatório\|normativamente\|deve-se fazer" "$NOTE_PATH"; then
  HAS_NORMATIVE="true"
fi
if grep -qi "causa diretamente\|portanto todos\|logo, necessariamente\|sempre que X então Y" "$NOTE_PATH"; then
  HAS_LINEAR="true"
fi

if [[ "$HAS_TELEOLOGY" == "true" || "$HAS_NORMATIVE" == "true" || "$HAS_LINEAR" == "true" ]]; then
  echo "[ESTRAT] BLOQUEADO pelo gate VE-IW:"
  echo "  has_teleology: $HAS_TELEOLOGY"
  echo "  has_normative_claim: $HAS_NORMATIVE"
  echo "  has_linear_causality: $HAS_LINEAR"
  echo "[ESTRAT] Revise a nota antes de exportar."
  exit 2
fi

VE_IW_APPROVED="true"
VE_IW_SEAL=$(sha256sum "$NOTE_PATH" | awk '{print $1}')

# Extrair título (primeira linha H1 do markdown)
INSIGHT_TITLE=$(grep -m1 '^# ' "$NOTE_PATH" | sed 's/^# //' || echo "Sem título")

# Extrair insight summary (primeiro parágrafo após ## Insight Central)
INSIGHT_SUMMARY=$(awk '/^## Insight Central/{found=1; next} found && /^[^#]/{print; exit}' "$NOTE_PATH" || echo "")

# Extrair ações (checkboxes markdown)
TASK_ACTIONS=$(grep -E '^\s*- \[ \]' "$NOTE_PATH" | sed 's/^\s*- \[ \] //' | awk '{printf "  - \"%s\"\n", $0}' || echo "")

# Extrair conexões (backlinks [[...]])
CONNECTIONS=$(grep -oP '\[\[.*?\]\]' "$NOTE_PATH" | sed 's/\[\[//;s/\]\]//' | awk '{printf "  - \"%s\"\n", $0}' || echo "")

# Criar diretório de saída
mkdir -p "$OUTPUT_DIR"
OUTPUT_FILE="${OUTPUT_DIR}/${SEED_ID}.yaml"

cat > "$OUTPUT_FILE" <<YAML
# ESTRAT-SEED v1.0
type: seed
protocol_version: "1.0"
id: "${SEED_ID}"
source_project: "IdeaWalker"
ve_iw_approved: ${VE_IW_APPROVED}
ve_iw_seal: "${VE_IW_SEAL}"

insight_title: "${INSIGHT_TITLE}"
insight_summary: "${INSIGHT_SUMMARY}"
raw_intuition: |
$(sed 's/^/  /' "$NOTE_PATH")
structured_note_path: "${NOTE_PATH}"
task_actions:
${TASK_ACTIONS:-  []}
connections:
${CONNECTIONS:-  []}

epistemic_flags:
  has_teleology: ${HAS_TELEOLOGY}
  has_normative_claim: ${HAS_NORMATIVE}
  has_linear_causality: ${HAS_LINEAR}
YAML

echo "[ESTRAT] SEED exportado com sucesso:"
echo "  ID: ${SEED_ID}"
echo "  Arquivo: ${OUTPUT_FILE}"
echo "  VE-IW: aprovado"
echo "  Seal: ${VE_IW_SEAL}"
