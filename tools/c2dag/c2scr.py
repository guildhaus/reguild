"""The scratch-register picks of one function (C2 13.00.9044): which register each expression temporary got, in
the order VC6 decides them, and why (its preferred register, the rover, the list).

    python tools/c2dag/c2scr.py src/hst.c ai_CommandExecution                 # the tree's text
    python tools/c2dag/c2scr.py src/hst.c FUNC --variants FILE [--label L]    # one dcp try variant
    python tools/c2dag/c2scr.py probe.c probe --uses                          # every tuple with its operands too
    python tools/c2dag/c2scr.py src/hst.c FUNC --lines 1350-1362              # only these file lines
    python tools/c2dag/c2scr.py src/am.c "*" --uses                           # every function (labels carry the name)

The rule (FUN_1070a291 -> FUN_1070a404 / FUN_1070a4ab -> FUN_1070c32b; lowering, after the global allocator and
before the scheduler, so no scheduler hack can change it):
  * tuples are walked once in layout order; per tuple first the uses, then the defs. A register is freed at the
    LAST use of what it holds (uses first: a temp dying in the tuple frees its register for the tuple's def).
  * a temp that the allocator left without a register gets one at its defining tuple:
    1. its preferred register (`pref`), when it is free and not in conflict: the source register of the mov that
       defines it (`t = reg`), the destination register of the mov that uses it (`reg = t`: an allocated local,
       eax of a return value, ecx / edx of a __fastcall argument), or the base register of a lea without an
       index. THE ROVER DOES NOT MOVE.
    2. else the rover: the next of eax, ecx, edx (cyclic, starting at the register behind the last rover pick)
       that is free, a byte register when needed, and not in conflict with a register use inside the temp's
       life (an allocated local's register, ecx of a shift count, eax / edx of a division, a call's kills). The
       rover is left behind the register taken.
    3. else the first free of eax ecx edx esi edi ebx ebp.
  * the rover is per function (reset to eax at the function's start only); the content table is reset at every
    label (to the registers live into the block).
  So one step of the rotation = one temp picked by the rover. A temp picked by preference is a temp that makes no
  step; a value the allocator gave a register (a local, a CSE temp, a constant, a split piece) makes none either
  but is busy for the rover (it is skipped). --uses shows the `busy{}` set at each tuple.

Output: `P line L op X tID [pref R] busy{ reg=holder .. } rover R -> R (rover | pref | list)`; `L line` = a label
(content table reset). `E line L op X: R defined while it holds tN` = an eviction (FUN_107701e7): the tuple
defines a register that holds a live scratch temp; the temp's new pick follows and a `mov new,old` stays in the
code (`mov eax,esi; mov esi,1`). With --uses: `U line L op X U: sym@where! .. D: ..` per tuple (`!` = last use: the register
is freed), before its picks. Line numbers are those of the traced text (with --variants the copy under
build/c2dag/scr).
"""
import argparse, os, re, shutil, subprocess, sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import c2trace  # noqa: E402  (variant_text, function_span, env)

ROOT = c2trace.ROOT
VC = c2trace.VC
WORK = os.path.join(ROOT, "build", "c2dag", "scr")
BIN = os.path.join(WORK, "bin")


def setup():
    src = os.path.join(HERE, "c2scr.c")
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
                        "/Fo" + os.path.join(WORK, "c2scr.obj"), "/link", "/BASE:0x20000000"],
                       env=c2trace.env(), cwd=WORK, capture_output=True, text=True)
    if r.returncode:
        raise SystemExit("building the proxy failed:\n" + r.stdout)
    shutil.copy2(os.path.join(WORK, "c2.dll"), dll)


def picks(src, func, variants=None, label=None, uses=False, flags=("/O2", "/Gy")):
    """the trace lines of one function's lowering, and the path of the traced text"""
    setup()
    text = open(src, encoding="latin-1", newline="").read().replace("\r\n", "\n")
    tag = os.path.splitext(os.path.basename(src))[0]
    if variants:
        text, lab = c2trace.variant_text(text, func, variants, label, src=src)
        tag += "_" + re.sub(r"\W", "_", lab)
    c = os.path.join(WORK, tag + ".c")
    outf = os.path.join(WORK, tag + ".scr")
    with open(c, "w", encoding="latin-1", newline="\n") as f:
        f.write(text)
    if os.path.exists(outf):
        os.remove(outf)
    e = c2trace.env()
    e["C2SCR_OUT"] = outf
    if func != "*":      # "*": every function of the unit
        e["C2SCR_FUNC"] = func
    if uses:
        e["C2SCR_USES"] = "1"
    r = subprocess.run([os.path.join(BIN, "CL.EXE"), "/nologo", "/c"] + list(flags) +
                       ["/I" + os.path.join(ROOT, "include"), "/I" + os.path.join(ROOT, "src"), "/I" + c2trace.DX,
                        "/I" + os.path.dirname(os.path.abspath(src)),
                        "/FAs", "/Fa" + os.path.join(WORK, tag + ".asm"), "/Fo" + os.path.join(WORK, tag + ".obj"), c],
                       env=e, cwd=ROOT, capture_output=True, text=True)
    if r.returncode:
        raise SystemExit("compile failed:\n" + "\n".join(l for l in r.stdout.splitlines() if "error" in l))
    lines = [l.rstrip() for l in open(outf, encoding="latin-1")] if os.path.exists(outf) else []
    return lines, c


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("src")
    ap.add_argument("func")
    ap.add_argument("--variants")
    ap.add_argument("--label")
    ap.add_argument("--uses", action="store_true", help="every tuple with its operands (U lines)")
    ap.add_argument("--lines", metavar="A-B", help="only the rows of these file lines")
    ap.add_argument("--flags", default="/O2 /Gy")
    a = ap.parse_args()
    lines, c = picks(a.src, a.func, a.variants, a.label, a.uses, a.flags.split())
    lo, hi = (int(x) for x in a.lines.split("-")) if a.lines else (0, 1 << 30)
    for l in lines:
        m = re.match(r"\w line (-?\d+)", l)
        if m and not lo <= int(m.group(1)) <= hi:
            continue
        print(l)
    print("(%d rows; text %s)" % (len(lines), os.path.relpath(c, ROOT)))


if __name__ == "__main__":
    try:
        main()
    except OSError as e:
        if e.errno not in (22, 32):
            raise
