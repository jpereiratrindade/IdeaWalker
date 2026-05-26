#!/usr/bin/env sh
set -eu

tmp="${TMPDIR:-/tmp}/ideawalker_include_edges.$$"
trap 'rm -f "$tmp"' EXIT

find src -type f \( -name '*.cpp' -o -name '*.hpp' -o -name '*.h' \) | sort |
while IFS= read -r file; do
  dir=$(dirname "$file")
  grep -E '^[[:space:]]*#include[[:space:]]+"[^"]+"' "$file" 2>/dev/null |
  sed -E 's/^[^"]*"([^"]+)".*/\1/' |
  while IFS= read -r inc; do
    target=""
    if [ -f "src/$inc" ]; then target="src/$inc"; fi
    if [ -z "$target" ] && [ -f "$dir/$inc" ]; then target="$dir/$inc"; fi
    if [ -n "$target" ]; then printf '%s\t%s\n' "$file" "$target"; fi
  done
done > "$tmp"

awk -F '\t' '
  { edges[$1, ++out[$1]] = $2; nodes[$1]=1; nodes[$2]=1; }
  function visit(n, i, m) {
    if (stack[n]) { print "include cycle involving " n; found=1; return; }
    if (seen[n]) return;
    seen[n]=1; stack[n]=1;
    for (i=1; i<=out[n]; i++) visit(edges[n, i]);
    stack[n]=0;
  }
  END {
    for (n in nodes) visit(n);
    exit found ? 1 : 0;
  }
' "$tmp"
