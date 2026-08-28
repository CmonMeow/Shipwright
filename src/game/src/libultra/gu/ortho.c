#include "global.h"

#include "port/frame_interpolation.h"

void guOrthoF(float mf[4][4], float left, float right, float bottom, float top, float near, float far, float scale) {
    int32_t i, j;

    guMtxIdentF(mf);

    mf[0][0] = 2 / (right - left);
    mf[1][1] = 2 / (top - bottom);
    mf[2][2] = -2 / (far - near);
    mf[3][0] = -(right + left) / (right - left);
    mf[3][1] = -(top + bottom) / (top - bottom);
    mf[3][2] = -(far + near) / (far - near);
    mf[3][3] = 1;

    for (i = 0; i < 4; i++) {
        for (j = 0; j < 4; j++) {
            mf[i][j] *= scale;
        }
    }
}

void guOrtho(Mtx* mtx, float left, float right, float bottom, float top, float near, float far, float scale) {
    float mf[4][4];

    guOrthoF(mf, left, right, bottom, top, near, far, scale);

    // guMtxF2L((MtxF*)mf, mtx);
    FrameInterpolation_RecordOpenChild("ortho", 0);
    Matrix_MtxFToMtx((MtxF*)mf, mtx);
    FrameInterpolation_RecordCloseChild();
}
