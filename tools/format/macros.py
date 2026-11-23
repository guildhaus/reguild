import re, glob, os
os.chdir(os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__)))))  # the repo root
BS = chr(92)
defs = {}
for f in sorted(glob.glob('src/*') + glob.glob('include/*')):
    lines = open(f, encoding='utf-8').read().split('\n')
    i = 0
    while i < len(lines):
        l = lines[i]
        m = re.match(r'\s*#\s*define\s+(\w+)(\([^)]*\))?(.*)', l)
        if m:
            start = i
            body = l
            while body.endswith(BS):
                i += 1
                body = body[:-1] + ' ' + lines[i]
            name = m.group(1)
            params = [p.strip() for p in (m.group(2) or '()')[1:-1].split(',') if p.strip()]
            text = body[m.end(2) if m.group(2) else m.end(1):]
            text_nc = re.sub(r'/\*.*?\*/', '', text)
            text_nc = re.sub(r'"(\\.|[^"\\])*"', '""', text_nc)
            strz = [p for p in params if re.search(r'(?<!#)#\s*' + re.escape(p) + r'\b', text_nc)]
            defs.setdefault(name, []).append((f, start + 1, i + 1, params, strz, '__LINE__' in text_nc, text_nc))
        i += 1
line_users = {n for n, v in defs.items() if any(d[5] for d in v)}
str_users = {n for n, v in defs.items() if any(d[4] for d in v)}
changed = True
while changed:
    changed = False
    for n, v in defs.items():
        for d in v:
            toks = set(re.findall(r'\b\w+\b', d[6]))
            if n not in line_users and toks & line_users:
                line_users.add(n); changed = True
            if n not in str_users and toks & str_users:
                str_users.add(n); changed = True
print('LINE macros:')
for n in sorted(line_users):
    for d in defs[n]:
        print('  ', n, d[0], d[1], d[2])
print('STRINGIFY macros:')
for n in sorted(str_users):
    for d in defs[n]:
        print('  ', n, d[0], d[1], d[2], d[4])
import json
json.dump({'line': sorted(line_users), 'str': sorted(str_users)}, open(os.path.join(os.path.dirname(os.path.abspath(__file__)), 'macros.json'), 'w'))
