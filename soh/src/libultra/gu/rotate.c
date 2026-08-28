#include "global.h"

void guRotateF(float m[4][4], float a, float x, float y, float z) {
    static float D_80134D10 = M_PI / 180.0f;

    guNormalize(&x, &y, &z);

    a = a * D_80134D10;

    float sine = sinf(a);
    float cosine = cosf(a);

    float ab = x * y * (1 - cosine);
    float bc = y * z * (1 - cosine);
    float ca = z * x * (1 - cosine);

    guMtxIdentF(m);

    float xs = x * sine;
    float ys = y * sine;
    float zs = z * sine;

    float t = x * x;
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

void guRotate(Mtx* m, float a, float x, float y, float z) {
    float mf[4][4];

    guRotateF(mf, a, x, y, z);
    guMtxF2L((MtxF*)mf, m);
}
