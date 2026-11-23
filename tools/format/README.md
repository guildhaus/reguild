# tools/format: reformat the sources with clang-format, binary unchanged

The sources follow `.clang-format` (LLVM style, includes unsorted, comments only re-indented). Day-to-day edits
are not formatted; run this now and then over the whole tree.

    sh tools/format/snap.sh SNAP_BEFORE        # dcp build + every dcp output + grades + linked exe
    sh tools/format/format.sh [FILE...]        # default: src/*.c include/*.h
    dcp build --force
    sh tools/format/snap.sh SNAP_AFTER
    bash tools/format/cmp.sh SNAP_BEFORE SNAP_AFTER

Accept the run only when cmp.sh shows no grade change (tie-break flips of functions marked sensitive in
ledger/tiebreak.tsv aside), no dcp output change beyond line numbers, and an exe that differs only in the PE
timestamp. Then add the formatting commit to .git-blame-ignore-revs.

- `format.sh`: protect.py --apply, clang-format -i three times (fixpoint), reindent.py. clang-format comes
  from `$CLANG_FORMAT`, PATH, or the LLVM default install.
- `protect.py`: wraps what clang-format must not touch in `/* clang-format off */ ... /* clang-format on */`:
  `__asm` macros, blocks and statement runs; functions with a bare `__asm int 3`; `#line` spans up to the last
  `__LINE__` user (D3DCHECK, err_Log(__FILE__, __LINE__ ..), assert: the pushed line numbers); stringifying
  macro calls (D3DCHECK's `#call`, assert's `#exp`: the texts are in the binary); trailing `@original` / `@vc6`
  markers on long lines; `= { /* 0xADDR */` data tables; one-line view macros. Without `--apply` it lists them.
- `macros.py`: finds the `__LINE__` and stringifying macros, writes `macros.json` (read by protect.py).
- `reindent.py`: shifts short protected regions to the indentation of the code around them.
- `snap.sh`, `cmp.sh`, `exediff.py`: the before/after snapshot, comparison (modulo line numbers) and the
  byte-for-byte exe comparison by section.
