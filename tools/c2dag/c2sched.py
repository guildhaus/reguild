"""The scheduler of C2 traced and patched for one function (proxy: c2sched.c; C2 13.00.9044).

    python tools/c2dag/c2sched.py trace src/d3_tools.c d3t_UpdateSnow [--variants FILE --label L]
        the scheduling graph of the function's blocks and every pick of the cycle scheduler (`sel` / ` cons`)
    python tools/c2dag/c2sched.py hack src/d3_tools.c d3t_UpdateSnow SPEC [SPEC ..] [--show N] [--want FILE]
        the function compiled once per SPEC with patched scheduler decisions, each listing compared with the
        unpatched one (or with the listing in FILE, one instruction per line: the original's order)
    python tools/c2dag/c2sched.py wall SPEC [UNIT ..]
        every unit (or the named ones) compiled with and without the weight override SPEC (C2DAG_W): the functions
        whose listing changes, with their grade from ledger/grades.tsv

A hack SPEC is parts joined by `+`: `81,5` region lengths (C2DAG_CUT), `hR:I:D` priority of node I of region R
plus D (C2DAG_HACK), `lR:I:J:D` latency of the edge I -> J plus D (C2DAG_LAT), `gN` processor model (C2DAG_CPU),
`f` the x87 pre-pass off (C2DAG_FPOFF), `-` nothing. Node and region numbers are those of `trace`.
Use: an order hunk no source variant moves. Patch the decision (a priority unit, a latency, the 81-tuple cut)
until the listing is the original's; that names what the source has to change, or shows that no text reaches it.

The proxy is built on first use into build/c2dag/sched (cl.exe, c1.dll and the real C2.DLL are copied there);
nothing built belongs in the tree.
"""
import glob, os, re, shutil, subprocess, sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(os.path.dirname(HERE))
sys.path.insert(0, HERE)
import c2trace

VC, DX = c2trace.VC, c2trace.DX
WORK = os.path.join(ROOT, "build", "c2dag", "sched")
BIN = os.path.join(WORK, "bin")


def setup():
    src = os.path.join(HERE, "c2sched.c")
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
                        "/Fo" + os.path.join(WORK, "c2sched.obj"), "/link", "/BASE:0x20000000", "user32.lib"],
                       env=c2trace.env(), cwd=WORK, capture_output=True, text=True)
    if r.returncode:
        raise SystemExit("building the proxy failed:\n" + r.stdout)
    shutil.copy2(os.path.join(WORK, "c2.dll"), dll)


def compile_(c, tag, extra):
    """compile c with the proxy and the C2DAG_* settings of extra; (ok, the .asm path, the .dag path)"""
    asm, dag = os.path.join(WORK, tag + ".asm"), os.path.join(WORK, tag + ".dag")
    if os.path.exists(dag):
        os.remove(dag)
    e = c2trace.env()
    for k in [k for k in e if k.startswith("C2DAG_")]:
        del e[k]
    e.update(extra)
    r = subprocess.run([os.path.join(BIN, "CL.EXE"), "/nologo", "/c", "/O2", "/Gy", "/I" + os.path.join(ROOT, "include"),
                        "/I" + os.path.join(ROOT, "src"), "/I" + DX, "/Fa" + asm, "/Fo" + os.path.join(WORK, tag + ".obj"), c],
                       env=e, cwd=ROOT, capture_output=True, text=True)
    return r.returncode == 0, asm, dag


def functions(asm):
    """{decorated name: its instructions, one spelling for the generated labels}"""
    t = open(asm, encoding="latin-1").read()
    d = {}
    for m in re.finditer(r"^(\S+)\s+PROC.*?^\1\s+ENDP", t, re.S | re.M):
        d[m.group(1)] = [re.sub(r"\$L\d+", "$L", re.sub(r"\s+", " ", l.split(";")[0].strip()))
                         for l in m.group(0).split("\n") if l.startswith("\t")]
    return d


def listing(asm, func):
    return next((v for k, v in functions(asm).items() if re.sub(r"@\d+$", "", k.lstrip("_@")) == func), None)


def unit_text(a):
    """the traced text (the tree's or one variant's), written to WORK; its path"""
    args = list(a)
    text = open(args[0], encoding="latin-1", newline="").read().replace("\r\n", "\n")
    if "--variants" in args:
        i = args.index("--variants")
        lab = args[args.index("--label") + 1] if "--label" in args else None
        text, _ = c2trace.variant_text(text, args[1], args[i + 1], lab, src=args[0])
    c = os.path.join(WORK, "w.c")
    with open(c, "w", encoding="latin-1", newline="\n") as f:
        f.write(text)
    return c


def spec_env(spec, func):
    e = {"C2DAG_SCHED": func}
    for part in (spec.split("+") if spec != "-" else []):
        if part.startswith("h"):
            e["C2DAG_HACK"] = part[1:]
        elif part.startswith("l"):
            e["C2DAG_LAT"] = part[1:]
        elif part.startswith("g"):
            e["C2DAG_CPU"] = part[1:]
        elif part == "f":
            e["C2DAG_FPOFF"] = "1"
        else:
            e["C2DAG_CUT"] = part
    return e


def trace(a):
    func = a[1]
    c = unit_text(a)
    ok, asm, dag = compile_(c, "w", {"C2DAG_OUT": os.path.join(WORK, "w.dag"), "C2DAG_SCHED": func})
    if not ok:
        raise SystemExit("compile failed")
    on = False
    for l in open(dag, encoding="latin-1"):
        if l.startswith("block "):
            on = ("_" + func) in l.split()
        if l.startswith(("sel", " cons")) or on:
            sys.stdout.write(l)


def hack(a):
    func = a[1]
    show = int(a[a.index("--show") + 1]) if "--show" in a else 0
    want = a[a.index("--want") + 1] if "--want" in a else None
    opts = {"--show": 1, "--want": 1, "--variants": 1, "--label": 1}
    specs, i = [], 2
    while i < len(a):
        if a[i] in opts:
            i += 1 + opts[a[i]]
        else:
            specs.append(a[i]); i += 1
    c = unit_text(a)
    if want:
        T = [re.sub(r"\$L\d+", "$L", re.sub(r"\s+", " ", l.strip())) for l in open(want, encoding="latin-1") if l.strip()]
    else:
        ok, asm, _ = compile_(c, "base", {})
        T = listing(asm, func) if ok else None
        if T is None:
            raise SystemExit("the unpatched compile failed or has no %s" % func)
    for spec in specs:
        ok, asm, _ = compile_(c, "w", dict(spec_env(spec, func), C2DAG_OUT=os.path.join(WORK, "w.dag")))
        L = listing(asm, func) if ok else None
        if L is None:
            print(spec, "compile failed"); continue
        if L == T:
            print(spec, "the same listing" if not want else "MATCH"); continue
        d = [k for k in range(min(len(L), len(T))) if L[k] != T[k]]
        print(spec, "len %d/%d, first difference at %s, %d lines differ" % (len(L), len(T), d[0] if d else "end", len(d)))
        if show and d:
            for k in range(max(0, d[0] - 2), min(len(L), d[0] + show)):
                print("    %-45s | %s" % (L[k], T[k] if k < len(T) else ""))


def wall(a):
    spec = a[0]
    units = a[1:] or sorted(os.path.splitext(os.path.basename(f))[0] for f in glob.glob(os.path.join(ROOT, "src", "*.c")))
    grades = {}
    for l in open(os.path.join(ROOT, "ledger", "grades.tsv"), encoding="utf-8"):
        p = l.rstrip("\n").split("\t")
        if len(p) > 4:
            grades[p[2]] = p[4]
    nf, tot = 0, {}
    for u in units:
        c = os.path.join(ROOT, "src", u + ".c")
        ok1, asm1, _ = compile_(c, "a", {})
        ok2, asm2, _ = compile_(c, "b", {"C2DAG_W": spec})
        if not (ok1 and ok2):
            print(u, "compile failed", flush=True); continue
        x, y = functions(asm1), functions(asm2)
        nf += len(x)
        for n in x:
            if y.get(n) != x[n]:
                nm = re.sub(r"@\d+$", "", n.lstrip("_@"))
                g = grades.get(nm, "?")
                tot[g] = tot.get(g, 0) + 1
                print("%s\t%s\t%s\tlines %d" % (u, nm, g, sum(1 for p, q in zip(x[n], y.get(n, [])) if p != q)), flush=True)
    print("functions", nf, "changed by grade", tot, flush=True)


def main():
    a = sys.argv[1:]
    if len(a) < 2 or a[0] not in ("trace", "hack", "wall") or (a[0] != "wall" and len(a) < 3):
        raise SystemExit(__doc__)
    os.makedirs(WORK, exist_ok=True)
    setup()
    {"trace": trace, "hack": hack, "wall": wall}[a[0]](a[1:])


if __name__ == "__main__":
    main()
