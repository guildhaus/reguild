#!/bin/sh
# compare two snapshots modulo line numbers
a=$1; b=$2
T=$(dirname "$0")
norm() { sed -E 's/(line|\.[ch][:(])[ ]?[0-9]+/\1N/g; s/:[0-9]+\b/:N/g; s/obj=[0-9a-f]+//; s/ms=[0-9]+//g; s/cached=[a-z]+//; s/[0-9]+ ms//; s/ran of/X/; s/^rules: .*//' "$1" | sort; }
for x in check lint findings exceptions status grade phi phimany datamap deep base names units; do
  n=$(diff <(norm $a/$x.txt) <(norm $b/$x.txt) | grep -c '^[<>]')
  echo "== $x: $n differing lines"
  diff <(norm $a/$x.txt) <(norm $b/$x.txt) | head -${3:-10}
done
echo "== grades.tsv"
diff $a/grades.tsv $b/grades.tsv | head
echo "== tiebreak.tsv (best column)"
diff <(cut -f1-3,6,7 $a/tiebreak.tsv) <(cut -f1-3,6,7 $b/tiebreak.tsv) | head
echo "== exe"
python $T/exediff.py $a/GildeGold.exe $b/GildeGold.exe | tail -5
