#!/usr/bin/env bash
# scripts/audit_evaluations.sh
#
# Verifica estrutura minima de avaliacoes internas em docs/avaliacoes.

set -euo pipefail

EVAL_DIR="docs/avaliacoes"
EXCLUDED=("README.md" "TEMPLATE.md")

echo "=== Internal Evaluations Audit ==="
echo "Directory: ${EVAL_DIR}/"
echo ""

if [[ ! -d "${EVAL_DIR}" ]]; then
    echo "❌ FAIL: directory not found: ${EVAL_DIR}"
    exit 1
fi

declare -A EXCLUDED_MAP=()
for ex in "${EXCLUDED[@]}"; do
    EXCLUDED_MAP["$ex"]=1
done

FILES=()
while IFS= read -r -d '' f; do
    fname=$(basename "$f")
    if [[ -n "${EXCLUDED_MAP[$fname]:-}" ]]; then
        continue
    fi
    FILES+=("$f")
done < <(find "${EVAL_DIR}" -maxdepth 1 -name "*.md" -print0 | sort -z)

if (( ${#FILES[@]} == 0 )); then
    echo "✅ PASS: no evaluation files to validate."
    exit 0
fi

REQUIRED_FIELDS=("**Data:**" "**Autor:**" "**Base:**" "**Escopo:**" "**Status:**")

FAILED=0
for file in "${FILES[@]}"; do
    echo "Checking: ${file}"
    for field in "${REQUIRED_FIELDS[@]}"; do
        if ! grep -Fq "${field}" "${file}"; then
            echo "  ❌ Missing field: ${field}"
            FAILED=1
        fi
    done

    if ! grep -Eq '^##[[:space:]]*4([.)]|[[:space:]])[[:space:]]*Recomendacoes|^##[[:space:]]*Recomendacoes' "${file}"; then
        echo "  ❌ Missing recommendations section (expected '## 4) Recomendacoes' or equivalent)."
        FAILED=1
    fi

    if ! grep -Eq '^##[[:space:]]*5([.)]|[[:space:]])[[:space:]]*Resposta do IW|^##[[:space:]]*Resposta do IW' "${file}"; then
        echo "  ❌ Missing IW response section (expected '## 5) Resposta do IW' or equivalent)."
        FAILED=1
    fi
done

if (( FAILED != 0 )); then
    echo ""
    echo "❌ FAIL: evaluation audit failed."
    exit 1
fi

echo ""
echo "✅ PASS: all evaluations have required structure."
echo "=== Audit complete ==="

