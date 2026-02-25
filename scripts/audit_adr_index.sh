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

# --- Coleta arquivos ADR no diretório ---
DISK_FILES=()
while IFS= read -r -d '' f; do
    fname=$(basename "$f")
    skip=false
    for ex in "${EXCLUDED[@]}"; do
        [[ "$fname" == "$ex" ]] && skip=true && break
    done
    $skip || DISK_FILES+=("$fname")
done < <(find "${ADR_DIR}" -maxdepth 1 -name "*.md" -print0 | sort -z)

echo "Files on disk (${#DISK_FILES[@]}):"
for f in "${DISK_FILES[@]}"; do echo "  - $f"; done
echo ""

# --- Verifica que cada arquivo do disco aparece no INDEX ---
MISSING_FROM_INDEX=()
for fname in "${DISK_FILES[@]}"; do
    if ! grep -qF "$fname" "${INDEX_FILE}"; then
        MISSING_FROM_INDEX+=("$fname")
    fi
done

if [ ${#MISSING_FROM_INDEX[@]} -gt 0 ]; then
    echo "❌ FAIL: Files not referenced in ADR_INDEX.md:"
    for f in "${MISSING_FROM_INDEX[@]}"; do echo "  - $f"; done
    echo ""
    echo "→ Add the missing entries to ${INDEX_FILE}"
    exit 1
fi

echo "✅ PASS: All ADR files in adr/ are referenced in ADR_INDEX.md."
echo "=== Audit complete ==="
