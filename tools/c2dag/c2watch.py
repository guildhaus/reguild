"""Which code of c2.dll reads / writes a range of its data while one function is compiled (C2 13.00.9044).

    python tools/c2dag/c2watch.py FILE.c [FUNC] [--lo RVA --hi RVA] [--seq] [--flags "/O2 /Gy"]

Default range: the register table entries of eax, ecx, edx (RVA 0xc4a98 .. 0xc4ba0, 0x58 bytes each). Prints one
line per distinct (instruction RVA, data RVA, r/w) with its count and the return addresses found on the stack;
--seq prints every access in order with the dword's value before it. The proxy is tools/c2dag/c2watch.c (a
PAGE_NOACCESS watch with a vectored handler), built into build/c2dag/watch/bin next to a copy of the compiler.
FILE.c must compile on its own (a probe file, or a preprocessed unit).
"""
import argparse, os, re, shutil, subprocess, sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(os.path.dirname(HERE))
VC = os.environ.get("C2DAG_VC", r"C:\Tools\MSVC600-SP5PP")
WORK = os.path.join(ROOT, "build", "c2dag", "watch")
BIN = os.path.join(WORK, "bin")


def env():
    e = dict(os.environ)
    e["INCLUDE"] = VC + r"\VC98\Include"
    e["LIB"] = VC + r"\VC98\Lib"
    e["PATH"] = VC + r"\VC98\Bin;" + VC + r"\Common\MSDev98\Bin;" + e["PATH"]
    return e


def setup():
    src = os.path.join(HERE, "c2watch.c")
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
                        "/Fo" + os.path.join(WORK, "c2watch.obj"), "/link", "/BASE:0x20000000"],
                       env=env(), cwd=WORK, capture_output=True, text=True)
    if r.returncode:
        raise SystemExit("building the watch proxy failed:\n" + r.stdout)
    shutil.copy2(os.path.join(WORK, "c2.dll"), dll)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("src")
    ap.add_argument("func", nargs="?")
    ap.add_argument("--lo")
    ap.add_argument("--hi")
    ap.add_argument("--seq", action="store_true")
    ap.add_argument("--flags", default="/O2 /Gy")
    a = ap.parse_args()
    setup()
    e = env()
    e["PATH"] = BIN + ";" + e["PATH"]
    outf = os.path.join(WORK, "watch.out")
    if os.path.exists(outf):
        os.remove(outf)
    e["C2WATCH_OUT"] = outf
    if a.func:
        e["C2WATCH_FUNC"] = a.func
    if a.lo:
        e["C2WATCH_LO"] = a.lo
    if a.hi:
        e["C2WATCH_HI"] = a.hi
    if a.seq:
        e["C2WATCH_SEQ"] = "1"
    r = subprocess.run([os.path.join(BIN, "cl.exe"), "/nologo", "/c"] + a.flags.split() +
                       ["/Fo" + os.path.join(WORK, "w.obj"), "/FAs", "/Fa" + os.path.join(WORK, "w.asm"),
                        os.path.abspath(a.src)], env=e, cwd=WORK, capture_output=True, text=True)
    if r.returncode:
        sys.stderr.write("cl exit %d\n" % r.returncode + r.stdout + r.stderr)
    if os.path.exists(outf):
        sys.stdout.write(open(outf).read())


if __name__ == "__main__":
    main()
