#include "global.h"

void guLookAtF(f32 mf[4][4], f32 xEye, f32 yEye, f32 zEye, f32 xAt, f32 yAt, f32 zAt, f32 xUp, f32 yUp, f32 zUp) {

    guMtxIdentF(mf);

    f32 xLook = xAt - xEye;
    f32 yLook = yAt - yEye;
    f32 zLook = zAt - zEye;
    f32 length = -1.0 / sqrtf(SQ(xLook) + SQ(yLook) + SQ(zLook));
    xLook *= length;
    yLook *= length;
    zLook *= length;

    f32 xRight = yUp * zLook - zUp * yLook;
    f32 yRight = zUp * xLook - xUp * zLook;
    f32 zRight = xUp * yLook - yUp * xLook;
    length = 1.0 / sqrtf(SQ(xRight) + SQ(yRight) + SQ(zRight));
    xRight *= length;
    yRight *= length;
    zRight *= length;

    xUp = yLook * zRight - zLook * yRight;
    yUp = zLook * xRight - xLook * zRight;
    zUp = xLook * yRight - yLook * xRight;
    length = 1.0 / sqrtf(SQ(xUp) + SQ(yUp) + SQ(zUp));
    xUp *= length;
    yUp *= length;
    zUp *= length;

    mf[0][0] = xRight;
    mf[1][0] = yRight;
    mf[2][0] = zRight;
    mf[3][0] = -(xEye * xRight + yEye * yRight + zEye * zRight);

    mf[0][1] = xUp;
    mf[1][1] = yUp;
    mf[2][1] = zUp;
    mf[3][1] = -(xEye * xUp + yEye * yUp + zEye * zUp);

    mf[0][2] = xLook;
    mf[1][2] = yLook;
    mf[2][2] = zLook;
    mf[3][2] = -(xEye * xLook + yEye * yLook + zEye * zLook);

    mf[0][3] = 0;
    mf[1][3] = 0;
    mf[2][3] = 0;
    mf[3][3] = 1;
}

void guLookAt(Mtx* m, f32 xEye, f32 yEye, f32 zEye, f32 xAt, f32 yAt, f32 zAt, f32 xUp, f32 yUp, f32 zUp) {
    f32 mf[4][4];

    guLookAtF(mf, xEye, yEye, zEye, xAt, yAt, zAt, xUp, yUp, zUp);

    guMtxF2L((MtxF*)mf, m);
}
