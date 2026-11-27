/* c2.dll proxy for VC6 SP5+PP (C2 13.00.9044): a memory-access watch on a range of the back end's data, to find
   the code that reads / writes a table (the register table 0xc49e8 + 0x58 * n by default: eax 2, ecx 3, edx 4).
   The pages of the range are made PAGE_NOACCESS; a vectored handler records each faulting instruction (its RVA,
   the RVA accessed, read / write, the return addresses on the stack that point into c2's text) while the function
   named by C2WATCH_FUNC (any when unset) is compiled, single-steps it and protects the page again.
     C2WATCH_OUT   the report file (written at exit: one line per distinct (eip, offset, rw), with count)
     C2WATCH_LO / C2WATCH_HI   the watched RVA range (hex; default c4a98 .. c4ba0 = the eax, ecx, edx entries)
     C2WATCH_SEQ=1 also prints every access in order (eip offset rw value-before)
   Build as c2trace.c: cl /nologo /LD /O1 c2watch.c /Fec2.dll /link /BASE:0x20000000 (tools/c2dag/c2watch.py). */
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef int (__stdcall *INVOKE)(void *, void *, void *);
typedef int (__stdcall *ABORT)(void *);
static HMODULE real;
static INVOKE realInvoke;
static ABORT realAbort;
static FILE *out;
static unsigned long lo = 0xc4a98, hi = 0xc4ba0, plo, phi;
static int seq, armed;
static struct { unsigned long eip, off; int rw; long n; unsigned long ra[6]; } tab[8192];
static int ntab;

static int fnOn(void)
{
    char *cur = *(char **)((char *)real + 0xad730), *nm = 0, *want = getenv("C2WATCH_FUNC");
    size_t n;
    if (!want) return 1;
    if (IsBadReadPtr(cur, 0x1c)) return 0;
    nm = *(char **)(cur + 0x18);
    if (!nm || IsBadStringPtrA(nm, 128)) return 0;
    if (*nm == 0x5f || *nm == 0x40) nm++;
    n = strlen(want);
    return !strncmp(nm, want, n) && (nm[n] == 0 || nm[n] == 0x40);
}
static void protect(int on)
{
    DWORD old;
    VirtualProtect((char *)real + plo, phi - plo, on ? PAGE_NOACCESS : PAGE_READWRITE, &old);
}
static LONG CALLBACK veh(EXCEPTION_POINTERS *x)
{
    EXCEPTION_RECORD *r = x->ExceptionRecord;
    CONTEXT *c = x->ContextRecord;
    if (r->ExceptionCode == EXCEPTION_ACCESS_VIOLATION) {
        unsigned long a = (unsigned long)r->ExceptionInformation[1] - (unsigned long)real;
        if (a < plo || a >= phi) return EXCEPTION_CONTINUE_SEARCH;
        protect(0);
        if (a >= lo && a < hi && fnOn()) {
            unsigned long eip = c->Eip - (unsigned long)real; int rw = (int)r->ExceptionInformation[0], i, k = 0;
            unsigned long *sp = (unsigned long *)c->Esp, ra[6];
            memset(ra, 0, sizeof ra);
            for (i = 0; i < 400 && k < 6 && !IsBadReadPtr(sp + i, 4); i++) {
                unsigned long v = sp[i] - (unsigned long)real;
                if (v >= 0x1000 && v < 0xabd40 && ((unsigned char *)sp[i])[-5] == 0xe8) ra[k++] = v;
            }
            for (i = 0; i < ntab; i++) if (tab[i].eip == eip && tab[i].off == a && tab[i].rw == rw) break;
            if (i == ntab && ntab < 8192) { tab[i].eip = eip; tab[i].off = a; tab[i].rw = rw; tab[i].n = 0; memcpy(tab[i].ra, ra, sizeof ra); ntab++; }
            if (i < 8192) tab[i].n++;
            if (seq && out)
                fprintf(out, "S %lx %lx %c %lx  <- %lx %lx %lx\n", eip, a, rw ? 0x77 : 0x72,
                        *(unsigned long *)((char *)real + (a & ~3UL)), ra[0], ra[1], ra[2]);
        }
        c->EFlags |= 0x100;
        armed = 1;
        return EXCEPTION_CONTINUE_EXECUTION;
    }
    if (r->ExceptionCode == EXCEPTION_SINGLE_STEP && armed) {
        armed = 0;
        protect(1);
        return EXCEPTION_CONTINUE_EXECUTION;
    }
    return EXCEPTION_CONTINUE_SEARCH;
}
static void report(void)
{
    int i, k;
    if (!out) return;
    protect(0);
    for (i = 0; i < ntab; i++) {
        fprintf(out, "W %lx %lx %c %ld  <-", tab[i].eip, tab[i].off, tab[i].rw ? 0x77 : 0x72, tab[i].n);
        for (k = 0; k < 6 && tab[i].ra[k]; k++) fprintf(out, " %lx", tab[i].ra[k]);
        fputc(10, out);
    }
    fclose(out); out = 0;
}
static void install(void)
{
    char path[MAX_PATH], *s; const char *o;
    void *(WINAPI *add)(ULONG, void *);
    GetModuleFileNameA(GetModuleHandleA("c2.dll"), path, MAX_PATH);
    s = strrchr(path, 0x5c);
    strcpy(s ? s + 1 : path, "c2real.dll");
    real = LoadLibraryA(path);
    if (!real) ExitProcess(3);
    realInvoke = (INVOKE)GetProcAddress(real, "_InvokeCompilerPass@12");
    realAbort = (ABORT)GetProcAddress(real, "_AbortCompilerPass@4");
    o = getenv("C2WATCH_OUT");
    if (o && *o) out = fopen(o, "a");
    if (getenv("C2WATCH_LO")) lo = strtoul(getenv("C2WATCH_LO"), 0, 16);
    if (getenv("C2WATCH_HI")) hi = strtoul(getenv("C2WATCH_HI"), 0, 16);
    seq = getenv("C2WATCH_SEQ") != 0;
    plo = lo & ~0xfffUL; phi = (hi + 0xfff) & ~0xfffUL;
    add = (void *(WINAPI *)(ULONG, void *))GetProcAddress(GetModuleHandleA("kernel32.dll"), "AddVectoredExceptionHandler");
    if (!add) return;
    add(1, veh);
    protect(1);
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
BOOL WINAPI DllMain(HINSTANCE h, DWORD why, LPVOID p)
{
    if (why == DLL_PROCESS_DETACH) report();
    return TRUE;
}
