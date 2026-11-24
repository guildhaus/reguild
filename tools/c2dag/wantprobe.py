"""wantprobe.py: per variant, does a wanted instruction pattern appear in the function's listing?

Usage (from anywhere):
    python tools/c2dag/wantprobe.py src/UNIT.c FUNC VARIANTS.txt REGEX

VARIANTS.txt is a `dcp try` variants file (### label blocks). Each variant (and the tree text,
printed as "(tree)") is compiled through flip.listing (the same compile flip.py uses) and the
script prints PRESENT/absent for REGEX over the function's listing lines, plus the instruction
count. Example: python tools/c2dag/wantprobe.py src/hmt.c hmt_Load v.txt "mov\\s+esi, DWORD PTR"
"""
import sys, os, re

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(os.path.dirname(HERE))
sys.path.insert(0, HERE)


def main():
    if len(sys.argv) != 5:
        print(__doc__)
        sys.exit(2)
    src, fn, vf, rx = [os.path.abspath(a) if i in (0, 2) else a for i, a in enumerate(sys.argv[1:5])]
    os.chdir(ROOT)
    import flip, c2trace
    os.makedirs(flip.OUT, exist_ok=True)
    # latin-1 round-trips the UTF-8 bytes unchanged
    text = open(src, encoding="latin-1", newline="").read().replace("\r\n", "\n")
    labels = list(c2trace.parse_variants(open(vf, encoding="latin-1").read()))
    want = re.compile(rx)
    for lab in ["(tree)"] + labels:
        t = text if lab == "(tree)" else c2trace.variant_text(text, fn, vf, lab)[0]
        ins = flip.listing("probe_" + re.sub(r"\W", "_", lab), t, fn)
        if ins is None:
            print("%-24s does not compile" % lab)
        else:
            print("%-24s %s  n=%d" % (lab, "PRESENT" if want.search("\n".join(ins)) else "absent", len(ins)))


if __name__ == "__main__":
    main()
