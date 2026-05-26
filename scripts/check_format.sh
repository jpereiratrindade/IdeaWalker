#!/usr/bin/env sh
set -eu

if ! command -v clang-format >/dev/null 2>&1; then
  echo "clang-format not found; install clang-format to run format check." >&2
  exit 2
fi

if git rev-parse --is-inside-work-tree >/dev/null 2>&1; then
  base="${GITHUB_BASE_REF:-}"
  if [ -n "$base" ] && git rev-parse "origin/$base" >/dev/null 2>&1; then
    files=$(git diff --name-only "origin/$base"...HEAD -- 'src/*.cpp' 'src/*.hpp' 'src/*.h' | sort)
  elif git rev-parse HEAD^ >/dev/null 2>&1; then
    files=$(git diff --name-only HEAD^ HEAD -- 'src/*.cpp' 'src/*.hpp' 'src/*.h' | sort)
  else
    files=$(find src -type f \( -name '*.cpp' -o -name '*.hpp' -o -name '*.h' \) | sort)
  fi
else
  files=$(find src -type f \( -name '*.cpp' -o -name '*.hpp' -o -name '*.h' \) | sort)
fi
if [ -z "$files" ]; then
  echo "No changed C++ files to format-check."
  exit 0
fi

clang-format --dry-run --Werror $files
