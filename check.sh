#!/bin/bash
# Find violations of Power of Ten principles

echo "=== Magic Numbers (uses of hardcoded literals) ==="
grep -rn "[0-9]\{3,\}" src/ include/ | grep -v "px\|// " | head -20

echo "=== Global functions (not in class/namespace) ==="
grep -rn "^[a-zA-Z].*(.*).*{" src/*.cpp | grep -v "^\s*\(void\|int\|float\|bool\|std\)" | head -20

echo "=== Unchecked memory access ([], at without bounds) ==="
grep -rn "\[.*\]" src/ include/ | grep -v "\.at(" | head -20

echo "=== Long functions (>200 lines) ==="
for f in src/*.cpp; do
  lines=$(wc -l < "$f")
  if [ "$lines" -gt 200 ]; then
    echo "$f: $lines lines"
  fi
done

echo "=== Missing const correctness ==="
grep -rn "void.*(" include/ | grep -v "const" | head -10

echo "=== Raw pointers (new/delete) ==="
grep -rn "\<new\>\|\<delete\>" src/ include/ | head -20
