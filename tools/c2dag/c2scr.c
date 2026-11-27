/* c2.dll proxy for VC6 SP5+PP (C2 13.00.9044): the scratch-register picks of the lowering pass (FUN_1070a291,
   after the global allocator, before the scheduler). Every expression temporary that the allocator left without a
   register gets one at its defining tuple, in tuple (layout) order, by FUN_1070c32b:
     1. its preferred register (temp +0x2c: the source register of a mov-like tuple 1 / 11f / 1dd / 17f / 181
        that defines it, the destination register of the mov-like tuple that uses it, or the base register of a
        lea 0x12 without index), when that register is free, allowed and not in the temp's conflict set: the
        rover does NOT move;
     2. else from the rover (0x107b1418, one per class, set to the list start per function, never reset at a
        block): the next of eax, ecx, edx in cyclic order that is free (content table 0x107b12e8[reg] == 0), a
        byte register when the temp needs one, and not in conflict (bit temp id of 0x107b11c0[reg]); the rover is
        left BEHIND the register taken;
     3. else the first free register of the list eax ecx edx esi edi ebx ebp; else a spill.
   The content table is cleared at every label tuple (kind 0x19: set to the block's live-in registers) and a
   register is freed at the last use (operand flag +0x11 & 0x10) of what it holds, uses before defs.
   Output (file C2SCR_OUT, function C2SCR_FUNC): one line per pick:
     P line L op X temp#ID [pref R] busy{..} rover R -> R (pref | rover | list)
   and `L line` at each label tuple, `E line` when a tuple defines a register that holds a live scratch temp
   (FUN_107701e7: the temp is evicted, its new pick follows as a P line), `U` lines (C2SCR_USES=1) for the uses that free a register.
   Build: cl /nologo /LD /O1 c2scr.c /Fec2.dll /link /BASE:0x20000000 (tools/c2dag/c2scr.py). */
#include <windows.h>
#include <stdio.h>
#include <string.h>

typedef int (__stdcall *INVOKE)(void *, void *, void *);
typedef int (__stdcall *ABORT)(void *);
static HMODULE real;
static INVOKE realInvoke;
static ABORT realAbort;
static FILE *out;

#define U32(p, o) (*(unsigned long *)((char *)(p) + (o)))
#define U16(p, o) (*(unsigned short *)((char *)(p) + (o)))
#define U8(p, o) (*(unsigned char *)((char *)(p) + (o)))
#define PTR(p, o) (*(void **)((char *)(p) + (o)))

static const char *rn(unsigned long n)
{
    static const char *regs[] = { "r0", "eax", "ecx", "edx", "ebx", "esp", "ebp", "esi", "edi" };
    static char b[4][12]; static int k;
    if (n < 9) return regs[n];
    k = (k + 1) & 3; sprintf(b[k], "r%lu", n); return b[k];
}
static int on(void)
{
    char *cur = *(char **)((char *)real + 0xad730), *nm = 0, *want = getenv("C2SCR_FUNC");
    size_t n;
    if (!out) return 0;
    if (!want) return 1;
    if (IsBadReadPtr(cur, 0x1c)) return 0;
    nm = *(char **)(cur + 0x18);
    if (!nm || IsBadStringPtrA(nm, 128)) return 0;
    if (*nm == 0x5f || *nm == 0x40) nm++;
    n = strlen(want);
    return !strncmp(nm, want, n) && (nm[n] == 0 || nm[n] == 0x40);
}
static unsigned long entryReg(void *e) { return ((unsigned long)e - ((unsigned long)real + 0xc4a40)) / 0x58; }
static unsigned long roverReg(void)
{
    unsigned long *p = *(unsigned long **)((char *)real + 0xb1418);
    return IsBadReadPtr(p, 4) ? 99 : *p;
}
static void symname(void *s)
{
    char *nm;
    if (!s || IsBadReadPtr(s, 0x30)) { fprintf(out, "?"); return; }
    if (U8(s, 4) == 1) { fprintf(out, "%s", rn(entryReg(PTR(s, 8)))); return; }
    nm = PTR(s, 0x18);
    if (U8(s, 4) != 3 && nm && !IsBadStringPtrA(nm, 64) && (nm[0] == 0x5f || (nm[0] >= 0x41 && nm[0] <= 0x7a)))
        fprintf(out, "%.40s", nm);
    else
        fprintf(out, "t%lx", U32(s, 0x1c));
}
static void busy(void)
{
    void **tab = (void **)((char *)real + 0xb12e8); int i;
    fprintf(out, " busy{");
    for (i = 1; i <= 8; i++) {
        void *c = tab[i];
        if (!c || i == 5) continue;
        fprintf(out, " %s=", rn(i));
        if ((unsigned long)c - ((unsigned long)real + 0xc4a40) < 0x58 * 80) fprintf(out, "reg");
        else symname(c);
    }
    fprintf(out, " }");
}
static void *curOp, *curTup; static unsigned long roverIn;
/* FUN_1070c32b(ecx = the defining operand, edx = its tuple) */
static void __cdecl hPick(unsigned long *r)
{
    curOp = (void *)r[7]; curTup = (void *)r[6]; roverIn = roverReg();
}
/* FUN_1070c669(ecx = the register number, edx = the temp): the register is given */
static void __cdecl hGive(unsigned long *r)
{
    unsigned long reg = r[7]; void *temp = (void *)r[6], *pref;
    if (!on()) return;
    if (!curTup || IsBadReadPtr(curTup, 0x24) || IsBadReadPtr(temp, 0x30)) return;
    pref = PTR(temp, 0x2c);
    fprintf(out, "P line %d op %lx t%lx", (int)*(short *)((char *)curTup + 0x10), U32(curTup, 4), U32(temp, 0x1c));
    if (pref) fprintf(out, " pref %s", rn(entryReg(pref)));
    if (U8(temp, 5) & 0x80) fprintf(out, " byte");
    busy();
    fprintf(out, " rover %s -> %s (%s)\n", rn(roverIn), rn(reg),
            roverReg() != roverIn ? "rover" : pref && entryReg(pref) == reg ? "pref" : reg == roverIn ? "rover=" : "list");
    fflush(out);
    curTup = 0;
}
/* FUN_1070d51a(ecx = a label tuple): the content table is reset */
static void __cdecl hLabel(unsigned long *r)
{
    void *t = (void *)r[7];
    if (!on() || IsBadReadPtr(t, 0x18)) return;
    fprintf(out, "L line %d%s", (int)*(short *)((char *)t + 0x10), PTR(t, 0x14) ? "" : " (no block)");
    if (!getenv("C2SCR_FUNC")) {   /* every function traced: name it at its labels */
        char *cur = *(char **)((char *)real + 0xad730), *nm = IsBadReadPtr(cur, 0x1c) ? 0 : *(char **)(cur + 0x18);
        if (nm && !IsBadStringPtrA(nm, 128)) fprintf(out, " %s", nm);
    }
    fputc(10, out);
}
/* FUN_1070a404(ecx = a tuple): its uses are resolved; print the tuple and what its last uses free */
static void __cdecl hUses(unsigned long *r)
{
    void *t = (void *)r[7], *o; int g = 0, k;
    if (!on() || IsBadReadPtr(t, 0x24)) return;
    fprintf(out, "U line %d op %lx", (int)*(short *)((char *)t + 0x10), U32(t, 4));
    for (k = 0; k < 2; k++) {
        fprintf(out, k ? " D:" : " U:");
        for (g = 0, o = PTR(t, k ? 0x20 : 0x1c); o && !IsBadReadPtr(o, 0x1c) && g < 12; o = PTR(o, 0), g++) {
            if (U8(o, 8) != 1) { fprintf(out, " k%x", U8(o, 8)); continue; }
            fprintf(out, " ");
            symname(PTR(o, 0x14));
            fprintf(out, "@");
            symname(PTR(o, 0x18));
            if (U8(o, 0x11) & 0x10) fprintf(out, "!");
        }
    }
    busy();
    fprintf(out, " rover %s\n", rn(roverReg()));
}

/* FUN_107701e7(ecx = the tuple, edx = a register it defines): the scratch temp in that register is evicted
   (moved to a new pick: a kept `mov new,old`, or stored and reloaded) */
static void __cdecl hEvict(unsigned long *r)
{
    void *t = (void *)r[7]; void **tab = (void **)((char *)real + 0xb12e8);
    if (!on() || IsBadReadPtr(t, 0x24) || r[6] > 8) return;
    fprintf(out, "E line %d op %lx: %s defined while it holds ", (int)*(short *)((char *)t + 0x10), U32(t, 4), rn(r[6]));
    symname(tab[r[6]]);
    fputc(10, out);
}

static void midhook(unsigned long rva, int len, void (__cdecl *h)(unsigned long *), const unsigned char *expect, int elen)
{
    unsigned char *fn = (unsigned char *)real + rva, *p, *s;
    DWORD old;
    if (memcmp(fn, expect, elen))
        return;
    s = p = (unsigned char *)VirtualAlloc(0, 64, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
    *p++ = 0x60; *p++ = 0x9c; *p++ = 0x54;            /* pushad; pushfd; push esp */
    *p++ = 0xe8; *(long *)p = (long)h - (long)(p + 4); p += 4;
    *p++ = 0x83; *p++ = 0xc4; *p++ = 0x04;            /* add esp,4 */
    *p++ = 0x9d; *p++ = 0x61;                          /* popfd; popad */
    memcpy(p, fn, len); p += len;
    *p++ = 0xe9; *(long *)p = (long)(fn + len) - (long)(p + 4);
    VirtualProtect(fn, len, PAGE_EXECUTE_READWRITE, &old);
    fn[0] = 0xe9; *(long *)(fn + 1) = (long)s - (long)(fn + 5);
    memset(fn + 5, 0x90, len - 5);
    VirtualProtect(fn, len, old, &old);
    FlushInstructionCache(GetCurrentProcess(), fn, len);
}
static void install(void)
{
    char path[MAX_PATH], *s; const char *o;
    static const unsigned char e1[] = { 0x83, 0xec, 0x14, 0x53, 0x55, 0x56, 0x57 };   /* 1070c32b */
    static const unsigned char e2[] = { 0x56, 0x8b, 0xf1, 0x89, 0x14, 0xb5 };         /* 1070c669 */
    static const unsigned char e3[] = { 0x83, 0xec, 0x08, 0x56, 0x8b, 0x71, 0x14 };   /* 1070d51a */
    static const unsigned char e4[] = { 0x55, 0x56, 0x8b, 0x71, 0x1c };               /* 1070a404 */
    static const unsigned char e5[] = { 0x83, 0xec, 0x0c, 0x53, 0x56, 0x8b, 0xf2 };   /* 107701e7 */
    GetModuleFileNameA(GetModuleHandleA("c2.dll"), path, MAX_PATH);
    s = strrchr(path, 0x5c);
    strcpy(s ? s + 1 : path, "c2real.dll");
    real = LoadLibraryA(path);
    if (!real) ExitProcess(3);
    realInvoke = (INVOKE)GetProcAddress(real, "_InvokeCompilerPass@12");
    realAbort = (ABORT)GetProcAddress(real, "_AbortCompilerPass@4");
    o = getenv("C2SCR_OUT");
    if (o && *o) out = fopen(o, "a");
    midhook(0xc32b, 7, hPick, e1, 7);
    midhook(0xc669, 10, hGive, e2, 6);
    midhook(0xd51a, 7, hLabel, e3, 7);
    midhook(0x701e7, 7, hEvict, e5, 7);
    if (getenv("C2SCR_USES")) midhook(0xa404, 5, hUses, e4, 5);
}
__declspec(dllexport) int __stdcall InvokeCompilerPass(void *a, void *b, void *c)
{
    if (!real) install();
    return realInvoke(a, b, c);
}
__declspec(dllexport) int __stdcall AbortCompilerPass(void *a)
{
    if (!real) install();
    return realAbort(a);
}
