#include "global.h"

void guRotateF(f32 m[4][4], f32 a, f32 x, f32 y, f32 z) {
    static f32 D_80134D10 = M_PI / 180.0f;

    guNormalize(&x, &y, &z);

    a = a * D_80134D10;

    f32 sine = sinf(a);
    f32 cosine = cosf(a);

    f32 ab = x * y * (1 - cosine);
    f32 bc = y * z * (1 - cosine);
    f32 ca = z * x * (1 - cosine);

    guMtxIdentF(m);

    f32 xs = x * sine;
    f32 ys = y * sine;
    f32 zs = z * sine;

    f32 t = x * x;
    m[0][0] = (1 - t) * cosine + t;
    m[2][1] = bc - xs;
    m[1][2] = bc + xs;
    t = y * y;
    m[1][1] = (1 - t) * cosine + t;
    m[2][0] = ca + ys;
    m[0][2] = ca - ys;
    t = z * z;
    m[2][2] = (1 - t) * cosine + t;
    m[1][0] = ab - zs;
    m[0][1] = ab + zs;
}

void guRotate(Mtx* m, f32 a, f32 x, f32 y, f32 z) {
    f32 mf[4][4];

    guRotateF(mf, a, x, y, z);
    guMtxF2L((MtxF*)mf, m);
}
