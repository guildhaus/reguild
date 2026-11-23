import sys, struct
a = open(sys.argv[1],'rb').read(); b = open(sys.argv[2],'rb').read()
def secs(d):
    pe = struct.unpack_from('<I', d, 0x3c)[0]
    n = struct.unpack_from('<H', d, pe+6)[0]
    opt = struct.unpack_from('<H', d, pe+20)[0]
    base = struct.unpack_from('<I', d, pe+24+28)[0]
    out=[]
    for i in range(n):
        o = pe+24+opt+40*i
        name = d[o:o+8].rstrip(b'\0').decode()
        vs, va, rs, ro = struct.unpack_from('<IIII', d, o+8)
        out.append((name, va+base, rs, ro))
    return pe, out
pe, sa = secs(a)
print('sizes', len(a), len(b))
diffs=[]
i=0; n=min(len(a),len(b))
while i<n:
    if a[i]!=b[i]:
        j=i
        while j<n and a[j]!=b[j]: j+=1
        diffs.append((i,j)); i=j
    else: i+=1
for s,e in diffs:
    where='header'
    for name,va,rs,ro in sa:
        if ro<=s<ro+rs: where='%s va=%#x'%(name, va+s-ro)
    if s==pe+8: where='PE timestamp'
    print('%#x..%#x %s' % (s,e,where))
print('ranges', len(diffs))
