/* Types of d3s.c (0x459d80..0x45ce60): the object shadow TU ("d3s:" tags, "Schatten" texture). Names are ours. */
#ifndef D3S_H
#define D3S_H
#include "d3.h"                       /* the D3 records and typedefs this header names */

typedef struct D3SCENT {            /* 0x14 bytes, one cached shadow texture (d3s_Cache); D3MESHSUB.cache */
    struct TSTEXTURE *tex;               /* 0x00 */
    void **owner;                   /* 0x04, the slot that uses it (its tex pointer is cleared when taken over) */
    unsigned int time;              /* 0x08, frame of last use */
    unsigned int size;              /* 0x0c, 0 = free */
    signed char flag;               /* 0x10 */
    char pad11[3];
} D3SCENT;

typedef struct D3SVERT {            /* 0x50 bytes, like D3VERTEX */
    float x, y, z;                  /* 0x00 */
    float w;                        /* 0x0c */
    float tu, tv;                   /* 0x10, position in the shadow texture (texels) */
    char pad18[0x28];
    unsigned long c0;               /* 0x40 */
    unsigned long c1;               /* 0x44 */
    float *pos;                     /* 0x48 */
    char pad4c[0x4];
} D3SVERT;

typedef struct D3SPOLY {            /* 0x28 bytes */
    D3SVERT *v[3];                  /* 0x00 */
    int pad0c;
    float *uv;                      /* 0x10 */
    struct TSTEXTURE *tex;               /* 0x14 */
    float z;                        /* 0x18 */
    char pad1c[0xa];
    unsigned char flags;            /* 0x26, 2 = steep, 4 = both sides */
    char pad27;
} D3SPOLY;

typedef struct D3SBUF {             /* the shadow mesh being built: the shadow object's surf[0].info (D3MESH,
                                       material + 0xf4) with the shadow's vertex and polygon types */
    D3SVERT *verts;                 /* 0x00 (0xf4) */
    D3SPOLY *polys;                 /* 0x04 (0xf8) */
    int nverts;                     /* 0x08 (0xfc) */
    int npolys;                     /* 0x0c (0x100) */
} D3SBUF;

/* declared in other units' headers before (moved to the owner) */
void d3_ExitShadows(void);  /* 0x45aee0 */
void d3_InitShadows(unsigned int size, unsigned int maxsize, int ncache, int age, char mips, int polys, int mapsize);  /* 0x45ad80 */
void d3s_CollectLights(void);  /* 0x45c9f0 */
void d3s_ShadowTriangle(float *p, unsigned char *c, unsigned char *dst, int pitch, unsigned char flags, struct D3SMENTRY *base, int radius);  /* 0x45a2e0, a shadow triangle: edges from the three points p (x, y pairs), colours c, radius pixels / rows around */
extern unsigned int d3s_Count;  /* 0x633dc0 */
char d3s_FreeLightCB(D3OBJECT *obj, D3OBJECT *light);  /* 0x45cd60 */
void d3s_FreeShadows(D3OBJECT *obj);  /* 0x45ccd0 */
extern unsigned int d3s_MaxPolys;  /* 0x633dc4 */
extern int d3s_NumUVs;  /* 0xfb925c */
extern int d3s_NumVerts;  /* 0xfb9220 */
extern D3OBJECT * d3s_Object;  /* 0x633448 */
void d3s_ResetShadows(D3OBJECT *obj);  /* 0x45ce00 */
void d3s_UpdateObject(D3OBJECT *obj);  /* 0x45ca20 */
void d3s_UpdateObjectAt(D3OBJECT *obj);  /* 0x45ca60 */

#endif
