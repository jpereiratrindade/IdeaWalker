#!/usr/bin/env bash
# scripts/audit_docops_contract.sh
#
# Verifica contrato minimo do workspace DocOps.

set -euo pipefail

ROOT_DIR="${1:-.}"
DOCOPS_DIR="${ROOT_DIR}/docops"
CONTRACT_FILE="${DOCOPS_DIR}/docops.yaml"
REGISTRY_FILE="${DOCOPS_DIR}/project_registry.yaml"
PROFILES_DIR="${DOCOPS_DIR}/profiles"
TEMPLATES_DIR="${DOCOPS_DIR}/templates"

echo "=== DocOps Contract Audit ==="
echo "Root: ${ROOT_DIR}"
echo ""

FAILED=0

require_file() {
    local file="$1"
    if [[ ! -f "${file}" ]]; then
        echo "❌ Missing file: ${file}"
        FAILED=1
    fi
}

require_dir() {
    local dir="$1"
    if [[ ! -d "${dir}" ]]; then
        echo "❌ Missing directory: ${dir}"
        FAILED=1
    fi
}

require_contract_field() {
    local key="$1"
    if ! grep -Eq "^${key}:" "${CONTRACT_FILE}" 2>/dev/null; then
        echo "❌ Missing contract field: ${key}"
        FAILED=1
    fi
}

require_registry_field() {
    local key="$1"
    if ! grep -Eq "^${key}:" "${REGISTRY_FILE}" 2>/dev/null; then
        echo "❌ Missing project registry field: ${key}"
        FAILED=1
    fi
}

require_profile_field() {
    local profile_file="$1"
    local key="$2"
    if ! grep -Eq "^${key}:" "${profile_file}"; then
        echo "  ❌ Missing profile field '${key}' in ${profile_file}"
        FAILED=1
    fi
}

require_dir "${DOCOPS_DIR}"
require_file "${CONTRACT_FILE}"
require_file "${REGISTRY_FILE}"
require_dir "${PROFILES_DIR}"
require_dir "${TEMPLATES_DIR}"

if [[ -f "${CONTRACT_FILE}" ]]; then
    require_contract_field "version"
    require_contract_field "context"
    require_contract_field "profiles_dir"
    require_contract_field "templates_dir"
    require_contract_field "workspace_layout"
    require_contract_field "edit_protocol"
    require_contract_field "governance"
    require_contract_field "llm_assistance"
    require_contract_field "project_registry"
fi

if [[ -f "${REGISTRY_FILE}" ]]; then
    require_registry_field "project_name"
    require_registry_field "governance_status"
    require_registry_field "ddd"
    require_registry_field "adr"
fi

if [[ -d "${PROFILES_DIR}" ]]; then
    mapfile -t profiles < <(find "${PROFILES_DIR}" -maxdepth 1 -name "*.yaml" | sort)
    if (( ${#profiles[@]} == 0 )); then
        echo "❌ No profile files found in ${PROFILES_DIR}"
        FAILED=1
    fi

    for profile in "${profiles[@]}"; do
        echo "Checking profile: ${profile}"
        require_profile_field "${profile}" "id"
        require_profile_field "${profile}" "name"
        require_profile_field "${profile}" "mode"
        require_profile_field "${profile}" "objective"
        require_profile_field "${profile}" "inputs"
        require_profile_field "${profile}" "allowed_actions"
        require_profile_field "${profile}" "prohibited_actions"
        require_profile_field "${profile}" "prompt_template"
    done
fi

for required in architect author editor reviewer; do
    if [[ ! -f "${PROFILES_DIR}/${required}.yaml" ]]; then
        echo "❌ Missing canonical profile: ${required}.yaml"
        FAILED=1
    fi
done

require_file "${TEMPLATES_DIR}/EDIT_REQUEST_TEMPLATE.md"

if (( FAILED != 0 )); then
    echo ""
    echo "❌ FAIL: DocOps contract audit failed."
    exit 1
fi

echo ""
echo "✅ PASS: DocOps contract is valid."
