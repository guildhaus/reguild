"""Shift each /* clang-format off */ region left so that its first code line sits at the indentation clang-format
gave the off comment (leading white space only; relative indentation inside the region kept). Regions with a
line continuation (macros, continued strings) are left alone."""
import sys, os, glob, re

BS = chr(92)
os.chdir(os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__)))))  # the repo root
files = sys.argv[1:] or sorted(glob.glob('src/*.c') + glob.glob('include/*.h'))
n = 0
for f in files:
    lines = open(f, encoding='utf-8', newline='').read().split('\n')
    out = list(lines)
    i = 0
    changed = False
    while i < len(lines):
        if lines[i].strip() == '/* clang-format off */':
            j = i + 1
            while lines[j].strip() != '/* clang-format on */':
                j += 1
            body = range(i + 1, j)
            # a whole function body (ends at the function's closing brace) keeps its own layout
            whole = j + 1 < len(lines) and lines[j + 1] == '}' and lines[i - 1].endswith('{')
            if not whole and not any(lines[k].endswith(BS) for k in body):
                want = len(lines[i]) - len(lines[i].lstrip(' '))
                first = next((k for k in body if lines[k].strip() and not lines[k].lstrip().startswith('#')), None)
                if first is not None and '\t' not in ''.join(lines[k][:len(lines[k]) - len(lines[k].lstrip())] for k in body):
                    have = len(lines[first]) - len(lines[first].lstrip(' '))
                    d = have - want
                    if d > 0 and all(not lines[k].strip() or lines[k].lstrip().startswith('#') or lines[k].startswith(' ' * d) for k in body):
                        for k in body:
                            if lines[k].strip() and not lines[k].lstrip().startswith('#'):
                                out[k] = lines[k][d:]
                        changed = True
                        n += 1
            i = j
        i += 1
    if changed:
        open(f, 'w', encoding='utf-8', newline='').write('\n'.join(out))
print('regions shifted:', n)
