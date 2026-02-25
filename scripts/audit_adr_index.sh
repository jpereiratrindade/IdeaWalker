#!/usr/bin/env bash
# scripts/audit_adr_index.sh
#
# F1.D4 — Verifica que o ADR_INDEX.md está sincronizado com os arquivos em adr/
#
# Regra:
#   Todo arquivo *.md em adr/ (exceto ADR_INDEX.md, ADR_TEMPLATE.md, README.md)
#   deve aparecer no ADR_INDEX.md.
#   Toda entrada no ADR_INDEX.md deve ter um arquivo correspondente em adr/.

set -euo pipefail

ADR_DIR="adr"
INDEX_FILE="${ADR_DIR}/ADR_INDEX.md"
EXCLUDED=("ADR_INDEX.md" "ADR_TEMPLATE.md" "README.md")

echo "=== ADR_INDEX Audit ==="
echo "Directory: ${ADR_DIR}/"
echo "Index:     ${INDEX_FILE}"
echo ""

if [[ ! -d "${ADR_DIR}" ]]; then
    echo "❌ FAIL: ADR directory not found: ${ADR_DIR}"
    exit 1
fi

if [[ ! -f "${INDEX_FILE}" ]]; then
    echo "❌ FAIL: ADR index not found: ${INDEX_FILE}"
    exit 1
fi

declare -A EXCLUDED_MAP=()
for ex in "${EXCLUDED[@]}"; do
    EXCLUDED_MAP["$ex"]=1
done

declare -A DISK_SET=()
declare -A INDEX_SET=()
declare -A INDEX_COUNTS=()
DISK_FILES=()
INDEX_FILES=()

# --- Coleta arquivos ADR no diretório ---
while IFS= read -r -d '' f; do
    fname=$(basename "$f")
    if [[ -n "${EXCLUDED_MAP[$fname]:-}" ]]; then
        continue
    fi
    DISK_FILES+=("$fname")
    DISK_SET["$fname"]=1
done < <(find "${ADR_DIR}" -maxdepth 1 -name "*.md" -print0 | sort -z)

# --- Coleta arquivos citados no ADR_INDEX ---
while IFS= read -r token; do
    file_ref="${token//\`/}"
    fname="$(basename "$file_ref")"
    if [[ -n "${EXCLUDED_MAP[$fname]:-}" ]]; then
        continue
    fi
    INDEX_FILES+=("$fname")
    INDEX_SET["$fname"]=1
    INDEX_COUNTS["$fname"]=$(( ${INDEX_COUNTS["$fname"]:-0} + 1 ))
done < <(grep -oE '`[^`]+\.md`' "${INDEX_FILE}" || true)

echo "Files on disk (${#DISK_FILES[@]}):"
for f in "${DISK_FILES[@]}"; do
    echo "  - $f"
done
echo ""

echo "Files referenced in index (${#INDEX_FILES[@]}):"
for f in "${INDEX_FILES[@]}"; do
    echo "  - $f"
done
echo ""

MISSING_FROM_INDEX=()
for fname in "${DISK_FILES[@]}"; do
    if [[ -z "${INDEX_SET[$fname]:-}" ]]; then
        MISSING_FROM_INDEX+=("$fname")
    fi
done

MISSING_ON_DISK=()
for fname in "${INDEX_FILES[@]}"; do
    if [[ -z "${DISK_SET[$fname]:-}" ]]; then
        MISSING_ON_DISK+=("$fname")
    fi
done

DUPLICATED_IN_INDEX=()
for fname in "${!INDEX_COUNTS[@]}"; do
    if (( INDEX_COUNTS["$fname"] > 1 )); then
        DUPLICATED_IN_INDEX+=("$fname")
    fi
done

FAILED=0

if (( ${#MISSING_FROM_INDEX[@]} > 0 )); then
    echo "❌ FAIL: Files on disk not referenced in ADR_INDEX.md:"
    for f in "${MISSING_FROM_INDEX[@]}"; do
        echo "  - $f"
    done
    echo ""
    FAILED=1
fi

if (( ${#MISSING_ON_DISK[@]} > 0 )); then
    echo "❌ FAIL: Files referenced in ADR_INDEX.md but missing on disk:"
    for f in "${MISSING_ON_DISK[@]}"; do
        echo "  - $f"
    done
    echo ""
    FAILED=1
fi

if (( ${#DUPLICATED_IN_INDEX[@]} > 0 )); then
    echo "❌ FAIL: Duplicate ADR file references in ADR_INDEX.md:"
    for f in "${DUPLICATED_IN_INDEX[@]}"; do
        echo "  - $f (${INDEX_COUNTS["$f"]}x)"
    done
    echo ""
    FAILED=1
fi

if (( FAILED != 0 )); then
    echo "→ Fix ADR_INDEX.md and rerun audit."
    exit 1
fi

echo "✅ PASS: ADR_INDEX.md is synchronized with adr/ directory."
echo "=== Audit complete ==="
