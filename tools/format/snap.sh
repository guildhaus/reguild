#!/bin/sh
# snapshot dcp's parse-level outputs into $1 (a directory)
out=$(mkdir -p "$1" && cd "$1" && pwd)
cd "$(dirname "$0")/../.." || exit 1
dcp build > "$out/build.txt" 2>&1
dcp check > "$out/check.txt" 2>&1
dcp lint > "$out/lint.txt" 2>&1
dcp findings > "$out/findings.txt" 2>&1
dcp exceptions > "$out/exceptions.txt" 2>&1
dcp status > "$out/status.txt" 2>&1
dcp grade > "$out/grade.txt" 2>&1
dcp phi > "$out/phi.txt" 2>&1
dcp phi --many > "$out/phimany.txt" 2>&1
dcp data map > "$out/datamap.txt" 2>&1
dcp next --deep --peek -n 1000 > "$out/deep.txt" 2>&1
dcp next --peek -n 1000 > "$out/base.txt" 2>&1
dcp next --names --peek -n 1000 > "$out/names.txt" 2>&1
: > "$out/units.txt"
for u in $(grep -o 'unit=[A-Za-z0-9_]*' "$out/grade.txt" | cut -d= -f2 | sort -u); do
  echo "## $u" >> "$out/units.txt"
  dcp data map "$u" >> "$out/units.txt" 2>&1
  dcp grade "$u" >> "$out/units.txt" 2>&1
done
cp ledger/grades.tsv ledger/tiebreak.tsv "$out/"
cp build/link/exe/GildeGold.exe "$out/"
