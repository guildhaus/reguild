typedef struct { float x, y, z; char pad[0x44]; } V;
typedef struct { V *keys; } S;
typedef struct { char pad[0x1cc]; S *info; } O;
typedef struct { char pad[0x34]; O *obj; char pad2[0x50]; void *scene; } SC;
typedef struct { char pad[0x184]; SC *scene; char sel; } C;
extern C *chars[32];
extern void *defscene;
extern float scx, ccx;
unsigned perm(int a, C *c);

void f(int left, int right) {
  int i, n, iz, sx;
  unsigned bits;
  float px, py, pz;
  C *c;
  V *v;
  S *info;

  for (i = 0; i < 0x20; i++) {
    c = chars[i];
    if (c == 0)
      continue;
    bits = perm(1, c);
    c->sel = 0;
    if ((bits & 0x800) == 0 || c->scene == 0 ||
        c->scene->scene != defscene || (info = c->scene->obj->info) == 0)
      continue;
    px = 0.0f;
    py = 0.0f;
    pz = 0.0f;
    v = info->keys;
    for (n = 8; n != 0; n--, v++) {
      px += v->x;
      py += v->y;
      pz += v->z;
    }
    px *= 0.125;
    py *= 0.125;
    pz *= 0.125;
    iz = (int)(1.0 / pz);
    sx = (int)(scx * iz * px + ccx);
    if (sx >= left && sx <= right)
      c->sel = 1;
  }
}
