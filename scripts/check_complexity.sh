#!/usr/bin/env sh
set -eu

threshold="${1:-40}"

if git rev-parse --is-inside-work-tree >/dev/null 2>&1; then
  base="${GITHUB_BASE_REF:-}"
  if [ -n "$base" ] && git rev-parse "origin/$base" >/dev/null 2>&1; then
    files=$(git diff --name-only "origin/$base"...HEAD -- 'src/*.cpp' 'src/*.hpp' | sort)
  elif git rev-parse HEAD^ >/dev/null 2>&1; then
    files=$(git diff --name-only HEAD^ HEAD -- 'src/*.cpp' 'src/*.hpp' | sort)
  else
    files=$(find src -type f \( -name '*.cpp' -o -name '*.hpp' \) | sort)
  fi
else
  files=$(find src -type f \( -name '*.cpp' -o -name '*.hpp' \) | sort)
fi

if [ -z "$files" ]; then
  echo "No changed C++ files to complexity-check."
  exit 0
fi

if command -v lizard >/dev/null 2>&1; then
  lizard $files -C "$threshold"
  exit $?
fi

echo "lizard not found; using lightweight cyclomatic proxy." >&2
violations=$(awk -v threshold="$threshold" '
  BEGIN { file=""; fn=""; count=0; line=0; }
  FNR == 1 { file=FILENAME; }
  /^[[:space:]]*[A-Za-z_][A-Za-z0-9_:<>,&*[:space:]]+[[:space:]]+[A-Za-z_][A-Za-z0-9_:]*[[:space:]]*\(.*\)[[:space:]]*(const)?[[:space:]]*\{/ {
    fn=$0; count=1; line=FNR;
  }
  fn != "" && /\b(if|for|while|case|catch)\b|&&|\|\|/ { count++; }
  fn != "" && /^\}/ {
    if (count > threshold) print file ":" line " complexity_proxy=" count " " fn;
    fn=""; count=0; line=0;
  }
' $files)

if [ -n "$violations" ]; then
  echo "$violations"
  exit 1
fi
