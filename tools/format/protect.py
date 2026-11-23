"""Find the line ranges clang-format must not touch and (with --apply) wrap them in
/* clang-format off */ ... /* clang-format on */.

Ranges:
  asm   : a #define whose body holds __asm; a run of statement lines holding __asm; an __asm { } block
  line  : from a #line directive to the end of the last statement before the next #line (or the
          file's end) that uses __LINE__ or a macro expanding to it (D3DCHECK, err_Log(__FILE__, __LINE__..)):
          clang-format must keep the line count between them (the pushed line numbers)
  str   : a statement using a stringifying macro (D3DCHECK's #call) outside the above
"""
import re, sys, os, glob, json

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))  # the repo root
BS = chr(92)
HERE = os.path.dirname(os.path.abspath(__file__))
OFF = '/* clang-format off */'
ON = '/* clang-format on */'
LINE_USERS = {'D3DCHECK', '__LINE__', 'assert'}  # in code (the asm label macros only name labels)
STR_USERS = {'D3DCHECK', 'assert'}
# files where a formatted line count let two asm labels of one function meet (a #line between them): their
# #line spans also cover the label macros' uses (found by the compile error C2412)
LABEL_FILES = {'src/d3_d3d.c'}


TRAIL = {}  # line index -> (end line index, comment text) of the first comment starting after code on that line


def strip(lines):
    """code of each line with comments and string/char literals blanked"""
    out = []
    inc = False
    TRAIL.clear()
    cur = None
    for k, l in enumerate(lines):
        s = []
        i = 0
        n = len(l)
        while i < n:
            if inc:
                j = l.find('*/', i)
                if j < 0:
                    if cur is not None:
                        TRAIL[cur[0]][1].append(l[i:])
                    i = n
                else:
                    if cur is not None:
                        TRAIL[cur[0]][1].append(l[i:j])
                        TRAIL[cur[0]][0] = k
                        cur = None
                    inc = False
                    i = j + 2
                continue
            c = l[i]
            if l.startswith('/*', i):
                if ''.join(s).strip() and k not in TRAIL:
                    TRAIL[k] = [k, []]
                    cur = (k,)
                inc = True
                i += 2
                s.append(' ')
                continue
            if l.startswith('//', i):
                if ''.join(s).strip() and k not in TRAIL:
                    TRAIL[k] = [k, [l[i + 2:]]]
                break
            if c in '"' + "'":
                q = c
                i += 1
                while i < n and l[i] != q:
                    if l[i] == BS:
                        i += 1
                    i += 1
                i += 1
                s.append('""')
                continue
            s.append(c)
            i += 1
        out.append(''.join(s))
    return out


_DEPTH = {}


def depth_at(code, k):
    """brace depth at the start of line k"""
    key = id(code)
    if key not in _DEPTH:
        ds = [0]
        d = 0
        for l in code:
            d += l.count('{') - l.count('}')
            ds.append(d)
        _DEPTH.clear()
        _DEPTH[key] = ds
    return _DEPTH[key][k]


def stmt_end(code, i):
    """last line index of the statement starting at line i (first ';' at paren depth 0)"""
    depth = 0
    j = i
    while j < len(code):
        for ch in code[j]:
            if ch in '([{':
                depth += 1
            elif ch in ')]}':
                depth -= 1
            elif ch == ';' and depth <= 0:
                return j
        j += 1
    return i


def ranges(path):
    text = open(path, encoding='utf-8', newline='').read()
    lines = text.split('\n')
    code = strip(lines)
    rs = []
    # macros: a #define whose (continued) body holds __asm
    i = 0
    defs = set()
    while i < len(lines):
        if re.match(r'\s*#\s*define\b', code[i]):
            s = i
            while lines[i].endswith(BS):
                i += 1
            body = ' '.join(code[s:i + 1])
            for k in range(s, i + 1):
                defs.add(k)
            if re.search(r'\b_?_asm\b', body):
                rs.append((s, i, 'asm-macro'))
        i += 1
    # statement lines with __asm (outside #defines)
    i = 0
    while i < len(lines):
        if i not in defs and re.search(r'\b_?_asm\b', code[i]):
            s = i
            if re.search(r'\b_?_asm\s*\{', code[i]) or (re.search(r'\b_?_asm\s*$', code[i]) and i + 1 < len(lines) and code[i + 1].strip().startswith('{')):
                depth = 0
                j = i
                started = False
                while j < len(lines):
                    for ch in code[j]:
                        if ch == '{':
                            depth += 1
                            started = True
                        elif ch == '}':
                            depth -= 1
                    if started and depth <= 0:
                        break
                    j += 1
                rs.append((s, j, 'asm-block'))
                i = j + 1
                continue
            j = i
            while j + 1 < len(lines) and (j + 1) not in defs and re.search(r'\b_?_asm\b', code[j + 1]):
                j += 1
            # an __asm statement has no ';': clang-format reads the next statement as its continuation; unless a '}'
            # follows, keep the whole function body that holds it (in its own layout)
            nx = next((k for k in range(j + 1, len(lines)) if code[k].strip()), None)
            if nx is not None and not code[nx].strip().startswith(('}', '#')):
                fs = s
                while fs > 0 and depth_at(code, fs) > 0:
                    fs -= 1
                fe = j
                while fe + 1 < len(lines) and depth_at(code, fe + 1) != 0:
                    fe += 1
                # fs: the line that opens the function body, fe: the one that closes it; keep what is between
                rs.append((fs + 1, fe - 1, 'asm-fn'))
            else:
                rs.append((s, j, 'asm-lines'))
            i = j + 1
            continue
        i += 1
    # #line spans
    dirs = [k for k in range(len(lines)) if re.match(r'\s*#\s*line\b', code[k])]
    # in a file with #line, the asm label macros (labels named by __LINE__) keep their remapped lines too:
    # the original numbering keeps them apart within a function, a formatted one may not
    lu = LINE_USERS | (set(json.load(open(os.path.join(HERE, 'macros.json')))['line']) if path in LABEL_FILES else set())
    uses = [k for k in range(len(lines)) if k not in defs and not code[k].lstrip().startswith('#') and any(re.search(r'\b' + u + r'\b', code[k]) for u in lu)]
    covered = set()
    for n, d in enumerate(dirs):
        nxt = dirs[n + 1] if n + 1 < len(dirs) else len(lines)
        us = [u for u in uses if d < u < nxt]
        if us:
            e = stmt_end(code, us[-1])
            rs.append((d, e, 'line'))
            for u in us:
                covered.add(u)
    for u in uses:
        if u not in covered:
            print('WARNING %s:%d __LINE__ use without a #line before it' % (path, u + 1), file=sys.stderr)
    # stringification uses outside the above
    for k in range(len(lines)):
        if k not in defs and not code[k].lstrip().startswith('#') and any(re.search(r'\b' + u + r'\b', code[k]) for u in STR_USERS):
            rs.append((k, stmt_end(code, k), 'str'))
    # dcp reads some comments by the line they start on: a trailing @original / @vc6 marker must stay on its
    # declaration's / call's first line (lint exemptions; exceptions read the why to the line's end), an address
    # comment behind `= {` must stay on the definition's first line (data attach), a view macro must stay one line
    for k, (ce, parts) in sorted(TRAIL.items()):
        body = ' '.join(parts)
        long_ = len(lines[k].expandtabs(4)) > 80
        marker = re.search(r'@(original|vc6)\b', body)
        addr = re.match(r'\s*([A-Za-z][A-Za-z0-9_-]*:)?0x[0-9a-fA-F]+', body)
        if k in defs:
            if (addr or marker) and (long_ or ce > k):
                e = k
                while lines[e].endswith(BS):
                    e += 1
                rs.append((k, max(e, ce), 'define-comment'))
        elif re.search(r'=\s*\{\s*$', code[k]) and (addr or marker):
            rs.append((k, max(stmt_end(code, k), ce), 'brace-comment'))
        elif marker and long_:
            rs.append((k, max(stmt_end(code, k), ce), 'marker'))
    # merge
    rs.sort()
    merged = []
    for s, e, w in rs:
        if merged and s <= merged[-1][1] + 1:
            ps, pe, pw = merged[-1]
            merged[-1] = (ps, max(pe, e), pw if w in pw else pw + '+' + w)
        else:
            merged.append((s, e, w))
    return lines, merged


def apply(path, lines, merged):
    out = []
    starts = {s: w for s, e, w in merged}
    ends = {e for s, e, w in merged}
    for k, l in enumerate(lines):
        if k in starts:
            out.append(OFF)
        out.append(l)
        if k in ends:
            out.append(ON)
    open(path, 'w', encoding='utf-8', newline='').write('\n'.join(out))


if __name__ == '__main__':
    os.chdir(ROOT)
    files = sys.argv[2:] if len(sys.argv) > 2 else sorted(glob.glob('src/*.c') + glob.glob('include/*.h'))
    do = sys.argv[1] == '--apply'
    total = 0
    for f in files:
        f = f.replace(os.sep, '/')
        lines, merged = ranges(f)
        if any(OFF in l for l in lines):
            print('SKIP (already protected)', f)
            continue
        for s, e, w in merged:
            print('%s:%d-%d %s (%d lines)' % (f, s + 1, e + 1, w, e - s + 1))
            total += e - s + 1
        if do and merged:
            apply(f, lines, merged)
    print('protected lines:', total, file=sys.stderr)
