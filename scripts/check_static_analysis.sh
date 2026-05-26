#!/usr/bin/env sh
set -eu

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
  echo "No changed C++ files to static-analyze."
  exit 0
fi

if command -v cppcheck >/dev/null 2>&1; then
  cppcheck --enable=warning,style,performance,portability \
    --inline-suppr \
    --suppress=missingIncludeSystem \
    --error-exitcode=1 \
    -Isrc $files
elif command -v clang-tidy >/dev/null 2>&1; then
  if [ ! -f compile_commands.json ]; then
    echo "compile_commands.json not found; configure CMake with -DCMAKE_EXPORT_COMPILE_COMMANDS=ON." >&2
    exit 2
  fi
  printf '%s\n' $files | grep -E '\.cpp$' | xargs -r clang-tidy -p .
else
  echo "Neither cppcheck nor clang-tidy found." >&2
  exit 2
fi
