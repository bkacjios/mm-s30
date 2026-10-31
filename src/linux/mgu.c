/*
 * Matrix helpers for the port's float Mtx (include/PR/gbi.h), replacing
 * libultra's MIPS assembly (src/libultra/mgu). From ports/oot.
 */
#include "ultra64.h"
#include <math.h>
#include <string.h>

void guMtxF2L(f32 mf[4][4], Mtx* m) {
    memcpy(m->m, mf, sizeof(m->m));
}

void guMtxL2F(f32 mf[4][4], Mtx* m) {
    memcpy(mf, m->m, sizeof(m->m));
}

void guMtxIdentF(f32 mf[4][4]) {
    s32 i, j;

    for (i = 0; i < 4; i++) {
        for (j = 0; j < 4; j++) {
            mf[i][j] = (i == j) ? 1.0f : 0.0f;
        }
    }
}

void guMtxIdent(Mtx* m) {
    guMtxIdentF(m->m);
}

void guNormalize(f32* x, f32* y, f32* z) {
    f32 m = 1.0f / sqrtf(*x * *x + *y * *y + *z * *z);

    *x *= m;
    *y *= m;
    *z *= m;
}

void guScale(Mtx* m, f32 x, f32 y, f32 z) {
    guMtxIdentF(m->m);
    m->m[0][0] = x;
    m->m[1][1] = y;
    m->m[2][2] = z;
}

void guTranslate(Mtx* m, f32 x, f32 y, f32 z) {
    guMtxIdentF(m->m);
    m->m[3][0] = x;
    m->m[3][1] = y;
    m->m[3][2] = z;
}
