"""The id meter: for each variant of a dcp try file, the symbol ids that order one statement's commutative
operands (see c2ids.py for the rule) and whether the variant's code is otherwise the tree's.

    python tools/c2dag/idmeter.py src/f3init.c init_OpenTown --stmt "b = &ai_Blds[k];" --nth 3 --variants FILE [LABEL ..]

--stmt TEXT / --nth N: the statement, as the N-th line of the function that contains TEXT (default the last), found
again in every variant (a line number would move). Without --variants: the tree alone.
Per row: the statement's sums after loop optimisation as `operand#id:key`, first operand first as C2 will order them
(the larger key first; `expr` = an expression, always before a leaf), and `code: same` when the function's listing
equals the tree's up to generated label names and the operand order inside `[a+b]` (so only the ids moved),
else the number of differing lines.
Use: aim a code-neutral change of the IL volume (another loop form, a disabled block) at the id window a base /
index order needs, before `dcp try` confirms the bytes (init_OpenTown: the walk's temporary at 0x800..0x810).
The proxy is c2ids.c, built by c2ids.py into build/c2dag/ids.
"""
import argparse, os, re, sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import c2ids, c2trace


def listing(asm, func):
    out, on = [], False
    for l in open(asm, encoding="latin-1"):
        if re.match(r"[_@]%s(@\d+)?\s+PROC" % re.escape(func), l):
            on = True
        elif re.match(r"[_@]%s(@\d+)?\s+ENDP" % re.escape(func), l):
            on = False
        elif on and re.match(r"\t[a-z]", l):
            s = re.sub(r"\s+", " ", l.split(";")[0].strip())
            s = re.sub(r"\$[A-Z]*\d+", "$", s)
            out.append(re.sub(r"\[(e[a-z]{2})\+(e[a-z]{2})(?=[\]+-])", lambda m: "[" + "+".join(sorted(m.groups())), s))
    return out


def measure(text, func, stmt, nth, tag):
    c = os.path.join(c2ids.WORK, tag + ".c")
    with open(c, "w", encoding="latin-1", newline="\n") as f:
        f.write(text)
    src = c2trace.Src(c, func)
    hits = [rel for rel, t in sorted(src.text.items()) if stmt in t]
    if not hits:
        return "the statement is not in this text", None
    rel = hits[nth - 1] if nth else hits[-1]
    out = c2ids.run(c, func, rel, rel)
    rows = []
    for part in out.split("== before ")[1:]:
        head, _, body = part.partition("\n")
        if head.strip() != "1ce7b":     # the state after loop optimisation, before the second simplification
            continue
        for l in body.split("\n"):
            m = re.match(r"\s+op (\w+)@(-?\d+) U:(.*) D:", l)
            if not m or m.group(1) not in c2ids.COMMUTATIVE:
                continue
            ops = []
            for k, rest in re.findall(r"\[k(\w+)([^\]]*)\]", m.group(3)):
                s = re.match(r" (\S+) id=(\w+) t=(\w+)( d)?", rest)
                if not s:
                    ops.append((0 if int(k, 16) in (7, 8, 9) else 1 << 40, "const" if int(k, 16) in (7, 8, 9) else "expr"))
                    continue
                _, val = c2ids.key(int(k, 16), int(s.group(2), 16), int(s.group(3), 16), bool(s.group(4)))
                nm = s.group(1) if s.group(1) != "#" else "temp"
                ops.append((1 << 40, "expr") if val is None else (val, "%s#%s:%x" % (nm, s.group(2), val)))
            ops.sort(key=lambda o: -o[0])      # stable, descending: C2's order
            rows.append(" + ".join(o[1] for o in ops))
    return "; ".join(rows) or "no commutative tuple on that line", listing(os.path.splitext(c)[0] + ".asm", func)


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("src"); ap.add_argument("func")
    ap.add_argument("--stmt", required=True); ap.add_argument("--nth", type=int, default=0)
    ap.add_argument("--variants"); ap.add_argument("labels", nargs="*")
    a = ap.parse_args()
    os.makedirs(c2ids.WORK, exist_ok=True)
    tree = open(a.src, encoding="latin-1", newline="").read().replace("\r\n", "\n")
    tag = os.path.splitext(os.path.basename(a.src))[0]
    row, base = measure(tree, a.func, a.stmt, a.nth, tag)
    print("%-16s %s" % ("(tree)", row))
    if not a.variants:
        return
    labels = a.labels or list(c2trace.parse_variants(open(a.variants, encoding="latin-1").read()))
    for lab in labels:
        try:
            text, _ = c2trace.variant_text(tree, a.func, a.variants, lab, src=a.src)
            row, L = measure(text, a.func, a.stmt, a.nth, tag + "_" + re.sub(r"\W", "_", lab))
        except SystemExit as e:
            print("%-16s %s" % (lab, str(e).split("\n")[0][:120])); continue
        if L is None or base is None:
            code = "-"
        elif L == base:
            code = "same"
        else:
            code = "%d lines differ (%d / %d instructions)" % (
                sum(1 for x, y in zip(L, base) if x != y) + abs(len(L) - len(base)), len(L), len(base))
        print("%-16s %s   code: %s" % (lab, row, code))


if __name__ == "__main__":
    main()
