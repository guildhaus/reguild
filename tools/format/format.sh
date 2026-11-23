#!/bin/sh
# Reformat the C sources with clang-format without changing the binary:
# protect the hazard regions, format to a fixpoint, re-indent the protected regions.
# usage: tools/format/format.sh [FILE...]   (default: src/*.c include/*.h)
# clang-format: $CLANG_FORMAT, else clang-format on PATH, else the LLVM default install.
T=$(cd "$(dirname "$0")" && pwd)
cd "$T/../.." || exit 1
CF=${CLANG_FORMAT:-$(command -v clang-format || echo "/c/Program Files/LLVM/bin/clang-format.exe")}
if [ $# -eq 0 ]; then set -- src/*.c include/*.h; fi
python "$T/protect.py" --apply "$@" > /dev/null || exit 1
for i in 1 2 3; do "$CF" -i "$@"; done
python "$T/reindent.py" "$@"
echo "files clang-format would still change: $("$CF" --dry-run "$@" 2>&1 | grep -c warning)"
