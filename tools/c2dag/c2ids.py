"""The operand order of a commutative operation (a sum's SIB base / index, `add a,b` against `add b,a`): the
symbol ids C2 sorts by, for one source line of one function (C2 13.00.9044).

    python tools/c2dag/c2ids.py src/f3init.c init_OpenTown --line 2488          # the sums of that file line
    python tools/c2dag/c2ids.py src/f3init.c init_OpenTown --line 2488 --variants FILE --label L
    python tools/c2dag/c2ids.py src/f3init.c init_OpenTown --acct               # where the function's ids go

The rule (optimize.c, FUN_1071f5e8 -> FUN_10725527 with the comparator FUN_10724175): the operands of a
commutative tuple (add 26e, mul 270, and / or / xor 273..275, 27f, 280) are merge-sorted, stable, by DESCENDING key:
  * an expression (or a temporary with a defining tuple, `d`): key = height << 24 | nodes << 16 | hash, so the
    deeper subtree comes first (`k * size` before a plain global: index first);
  * a leaf that is no constant: key = 0x10000 | hash; a constant: hash only (last);
  * hash of a temporary without a defining tuple (type 3: a strength-reduced induction variable, a CSE
    temporary): (id << 6) & 0xffff; of any other symbol: ((id & 0x7ff) << 5) ^ (id >> 11).
The sort runs again after strength reduction (FUN_1071d3dd(fn, 2)): `base + iv` is then two leaves and the larger
hash is the first operand = the SIB base. A global's id is small (0x20 + its rank among the function's data
globals in order of first use), an optimizer temporary's hash is spread over 0..0xffc0 with period 0x400 in the
id: the iv comes first unless (id & 0x3ff) << 6 falls below the global's id << 5.

Ids are per function. Every symbol record lives in a block of 32 consecutive ids (FUN_1071c6fb), one current block
per class: locals and their pieces, data globals, C1's expression temporaries (recycled), optimizer temporaries
(FUN_10722a72: never recycled; one per distinct value-numbered expression of the whole function, then one per
strength-reduction / copy temporary in loop order). So an optimizer temporary's id is 32 * (the blocks C1's text
needed: about one per 32 expression operators, dead `if (0)` code included) + its rank. Spellings that C1 folds
(casts, NULL, ++ / += 1, parentheses, -> against (*p)., a for loop's shape with the same statements) change no
id; another statement structure does (a plain index loop for a hand pointer walk, code under a constant-false
condition). --acct prints the three counts.
"""
import argparse, os, re, shutil, subprocess, sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(os.path.dirname(HERE))
sys.path.insert(0, HERE)
import c2trace

WORK = os.path.join(ROOT, "build", "c2dag", "ids")
BIN = os.path.join(WORK, "bin")
VC, DX = c2trace.VC, c2trace.DX
COMMUTATIVE = {"26e": "add", "270": "mul", "273": "and", "274": "or", "275": "xor", "27f": "27f", "280": "280"}
PHASE = {"1cd72": "as C1 wrote it", "3e0ee": "after the first simplification", "1ce7b": "after loop optimisation",
         "27dc5": "after the second simplification (the order the code has)"}


def setup():
    src = os.path.join(HERE, "c2ids.c")
    dll = os.path.join(BIN, "C2.DLL")
    if os.path.exists(dll) and os.path.getmtime(dll) >= os.path.getmtime(src):
        return
    os.makedirs(BIN, exist_ok=True)
    vb = VC + r"\VC98\Bin"
    for f in os.listdir(vb):
        if re.match(r"(?i)(cl\.exe|c1\.dll|.*\.err|cl\.cfg)$", f):
            shutil.copy2(os.path.join(vb, f), os.path.join(BIN, f))
    shutil.copy2(os.path.join(vb, "C2.DLL"), os.path.join(BIN, "c2real.dll"))
    r = subprocess.run([vb + r"\cl.exe", "/nologo", "/LD", "/O1", src, "/Fe" + os.path.join(WORK, "c2.dll"),
                        "/Fo" + os.path.join(WORK, "c2ids.obj"), "/link", "/BASE:0x20000000", "user32.lib"],
                       env=c2trace.env(), cwd=WORK, capture_output=True, text=True)
    if r.returncode:
        raise SystemExit("building the proxy failed:\n" + r.stdout)
    shutil.copy2(os.path.join(WORK, "c2.dll"), dll)


def run(c, func, lo, hi, acct=False):
    """compile c with the proxy; the proxy's output"""
    setup()
    out = os.path.splitext(c)[0] + ".ids"
    if os.path.exists(out):
        os.remove(out)
    e = c2trace.env()
    e.update(C2IDS_OUT=out, C2IDS_FUNC=func, C2IDS_LO=str(lo), C2IDS_HI=str(hi))
    if acct:
        e["C2IDS_ACCT"] = "1"
    r = subprocess.run([os.path.join(BIN, "CL.EXE"), "/nologo", "/c", "/O2", "/Gy", "/I" + os.path.join(ROOT, "include"),
                        "/I" + os.path.join(ROOT, "src"), "/I" + DX, "/Fa" + os.path.splitext(c)[0] + ".asm",
                        "/Fo" + os.path.splitext(c)[0] + ".obj", c], env=e, cwd=ROOT, capture_output=True, text=True)
    if r.returncode:
        raise SystemExit("compile failed:\n" + "\n".join(l for l in r.stdout.splitlines() if "error" in l))
    return open(out, encoding="latin-1").read() if os.path.exists(out) else ""


def key(kind, sid, typ, has_def):
    """an operand's sort key as text, and its value when it is a leaf's (None: an expression's key)"""
    if kind in (7, 8, 9):
        return "constant (last)", 0
    if kind not in (1, 2):
        return "expression", None
    if has_def:
        return "expression (its defining tuple's key)", None
    if typ == 3:
        k = 0x10000 | (sid << 6) & 0xffff
    else:
        x = (sid & 0xffff) ^ (sid >> 16)
        k = 0x10000 | (((x << 5) >> 16) ^ ((x & 0x7ff) << 5)) & 0xffff
    return "key %x" % k, k


def sums(out, src):
    seen = []
    for part in out.split("== before ")[1:]:
        head, _, body = part.partition("\n")
        rows = []
        for l in body.split("\n"):
            m = re.match(r"\s+op (\w+)@(-?\d+) U:(.*) D:(.*)$", l)
            if not m or m.group(1) not in COMMUTATIVE:
                continue
            ops = []
            for k, rest in re.findall(r"\[k(\w+)([^\]]*)\]", m.group(3)):
                s = re.match(r" (\S+) id=(\w+) t=(\w+)( d)?", rest)
                if s:
                    text, val = key(int(k, 16), int(s.group(2), 16), int(s.group(3), 16), bool(s.group(4)))
                    nm = s.group(1) if s.group(1) != "#" else ("temp" if s.group(3) == "3" else "sym")
                    ops.append("%s#%s: %s" % (nm, s.group(2), text))
                else:
                    ops.append("k%s: %s" % (k, key(int(k, 16), 0, 0, False)[0]))
            rows.append("  %s @%d  %s" % (COMMUTATIVE[m.group(1)], src.ln(int(m.group(2))) if m.group(2) != "0" else 0,
                                          "  |  ".join(ops)))
        if head.strip() == "2812f":     # from here on the operands are registers
            break
        if rows and rows != seen:
            print("%s:" % PHASE.get(head.strip(), "before FUN_107%05x" % int(head.strip(), 16)))
            print("\n".join(rows))
            seen = rows
    if seen:
        print("(a state not printed again did not change; the last one is the order the code has)")


def acct(out):
    c = {"0": 0, "1": 0, "2": 0, "3": 0}
    first4 = None
    vn = later = 0
    for l in out.split("\n"):
        m = re.match(r"BLOCK firstid (\w+) class (\d)", l)
        if m and first4 is None:
            if m.group(2) == "4":
                first4 = int(m.group(1), 16)
            else:
                c[m.group(2)] += 1
        elif l == "TEMP vn":
            vn += 1
        elif l == "TEMP later":
            later += 1
    print("blocks of 32 ids before the optimizer's first temporary: %d for C1's expression temporaries, %d for "
          "locals and their pieces, %d for globals and others; the optimizer's first id: %s" % (
              c["0"], c["1"], c["2"] + c["3"], "none" if first4 is None else "0x%x" % first4))
    print("optimizer temporaries: %d value numbers (one per distinct expression of the function), %d later "
          "(strength reduction, copies; in loop order)" % (vn, later))


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("src"); ap.add_argument("func")
    ap.add_argument("--line", type=int, help="a file line of the traced text (the tree's, or the variant's copy)")
    ap.add_argument("--variants", help="a dcp try variants file: one of its variants instead of the tree's text")
    ap.add_argument("--label", help="the variant (default: the first)")
    ap.add_argument("--acct", action="store_true", help="where the function's ids go")
    a = ap.parse_args()
    if a.line is None and not a.acct:
        raise SystemExit("give --line N or --acct")
    os.makedirs(WORK, exist_ok=True)
    text = open(a.src, encoding="latin-1", newline="").read().replace("\r\n", "\n")
    tag = os.path.splitext(os.path.basename(a.src))[0]
    if a.variants:
        text, lab = c2trace.variant_text(text, a.func, a.variants, a.label, src=a.src)
        tag += "_" + re.sub(r"\W", "_", lab)
    c = os.path.join(WORK, tag + ".c")
    with open(c, "w", encoding="latin-1", newline="\n") as f:
        f.write(text)
    src = c2trace.Src(c, a.func)
    if a.acct:
        acct(run(c, a.func, -1, -1, acct=True))
    if a.line is not None:
        rel = a.line - 1 - src.d
        print("line %d of %s: %s" % (a.line, os.path.relpath(c, ROOT), src.txt(rel, 100)))
        sums(run(c, a.func, rel, rel), src)


if __name__ == "__main__":
    main()
