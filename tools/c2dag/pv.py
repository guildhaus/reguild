"""Probe variants: compile each variant of a small stand-alone probe file with VC6 /O2 and print one stretch of
its listing per row, to try many source forms of one hunk in seconds (no project headers, no dcp daemon).

    python tools/c2dag/pv.py tools/c2dag/probes/band.c VARIANTS.txt --from _defscene --skip 2 --until "fxch|fadd"

VARIANTS.txt: `### label` lines, each followed by `<<<` old `===` new `>>>` blocks against the probe's text (each
old text once). Per row: the instructions from the first line matching --from (+ --skip) up to the first line
matching --until (at most --max), `COPY` in front when the stretch holds a register-to-register mov.
A probe is the hunk's function cut down until the listing still shows the tree's instructions at the hunk
(probes/band.c: sv_UpdateBand's info test and vertex loop). What a probe finds must still be confirmed with
`dcp try` on the real function.
"""
import argparse, os, re, subprocess, tempfile

VC = os.environ.get("C2DAG_VC", r"C:\Tools\MSVC600-SP5PP")


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("probe"); ap.add_argument("variants")
    ap.add_argument("--from", dest="frm", default=r"PROC", help="regex of the first listing line (default: the top)")
    ap.add_argument("--skip", type=int, default=0, help="lines to skip behind the --from line")
    ap.add_argument("--until", default=None, help="regex of the line that ends the stretch")
    ap.add_argument("--max", type=int, default=14)
    a = ap.parse_args()
    env = dict(os.environ)
    env["INCLUDE"] = VC + r"\VC98\Include"
    env["PATH"] = VC + r"\VC98\Bin;" + VC + r"\Common\MSDev98\Bin;" + env["PATH"]
    base = open(a.probe).read()
    wd = tempfile.mkdtemp(prefix="pv_")
    for part in re.split(r"^### ", open(a.variants).read(), flags=re.M)[1:]:
        lab, _, body = part.partition("\n")
        src, bad = base, None
        for old, new in re.findall(r"<<<\n(.*?)===\n(.*?)>>>", body, re.S):
            if src.count(old) != 1:
                bad = "old text occurs %d times: %r" % (src.count(old), old[:40])
            src = src.replace(old, new)
        if bad:
            print("%-10s %s" % (lab.strip(), bad))
            continue
        with open(os.path.join(wd, "p.c"), "w") as f:
            f.write(src)
        r = subprocess.run([VC + r"\VC98\Bin\cl.exe", "/nologo", "/c", "/O2", "/Gy", "/Fap.asm", "/Fop.obj", "p.c"],
                           cwd=wd, env=env, capture_output=True, text=True)
        if r.returncode:
            print("%-10s does not compile: %s" % (lab.strip(), " ".join(r.stdout.split())[-200:]))
            continue
        L = [re.sub(r"\s+", " ", l.split(";")[0]).strip() for l in open(os.path.join(wd, "p.asm"))
             if re.match(r"\t[a-z]|\$L\d+:|\w+\s+PROC", l)]
        i0 = next((i for i, l in enumerate(L) if re.search(a.frm, l)), 0) + a.skip
        seg = L[i0:i0 + a.max]
        if a.until:
            seg = seg[:next((i for i, l in enumerate(seg) if re.search(a.until, l)), len(seg))]
        copy = any(re.match(r"mov e\w\w, e\w\w$", l) for l in seg)
        print("%-10s %s%s" % (lab.strip(), "COPY " if copy else "", " | ".join(seg).replace("DWORD PTR ", "")))


if __name__ == "__main__":
    main()
