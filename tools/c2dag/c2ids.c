/* c2.dll proxy for VC6 SP5+PP (C2 13.00.9044), used by c2ids.py: loads the real back end (c2real.dll, same
   directory), hooks the calls of the per-function pipeline (FUN_1076e010 and its phase drivers FUN_1071cd72,
   FUN_1073e0ee, FUN_10728f2e) and writes, before each phase, the tuples of the chosen source lines of one function
   whenever they changed: opcode@line and every operand with its symbol's id (+0x1c) and type (+4). With
   C2IDS_ACCT=1 it also logs every new 32-record symbol block (FUN_1071c6fb) and every optimizer temporary
   (FUN_10722a72). Environment: C2IDS_OUT file, C2IDS_FUNC name, C2IDS_LO / C2IDS_HI (C2's lines: 0 = the
   definition line).
   Build: cl /nologo /LD /O1 c2ids.c /Fec2.dll /link /BASE:0x20000000 */
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

typedef int (__stdcall *INVOKE)(void *, void *, void *);
typedef int (__stdcall *ABORT)(void *);
static HMODULE real;
static INVOKE realInvoke;
static ABORT realAbort;
static FILE *out;
static int lo, hi;
#define U32(p, o) (*(unsigned long *)((char *)(p) + (o)))
#define U16(p, o) (*(unsigned short *)((char *)(p) + (o)))
#define U8(p, o) (*(unsigned char *)((char *)(p) + (o)))
#define PTR(p, o) (*(void **)((char *)(p) + (o)))

/* call rel32 sites (rva) and their targets: the phases in pipeline order are found by the target */
static struct { unsigned long rva, tgt; } sites[] = {
    { 0x6e0e2, 0x1cd72 }, { 0x6e0f1, 0x3e0ee }, { 0x6e0f8, 0x28f2e }, { 0x6e10b, 0x2c9a2 }, { 0x6e112, 0x2e2f0 },
    { 0x6e119, 0xcdb9 }, { 0x1cd7e, 0x40f87 }, { 0x1cdd0, 0x1c03f }, { 0x1cde3, 0x38d07 }, { 0x1ce02, 0x3c254 },
    { 0x1ce2d, 0x3ca3a }, { 0x1ce42, 0x3436 }, { 0x1ce55, 0x3d289 }, { 0x1ce6a, 0x1d3dd }, { 0x3e0f9, 0x3e1a3 },
    { 0x3e109, 0x1ce7b }, { 0x3e11a, 0x1d3dd }, { 0x3e135, 0x27dc5 }, { 0x3e148, 0x28079 }, { 0x3e15b, 0x2812f },
    { 0x3e16e, 0x283f7 }, { 0x3e181, 0x28d53 }, { 0x3e194, 0x28eed }, { 0x28f76, 0x29001 }, { 0x28f89, 0x29243 },
};
static char prev[1 << 21], cur[1 << 21];
static int curN;
static void emit(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    if (curN < (int)sizeof(cur) - 600)
        curN += vsprintf(cur + curN, fmt, ap);
    va_end(ap);
}
static int on(void)
{
    char *c = *(char **)((char *)real + 0xad730), *nm = 0, *want = getenv("C2IDS_FUNC");
    size_t n;
    if (!out || !want || IsBadReadPtr(c, 0x1c))
        return 0;
    nm = *(char **)(c + 0x18);
    if (!nm || IsBadStringPtrA(nm, 128))
        return 0;
    if (*nm == '_' || *nm == '@')
        nm++;
    n = strlen(want);
    return !strncmp(nm, want, n) && (nm[n] == 0 || nm[n] == '@');
}
static int isname(const char *nm)
{
    int i;
    if (!nm || IsBadStringPtrA(nm, 64) || !(nm[0] == '_' || (nm[0] >= 'A' && nm[0] <= 'z')))
        return 0;
    for (i = 1; nm[i]; i++)
        if (nm[i] < '$' || nm[i] > 'z')
            return 0;
    return 1;
}
/* an operand: kind (+8; 1 / 2 a symbol through +0x18 / +0x14, 7 an immediate); the symbol's name, id (+0x1c),
   type byte (+4; 3 = a temporary) and `d` when it has a defining tuple (+0x14: its sort key is that tree's) */
static void opnd(void *o)
{
    int k = U8(o, 8);
    emit(" [k%x", k);
    if (k == 1 || k == 2 || k == 4) {
        void *sy = PTR(o, k == 1 ? 0x18 : 0x14), *s0;
        if (sy && !IsBadReadPtr(sy, 0x20)) {
            const char *nm = PTR(sy, 0x18);
            if (!isname(nm)) {
                s0 = PTR(sy, 0);
                nm = s0 && !IsBadReadPtr(s0, 0x20) ? (const char *)PTR(s0, 0x18) : 0;
            }
            emit(" %s id=%lx t=%x%s", isname(nm) ? nm : "#", U32(sy, 0x1c), U8(sy, 4), PTR(sy, 0x14) ? " d" : "");
        }
    }
    emit("]");
}
/* C2IDS_KEYS: the sort key of an operand as C2 computes it (FUN_1071f734: height << 24 | nodes << 16 | hash) and
   what the hash is made of (FUN_1071f7f0, called in the real back end): a temporary's defining tuple with its
   operands, a memory operand's displacement (+0x2c), word +4 and base operand (+0x30) */
typedef unsigned __int64 (__fastcall *HASHFN)(void *);
static int keys;
static void keyof(void *o, int depth)
{
    int k = U8(o, 8), cls = U8((char *)real + 0x26f57, k);
    void *sy, *def, *u;
    int h;
    emit("{k%x h=%04lx", k, (unsigned long)((HASHFN)((char *)real + 0x1f7f0))(o) & 0xffff);
    if (cls == 2) {
        emit(" mem disp=%lx w4=%x", U32(o, 0x2c), U16(o, 4));
        if (PTR(o, 0x30) && !IsBadReadPtr(PTR(o, 0x30), 0x34) && depth < 6) {
            emit(" base=");
            keyof(PTR(o, 0x30), depth + 1);
        }
    } else if (cls == 0 && (k == 1 || k == 2)) {
        sy = PTR(o, 0x14);
        if (sy && !IsBadReadPtr(sy, 0x20)) {
            const char *nm = PTR(sy, 0x18);
            def = PTR(sy, 0x14);
            emit(" %s id=%lx t=%x", isname(nm) ? nm : "#", U32(sy, 0x1c), U8(sy, 4));
            if (def && !IsBadReadPtr(def, 0x24) && depth < 6) {
                emit(" def op %lx(", U32(def, 4));
                for (h = 0, u = PTR(def, 0x1c); u && !IsBadReadPtr(u, 0x34) && h < 6; u = PTR(u, 0), h++) {
                    emit(h ? ", key=%08lx " : "key=%08lx ", U32(u, 0xc));
                    keyof(u, depth + 1);
                }
                emit(")");
            }
        }
    }
    emit("}");
}
static void dump(int id)
{
    void *F = *(void **)((char *)real + 0xc44bc), *t, *o, *p8, *b0;
    int g = 0, k, h, n = 0;
    curN = 0;
    cur[0] = 0;
    if (!F || IsBadReadPtr(F, 0x40))
        return;
    p8 = PTR(F, 8);
    if (!p8 || IsBadReadPtr(p8, 8))
        return;
    b0 = PTR(p8, 0);
    if (!b0 || IsBadReadPtr(b0, 0x24))
        return;
    for (t = PTR(b0, 0x1c); t && !IsBadReadPtr(t, 0x24) && g < 200000; t = PTR(t, 0), g++) {
        int ln = *(short *)((char *)t + 0x10);
        if (ln)
            n = ln;
        if (n < lo || n > hi || !(U8(t, 9) & 1))
            continue;
        emit("  op %lx@%d", U32(t, 4), ln);
        for (k = 0; k < 2; k++) {
            emit(k ? " D:" : " U:");
            for (h = 0, o = PTR(t, k ? 0x20 : 0x1c); o && !IsBadReadPtr(o, 0x30) && h < 12; o = PTR(o, 0), h++)
                opnd(o);
        }
        emit("\n");
        if (keys && U32(t, 4) >= 0x26e && U32(t, 4) <= 0x280)
            for (h = 0, o = PTR(t, 0x1c); o && !IsBadReadPtr(o, 0x34) && h < 12; o = PTR(o, 0), h++) {
                emit("    K key=%08lx ", U32(o, 0xc));
                keyof(o, 0);
                emit("\n");
            }
    }
    if (strcmp(cur, prev)) {
        fprintf(out, "== before %lx\n%s", sites[id].tgt, cur);
        fflush(out);
        strcpy(prev, cur);
    }
}
static void __cdecl hook(int id) { if (on()) dump(id); }
static int lastKind;
static void __cdecl hKind(int kind) { lastKind = kind & 0xff; }
static void __cdecl hBlock(int unused)
{
    if (on())
        fprintf(out, "BLOCK firstid %lx class %d\n", U32((char *)real + 0xb16b8, 0), U8((char *)real + 0x4615, lastKind));
}
static void __cdecl hTemp(int ecx)
{
    unsigned long *sp = (unsigned long *)&ecx, ret2 = 0;
    int i;
    if (!on())
        return;
    for (i = 11; i < 40; i++) {     /* the caller behind the wrapper FUN_107229dc: value numbering or a later pass */
        unsigned long v = sp[i] - (unsigned long)real;
        if (v > 0x1000 && v < 0xb0000) { ret2 = v; break; }
    }
    fprintf(out, "TEMP %s\n", ret2 == 0x26b6c || ret2 == 0x25ffb ? "vn" : "later");
}
/* C2IDS_VN: every new entry of the expression table (hash.c, FUN_107229dc; hooked behind its call of
   FUN_10722a72: eax = the new temporary, ebx = the opcode) with the operands being numbered (the array
   0x107ad9a0 FUN_10722757 fills): the order in which the optimizer's temporaries get their ids */
static void __cdecl hVn(int ecx)
{
    unsigned long *sp = (unsigned long *)&ecx;
    void *sy = (void *)sp[9], **ops = (void **)((char *)real + 0xad9a0);
    int i;
    if (!on() || IsBadReadPtr(sy, 0x20))
        return;
    fprintf(out, "VN id=%lx op=%lx :", U32(sy, 0x1c), sp[6]);
    for (i = 0; i < 4 && ops[i] && !IsBadReadPtr(ops[i], 0x20); i++) {
        void *o = ops[i], *q;
        int k = U8(o, 8);
        if (k == 7)
            fprintf(out, " #%lx", U32(o, 0x18));
        else if (k == 5 || k == 6) {
            q = PTR(o, 0x1c);
            fprintf(out, " k%x:%lx", k, q && !IsBadReadPtr(q, 0x20) ? U32(q, 0x1c) : 0);
        } else {
            q = PTR(o, 0x14);
            if (q && !IsBadReadPtr(q, 0x2c)) {
                const char *nm = PTR(q, 0x18);
                fprintf(out, " k%x:%s%lx", k, k != 4 && isname(nm) ? nm : "", U32(q, k == 4 ? 0x28 : 0x1c));
            } else
                fprintf(out, " k%x", k);
        }
    }
    fprintf(out, "\n");
}
static void stubhook(unsigned char *fn, int len, void *h, int arg, int isCall)
{
    unsigned char *p, *st;
    DWORD old;
    st = p = (unsigned char *)VirtualAlloc(0, 64, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
    *p++ = 0x60; *p++ = 0x9c;                           /* pushad; pushfd */
    if (isCall) { *p++ = 0x68; *(long *)p = arg; p += 4; } else *p++ = 0x51;   /* push imm32 / push ecx */
    *p++ = 0xe8; *(long *)p = (long)h - (long)(p + 4); p += 4;
    *p++ = 0x83; *p++ = 0xc4; *p++ = 0x04;
    *p++ = 0x9d; *p++ = 0x61;
    if (isCall) { *p++ = 0xe8; *(long *)p = (long)(fn + 5 + *(long *)(fn + 1)) - (long)(p + 4); p += 4; }
    else { memcpy(p, fn, len); p += len; }
    *p++ = 0xe9; *(long *)p = (long)(fn + len) - (long)(p + 4);
    VirtualProtect(fn, len, PAGE_EXECUTE_READWRITE, &old);
    fn[0] = 0xe9; *(long *)(fn + 1) = (long)st - (long)(fn + 5);
    VirtualProtect(fn, len, old, &old);
    FlushInstructionCache(GetCurrentProcess(), fn, len);
}
static void install(void)
{
    static const unsigned char e1[] = { 0x33, 0xc0, 0x8a, 0xc1, 0x0f, 0xb6, 0x88 };   /* FUN_10701701 */
    static const unsigned char e2[] = { 0x51, 0x53, 0x56, 0xba, 0x0c, 0x0b, 0x00, 0x00 };   /* FUN_1071c6fb */
    static const unsigned char e3[] = { 0x56, 0x57, 0x8b, 0xf9, 0xb1, 0x0f };   /* FUN_10722a72 */
    char path[MAX_PATH], *s;
    const char *o;
    unsigned char *b;
    int i;
    GetModuleFileNameA(GetModuleHandleA("c2.dll"), path, MAX_PATH);
    s = strrchr(path, '\\');
    strcpy(s ? s + 1 : path, "c2real.dll");
    real = LoadLibraryA(path);
    if (!real)
        ExitProcess(3);
    realInvoke = (INVOKE)GetProcAddress(real, "_InvokeCompilerPass@12");
    realAbort = (ABORT)GetProcAddress(real, "_AbortCompilerPass@4");
    o = getenv("C2IDS_OUT");
    if (o && *o)
        out = fopen(o, "a");
    lo = getenv("C2IDS_LO") ? atoi(getenv("C2IDS_LO")) : 0;
    hi = getenv("C2IDS_HI") ? atoi(getenv("C2IDS_HI")) : 0;
    b = (unsigned char *)real;
    if (memcmp(b + 0x1701, e1, sizeof(e1)) || memcmp(b + 0x1c6fb, e2, sizeof(e2)) || memcmp(b + 0x22a72, e3, sizeof(e3)))
        return;     /* not the build this was written for: run unhooked */
    keys = getenv("C2IDS_KEYS") != 0;
    if (getenv("C2IDS_VN") && b[0x22a0c] == 0x8b && b[0x22a0e] == 0x66)
        stubhook(b + 0x22a0c, 7, hVn, 0, 0);
    if (getenv("C2IDS_ACCT")) {
        stubhook(b + 0x1701, 11, hKind, 0, 0);
        stubhook(b + 0x1c6fb, 8, hBlock, 0, 0);
        stubhook(b + 0x22a72, 6, hTemp, 0, 0);
    }
    for (i = 0; i < (int)(sizeof(sites) / sizeof(sites[0])); i++)
        if (b[sites[i].rva] == 0xe8)
            stubhook(b + sites[i].rva, 5, hook, i, 1);
}
__declspec(dllexport) int __stdcall InvokeCompilerPass(void *a, void *b, void *c)
{
    if (!real)
        install();
    return realInvoke(a, b, c);
}
__declspec(dllexport) int __stdcall AbortCompilerPass(void *a)
{
    if (!real)
        install();
    return realAbort(a);
}
