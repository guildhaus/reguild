/* d3_vtx.c -- 0x458e70..0x459d80, original file name unknown. Names are ours.
   Mesh vertex animation: key-frame blend weights, vertex and normal transforms into camera space,
   env-map coordinates, bounding box corners.
   Open: the file name, and whether it is one TU with d3_pose.c (the join compiles unchanged).
   unit of rea 8cef7d5b (src/game/unk_4591a0.c); boundaries in decl/text.dcl */
#include <math.h>
#include <windows.h>
#include <d3d.h>
#include "d3.h"
#include "d3_anim.h"
#include "d3_light.h"
#include "d3_vtx.h"
#include "bio.h"

/* d3_EnvMapCoords (was d3_pose.c: only 0x459300 calls it) works on the mesh surface (D3SURF / D3VERTEX); its
   prototypes there are for the other file. */
#include "d3_pose.h"
#include "d3_engine.h"
#include "ts_texture.h"
#include "d3_vec.h"
#include "cm.h"

/* 0x458e70, environment map coordinates of the vertices of a part that has a reflecting texture */
void d3_EnvMapCoords(D3OBJECT *obj, D3SURF *part) {
  D3MATRIX m;
  D3VECTOR4 b;
  D3VECTOR4 nv;
  int n, i, k;
  D3ANIMSLOT *slot;
  D3ANIMFRAME *fr;
  D3VERTEX *v;

  if (obj && part && part->mesh && (n = part->mesh->ntex) > 0) {
    for (i = 0; i < n; i++) {
      if (part->texHandles[i] && part->texHandles[i]->wrap)
        break;
    }
    if (i < n) {
      d3_WorldMatrix(obj, (obj->state & 0x80) ? 0 : d3_Camera, 1, &m);
      slot = d3anim_GetMainSlot(part);
      if (slot && (fr = &slot->data->frames[slot->frame]) != 0) {
        v = part->keys;
        for (i = 0; i < part->nkeys; i++, v++) {
          if (v->used) {
            float d;

            b.x = ((float)fr->normals[i * 3] - 128.0f) * 0.007843138f;
            b.y = ((float)fr->normals[i * 3 + 1] - 128.0f) * 0.007843138f;
            b.z = ((float)fr->normals[i * 3 + 2] - 128.0f) * 0.007843138f;
            nv.x = (m._11 * b.x) + (m._21 * b.y) + (m._31 * b.z);
            nv.y = m._12 * b.x + m._22 * b.y + m._32 * b.z;
            nv.z = m._13 * b.x + m._23 * b.y + m._33 * b.z;
            d = (nv.x * v->x + nv.y * v->y + nv.z * v->z) * -2.0f;
            b.x = (d * nv.x) + v->x;
            b.y = d * nv.y + v->y;
            b.z = nv.z * d + v->z;
            d3_Normalize(&b.v);
            v->tu = (b.x + 1.0f) * 0.5f;
            v->tv = (b.y + 1.0f) * 0.5f;
          }
        }
        return;
      }
      v = part->keys;
      for (i = 0; i < part->nkeys; i++, v++) {
        if (v->used) {
          float d;

          nv.x = (m._11 * v->src->nx) + m._31 * v->src->nz + m._21 * v->src->ny;
          nv.y = (m._12 * v->src->nx) + m._32 * v->src->nz + m._22 * v->src->ny;
          nv.z = (m._13 * v->src->nx) + m._33 * v->src->nz + m._23 * v->src->ny;
          d = (nv.x * v->x + nv.y * v->y + nv.z * v->z) * -2.0f;
          b.x = d * nv.x + v->x;
          b.y = d * nv.y + v->y;
          b.z = nv.z * d + v->z;
          d3_Normalize(&b.v);
          v->tu = (b.x + 1.0f) * 0.5f;
          v->tv = (b.y + 1.0f) * 0.5f;
        }
      }
    }
  }
}

/* 0x4591a0, blend weights of the two key frames around the current time of an animation slot */

/* 0x4591a0, the blend weights of the two key frames around the current sub-frame time of a vertex animation
   slot s: *a the weight of its frame, *b of its nextFrame (flags bit 1 blends backwards and swaps them; at the
   animation's ends the cosine eases the weight towards the frame) */
void d3_AnimBlendWeights(D3ANIMSLOT *s, float *a, float *b) {
  D3ANIMDATA *d = s->data;
  int n;
  float t;
  union {
    volatile float v; /* the store / read the original keeps in its slot */
    float f;          /* the non-volatile read (a fld 1.0f; fsub [r] order) */
  } r;

  if (s->flags.b[1] & 2) {
    t = s->subFrame * (1.0f / d->frames[s->nextFrame].count);
    if (s->flags.b[1] & 4) {
      n = d->nframes;
      if (n <= 2 && !(0 < s->nextFrame)) {
        t = ((float)cos(t * 3.14159274f) + 1.0f) * 0.5f;
        t = 1.0f - t;
      } else if (!(0 < s->nextFrame)) {
        t = (float)cos(t * 1.5707964f);
        t = 1.0f - t;
      } else if (s->frame >= n - 1) {
        t = (float)cos((t + 1.0f) * 1.5707964f) + 1.0f;
        t = 1.0f - t;
      }
    }
    r.v = t - (1.0f / d->frames[s->nextFrame].count) * s->frameTime;
    /* the non-volatile read is needed: it makes VC6 load the 1.0f first (fld 1.0f; fsub [r]) */
    *a = 1.0f - r.f;
    *b = r.v;
  } else {
    t = s->subFrame * (1.0f / d->frames[s->frame].count);
    if (s->flags.b[1] & 4) {
      n = d->nframes;
      if (n <= 2 && s->frame + 1 >= n - 1) {
        t = ((float)cos(t * 3.14159274f) + 1.0f) * 0.5f;
        t = 1.0f - t;
      } else if (s->nextFrame >= n - 1) {
        t = (float)cos((t + 1.0f) * 1.5707964f) + 1.0f;
        t = 1.0f - t;
      } else if (!(0 < s->frame)) {
        t = (float)cos(t * 1.5707964f);
        t = 1.0f - t;
      }
    }
    t = (1.0f / d->frames[s->frame].count) * s->frameTime + t;
    r.v = 1.0f - t;
    *a = t;
    *b = r.v;
  }
}

/* 0x459300, transforms the animated mesh's vertices: per active slot it blends the two key frames' base and
   scale by the blend weights and adds the result into info->keys, then applies the object matrix (tracking the
   z range in d3_MinZ / d3_MaxZ when the vertex normal bit is set) */
void d3_TransformVerts(D3OBJECT *obj, D3SURF *info, D3MATRIX *m) {
  D3SURF *ctl = info;
  D3VERTEX *v;
  D3BGFVERT *sp;
  D3ANIMSLOT *sl;
  D3ANIMFRAME *f1, *f2;
  unsigned char *p1, *p2;
  float a, b;
  D3VECTOR4 s1, b1, s2, b2, q1, q2;
  float z;
  float vx, vy, vz;
  int i, k;
  char first;

  /* @original a disabled projecting form of the four transform loops. Its text is ours, not recovered: the
     original function holds statements that leave no code. Their IL is counted in C2's symbol ids, and the ids
     order the operands of every float sum and product of the live loops (the rule in notes/vc6-matching.md,
     "Operand order of commutative ops": which product of a row is loaded first, sp->x * m->_12 against m->_12 *
     sp->x, sp->x + v->x against v->x + sp->x). Measured with dead probes: the original's order needs 31..46
     blocks of 32 expression temporaries where the live code has 20, and two more local ids in front of the
     blend loops' i and v (one or three: other orders). This block has 33 blocks and the two locals t and w. */
  if (0) {
    D3VERTEX *t;
    float w;

    if (ctl->active) {
      if (obj->objectFlags & 0x20) {
        for (t = info->keys; t < info->keys + info->nkeys; t++) {
          w = (t->src->x + t->x) * m->_14 + (t->src->y + t->y) * m->_24 +
              (t->src->z + t->z) * m->_34 + m->_44;
          t->x = (t->src->x + t->x) * m->_11 + (t->src->y + t->y) * m->_21 +
                 (t->src->z + t->z) * m->_31 + m->_41;
          t->y = (t->src->x + t->x) * m->_12 + (t->src->y + t->y) * m->_22 +
                 (t->src->z + t->z) * m->_32 + m->_42;
          t->z = (t->src->x + t->x) * m->_13 + (t->src->y + t->y) * m->_23 +
                 (t->src->z + t->z) * m->_33 + m->_43;
          t->x /= w;
          t->y /= w;
          if (t->z < d3_MinZ)
            d3_MinZ = t->z;
          if (t->z > d3_MaxZ)
            d3_MaxZ = t->z;
        }
      } else {
        for (t = info->keys; t < info->keys + info->nkeys; t++) {
          w = (t->src->x + t->x) * m->_14 + (t->src->y + t->y) * m->_24 +
              (t->src->z + t->z) * m->_34 + m->_44;
          t->x = (t->src->x + t->x) * m->_11 + (t->src->y + t->y) * m->_21 +
                 (t->src->z + t->z) * m->_31 + m->_41;
          t->y = (t->src->x + t->x) * m->_12 + (t->src->y + t->y) * m->_22 +
                 (t->src->z + t->z) * m->_32 + m->_42;
          t->z = (t->src->x + t->x) * m->_13 + (t->src->y + t->y) * m->_23 +
                 (t->src->z + t->z) * m->_33 + m->_43;
          t->x /= w;
          t->y /= w;
        }
      }
    } else if (obj->objectFlags & 0x20) {
      for (t = info->keys; t < info->keys + info->nkeys; t++) {
        w = t->src->x * m->_14 + t->src->y * m->_24 + t->src->z * m->_34 + m->_44;
        t->x = t->src->x * m->_11 + t->src->y * m->_21 + t->src->z * m->_31 + m->_41;
        t->y = t->src->x * m->_12 + t->src->y * m->_22 + t->src->z * m->_32 + m->_42;
        t->z = t->src->x * m->_13 + t->src->y * m->_23 + t->src->z * m->_33 + m->_43;
        t->x /= w;
        t->y /= w;
        if (t->z < d3_MinZ)
          d3_MinZ = t->z;
        if (t->z > d3_MaxZ)
          d3_MaxZ = t->z;
      }
    } else {
      for (t = info->keys; t < info->keys + info->nkeys; t++) {
        w = t->src->x * m->_14 + t->src->y * m->_24 + t->src->z * m->_34 + m->_44;
        t->x = t->src->x * m->_11 + t->src->y * m->_21 + t->src->z * m->_31 + m->_41;
        t->y = t->src->x * m->_12 + t->src->y * m->_22 + t->src->z * m->_32 + m->_42;
        t->z = t->src->x * m->_13 + t->src->y * m->_23 + t->src->z * m->_33 + m->_43;
        t->x /= w;
        t->y /= w;
      }
    }
    d3_EnvMapCoords(obj, info);
    return;
  }
  if (ctl->active) {
    first = 1;
    for (k = 0; k < 3; k++) {
      sl = &ctl->slots[k];
      if (sl->data && sl->weight != 0.0f) {
        f2 = sl->data->frames;
        f1 = &f2[sl->frame];
        f2 = &f2[sl->nextFrame];
        p1 = f1->packed;
        p2 = f2->packed;
        s1.x = f1->scale.x;
        s1.y = f1->scale.y;
        s1.z = f1->scale.z;
        s1.x = (sl->weight) * s1.x;
        s1.y = (sl->weight) * s1.y;
        s1.z = (sl->weight) * s1.z;
        b1.x = f1->base.x;
        b1.y = f1->base.y;
        b1.z = f1->base.z;
        b1.x = (sl->weight) * b1.x;
        b1.y = (sl->weight) * b1.y;
        b1.z = (sl->weight) * b1.z;
        s2.x = f2->scale.x;
        s2.y = f2->scale.y;
        s2.z = f2->scale.z;
        s2.x = (sl->weight) * s2.x;
        s2.y = (sl->weight) * s2.y;
        s2.z = (sl->weight) * s2.z;
        b2.x = f2->base.x;
        b2.y = f2->base.y;
        b2.z = f2->base.z;
        b2.x = (sl->weight) * b2.x;
        b2.y = (sl->weight) * b2.y;
        b2.z = (sl->weight) * b2.z;
        d3_AnimBlendWeights(sl, &a, &b);
        if (first) {
          for (i = 0, v = info->keys; i < info->nkeys; i++, p1 += 3, p2 += 3, v++) {
            q1.x = ((float)p1[0] * s1.x + b1.x) * b;
            q1.y = ((float)p1[1] * s1.y + b1.y) * b;
            q1.z = ((float)p1[2] * s1.z + b1.z) * b;
            q2.x = ((float)p2[0] * s2.x + b2.x) * a;
            q2.y = ((float)p2[1] * s2.y + b2.y) * a;
            q2.z = ((float)p2[2] * s2.z + b2.z) * a;
            v->x = q2.x + q1.x;
            v->y = q2.y + q1.y;
            v->z = q2.z + q1.z;
          }
          first = 0;
        } else {
          for (i = 0, v = info->keys; i < info->nkeys; i++, p1 += 3, p2 += 3, v++) {
            q1.x = ((float)p1[0] * s1.x + b1.x) * b;
            q1.y = ((float)p1[1] * s1.y + b1.y) * b;
            q1.z = ((float)p1[2] * s1.z + b1.z) * b;
            q2.x = ((float)p2[0] * s2.x + b2.x) * a;
            q2.y = ((float)p2[1] * s2.y + b2.y) * a;
            q2.z = ((float)p2[2] * s2.z + b2.z) * a;
            q1.x = q2.x + q1.x;
            q1.y = q2.y + q1.y;
            q1.z = q2.z + q1.z;
            v->x += q1.x;
            v->y += q1.y;
            v->z += q1.z;
          }
        }
      }
    }
    i = 0;
    if (obj->objectFlags & 0x20) {
      v = info->keys;
      if (0 < info->nkeys) {
        do {
          sp = v->src;
          vx = sp->x + v->x;
          vy = sp->y + v->y;
          vz = sp->z + v->z;
          v->x = vx * m->_11 + vy * m->_21 + vz * m->_31 + m->_41;
          v->y = vx * m->_12 + vy * m->_22 + vz * m->_32 + m->_42;
          z = vx * m->_13 + vy * m->_23 + vz * m->_33 + m->_43;
          v->z = z;
          if (!(d3_MinZ < z))
            d3_MinZ = z;
          if (!(v->z < d3_MaxZ))
            d3_MaxZ = (v->z);
          i++;
          v++;
        } while (i < info->nkeys);
        d3_EnvMapCoords(obj, info);
        return;
      }
    } else {
      v = info->keys;
      if (0 < info->nkeys) {
        do {
          sp = v->src;
          vx = sp->x + v->x;
          vy = sp->y + v->y;
          vz = sp->z + v->z;
          v->x = vx * m->_11 + vy * m->_21 + vz * m->_31 + m->_41;
          v->y = vx * m->_12 + vy * m->_22 + vz * m->_32 + m->_42;
          v->z = vx * m->_13 + vy * m->_23 + vz * m->_33 + m->_43;
          i++;
          v++;
        } while (i < info->nkeys);
        d3_EnvMapCoords(obj, info);
        return;
      }
    }
  } else {
    v = info->keys;
    i = 0;
    if (obj->objectFlags & 0x20) {
      if (0 < info->nkeys) {
        do {
          sp = v->src;
          v->x = sp->x * m->_11 + sp->y * m->_21 + sp->z * m->_31 + m->_41;
          v->y = sp->x * m->_12 + sp->y * m->_22 + sp->z * m->_32 + m->_42;
          z = sp->x * m->_13 + sp->y * m->_23 + sp->z * m->_33 + m->_43;
          v->z = z;
          if (!(d3_MinZ < z))
            d3_MinZ = z;
          if (!(v->z < d3_MaxZ))
            d3_MaxZ = (v->z);
          i++;
          v++;
        } while (i < info->nkeys);
        d3_EnvMapCoords(obj, info);
        return;
      }
    } else if (0 < info->nkeys) {
      do {
        sp = v->src;
        v->x = sp->x * m->_11 + sp->y * m->_21 + sp->z * m->_31 + m->_41;
        v->y = sp->x * m->_12 + sp->y * m->_22 + sp->z * m->_32 + m->_42;
        v->z = sp->x * m->_13 + sp->y * m->_23 + sp->z * m->_33 + m->_43;
        i++;
        v++;
      } while (i < info->nkeys);
    }
  }
  d3_EnvMapCoords(obj, info);
}
/* 0x4598d0, D3BGF::fn1ec of the stock mesh driver (d3_io_t.c): the normals of a mesh without key frames, from the
   normal of each vertex's source point (a D3VECTOR at 0x0c of the source) in camera space */

void d3_TransformNormals(D3OBJECT *obj, D3SURF *info) {
  D3MATRIX m;
  D3VERTEX *v;
  D3BGFVERT *n;
  int i;

  d3_WorldMatrix(obj, (obj->state & 0x80) ? 0 : d3_Camera, 0, &m);
  v = info->keys;
  for (i = 0; i < info->nkeys; i++, v++) {
    n = v->src;
    v->nx = m._11 * n->nx + m._21 * n->ny + m._31 * n->nz;
    v->ny = m._12 * n->nx + m._22 * n->ny + m._32 * n->nz;
    v->nz = m._13 * n->nx + m._23 * n->ny + m._33 * n->nz;
  }
}
/* 0x459980, normals of the object from the packed normals of key frame f (3 bytes per vertex) in camera space;
   without a frame the driver does it. Not the matrix d3_light.h declares for the third argument: it is a D3ANIMFRAME. */

void d3_FrameNormals(D3OBJECT *obj, D3SURF *info, D3ANIMFRAME *f) {
  D3MATRIX m;
  D3VERTEX *v;
  float x, y, z;
  int i;

  if (!f) {
    info->mesh->TransformNormals(obj, info);
    return;
  }
  d3_WorldMatrix(obj, (obj->state & 0x80) ? 0 : d3_Camera, 1, &m);
  v = info->keys;
  for (i = 0; i < info->nkeys; i++, v++) {
    x = (f->normals[i * 3] - 128.0f) * 0.007843138f;
    y = (f->normals[i * 3 + 1] - 128.0f) * 0.007843138f;
    z = (f->normals[i * 3 + 2] - 128.0f) * 0.007843138f;
    v->nx = m._11 * x + m._21 * y + m._31 * z;
    v->ny = m._12 * x + m._22 * y + m._32 * z;
    v->nz = m._13 * x + m._23 * y + m._33 * z;
  }
}
/* 0x459ab0, transforms the 8 corners of the bounding box that follow the vertices of the mesh by the matrix m,
   from the corners of the source (no animation) or of the union of the boxes of the key frames in use.
   Returns the corners (0 if the mesh has no vertices). info is a D3SURF (d3_anim.h) here. */

D3VERTEX *d3_GetBoxVerts(D3OBJECT *obj, D3SURF *info, D3MATRIX *mat) {
/* d = s * m for a point (row vector, w = 1); the parenthesised components give VC6's operand order */
#define XFORM(d, s, m)                                                         \
  ((d)->x = ((s)->x) * (m)->_11 + ((s)->y) * (m)->_21 + ((s)->z) * (m)->_31 +  \
            (m)->_41,                                                          \
   (d)->y = ((s)->x) * (m)->_12 + ((s)->y) * (m)->_22 + ((s)->z) * (m)->_32 +  \
            (m)->_42,                                                          \
   (d)->z = ((s)->x) * (m)->_13 + ((s)->y) * (m)->_23 + ((s)->z) * (m)->_33 +  \
            (m)->_43)
  D3SURF *ctl = info;
  D3MATRIX *m = mat;
  D3VERTEX *v;
  D3VECTOR4 mn, mx;
  D3VECTOR4 c[8];
  D3ANIMFRAME *fr;
  int i;
  int n = info->nkeys;

  if (n == 0)
    return 0;
  {
    if (ctl->active) {
      mn.x = 1e10f;
      mn.y = 1e10f;
      mn.z = 1e10f;
      mx.x = -1e10f;
      mx.y = -1e10f;
      mx.z = -1e10f;
      for (i = 0; i < 3; i++) {
        if (ctl->slots[i].data) {
          fr = &ctl->slots[i].data->frames[ctl->slots[i].frame];
          mn.x = min(mn.x, fr->min.x);
          mn.y = min(mn.y, fr->min.y);
          mn.z = min(mn.z, fr->min.z);
          mx.x = max(mx.x, fr->max.x);
          mx.y = max(mx.y, fr->max.y);
          mx.z = max(mx.z, fr->max.z);
        }
      }
      c[0].x = mn.x;
      c[0].y = mn.y;
      c[0].z = mn.z;
      c[1].x = mx.x;
      c[1].y = mn.y;
      c[1].z = mn.z;
      c[2].x = mn.x;
      c[2].y = mx.y;
      c[2].z = mn.z;
      c[3].x = mx.x;
      c[3].y = mx.y;
      c[3].z = mn.z;
      c[4].x = mn.x;
      c[4].y = mn.y;
      c[4].z = mx.z;
      c[5].x = mx.x;
      c[5].y = mn.y;
      c[5].z = mx.z;
      c[6].x = mn.x;
      c[6].y = mx.y;
      c[6].z = mx.z;
      c[7].x = mx.x;
      c[7].y = mx.y;
      c[7].z = mx.z;
      v = info->keys + n;
      for (i = 0; i < 8; i++, v++) {
        XFORM(v, &c[i], m);
      }

    } else {
      v = info->keys + n;
      for (i = 0; i < 8; i++, v++) {
        D3BGFVERT *p = v->src;

        /* the source point written out: its parentheses give the original's operand order (matching trick) */
        v->x = p->x * m->_11 + (p->y) * m->_21 + p->z * m->_31 + m->_41;
        v->y = p->x * (m->_12) + (p->y) * m->_22 + (p->z) * (m->_32) + m->_42;
        v->z = p->x * m->_13 + (p->y) * (m->_23) + (p->z) * (m->_33) + m->_43;
      }
    }
    return info->keys + info->nkeys;
  }
#undef XFORM
}
