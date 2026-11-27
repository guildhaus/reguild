/* Probe for the frame's reference counts (c2frame.py L rows) against the references left in the listing:
   which constructs are counted at layout time and removed afterwards. Each function keeps x in memory by
   four busier values; see notes/vc6-matching.md "How the frame is laid out". */
int g(int);
void h(int *);
extern int ga, gb;
/* four values that keep ebx / ebp / esi / edi, so that x / n live in memory */
#define BUSY g(p1 + p2), g(p3 + p4), g(p1 + p3), g(p2 + p4), g(p1 + p4), g(p2 + p3)

/* a: the same store in both arms of an if / else (tail merged after the frame) */
int fa(int c, int p1, int p2, int p3, int p4) {
  int x, y;
  BUSY;
  h(&y);
  if (c) {
    g(1);
    x = ga;
  } else {
    g(2);
    x = ga;
  }
  g(y);
  g(c);
  BUSY;
  return x;
}

/* b: the same load in both arms (a call argument) */
int fb(int c, int p1, int p2, int p3, int p4) {
  int x, y;
  BUSY;
  h(&y);
  x = ga;
  g(y);
  if (c) {
    ga = 1;
    g(x);
  } else {
    ga = 2;
    g(x);
  }
  g(c);
  BUSY;
  return y;
}

/* c: a register counter with two loop exits (an exit store per edge) */
int fc(int c, int p1, int p2, int p3, int p4) {
  int n, y, i;
  BUSY;
  h(&y);
  n = 0;
  for (i = 0; i < c; i++) {
    if (g(i))
      n++;
    else if (i)
      break;
  }
  g(y);
  g(c);
  BUSY;
  g(i);
  return n;
}

/* d: three exits */
int fd(int c, int p1, int p2, int p3, int p4) {
  int n, y, i;
  BUSY;
  h(&y);
  n = 0;
  for (i = 0; i < c; i++) {
    if (g(i))
      n++;
    else if (i)
      break;
    if (ga)
      break;
  }
  g(y);
  g(c);
  BUSY;
  g(i);
  return n;
}

/* e: a store followed by a load of the same local (store-to-load) */
int fe(int c, int p1, int p2, int p3, int p4) {
  int x, y;
  BUSY;
  h(&y);
  x = g(c);
  if (x)
    g(y);
  g(c);
  BUSY;
  g(y);
  return x;
}

/* f: the same return value store before two returns that share the epilogue */
int ff(int c, int p1, int p2, int p3, int p4) {
  int x, y;
  BUSY;
  h(&y);
  x = ga;
  g(y);
  if (c) {
    g(1);
    BUSY;
    return x;
  }
  g(2);
  BUSY;
  return x;
}
