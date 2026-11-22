/* d3_sm.c -- terrain height/shadow map ("d3_sm:", "d3sm_"), original file name unknown.
   Names are ours. */
#ifndef UNK_45DB40_H
#define UNK_45DB40_H
#include "d3.h"                       /* the D3 records and typedefs this header names */
#include "d3_sm_types.h"             /* D3SM, D3SMENTRY */

typedef struct D3SMBLOCK {          /* 0x64 bytes, 8x8 of them in a terrain */
    unsigned char lo, hi;           /* height range */
    unsigned char lod;              /* 0x02, sample step of the block (1, 2, 4, 8); 0 = not drawn */
    char pad03[0x61];
} D3SMBLOCK;

typedef struct D3TERRAIN {          /* the terrain a height map is sampled from */
    int size;                       /* 0x000 */
    int bs;                         /* 0x004, block size in cells (d3s_Project divides the cell range by it) */
    char pad008[0x8];
    unsigned char *heights;         /* 0x010 */
    unsigned char *types;           /* 0x014, index into the names, bit 7 = none */
    char pad018[0x78];
    D3VECTOR4 origin;               /* 0x090 */
    D3VECTOR4 axisx;                /* 0x0a0 */
    D3VECTOR4 axisz;                /* 0x0b0 */
    D3VECTOR4 axisy;                /* 0x0c0, axisy.y = height per unit */
    char pad0d0[0x6c];
    D3SMBLOCK blocks[8][8];         /* 0x13c */
} D3TERRAIN;

#define D3TERRAIN_NAMES(t) ((char (*)[0x40])((char *)(t) + 0x1a64))    /* 8 material names, overlaps blocks */

typedef struct D3SMMESH {           /* D3OBJECT.info */
    D3VERTEX *keys;
    D3TRI *tris;                  /* 0x04 */
    int nkeys;                      /* 0x08 */
    int ntris;                      /* 0x0c */
} D3SMMESH;

char d3sm_FindGround(D3SM *h, int x, int z, int *ox, int *oz);           /* 0x45efc0, the cell nearest to
                                                                            (x, z) with a real ground type */
char d3_GetHeightAt(D3SM *h, D3VECTOR *pos, int *px, int *pz, float *py);        /* 0x45f170, the cell of pos and
                                                                            (h != 0) the interpolated height there */

/* declared in other units' headers before (moved to the owner) */
void d3sm_Destroy(D3SM *h);  /* 0x45ef60 */
extern D3SM *d3sm_Shadow;  /* 0x633df8 */
char d3sm_Pick(D3SM *h, int sx, int sy, int *px, int *pz);              /* 0x45f2b0, the cell (px, pz) under the screen point (sx, sy); 0 = none */

#endif
