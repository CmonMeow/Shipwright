#include "global.h"
#include "vt.h"

#include "rendering/FrameInterpolation.h"

// clang-format off
MtxF sMtxFClear = {
    1.0f, 0.0f, 0.0f, 0.0f,
    0.0f, 1.0f, 0.0f, 0.0f,
    0.0f, 0.0f, 1.0f, 0.0f,
    0.0f, 0.0f, 0.0f, 1.0f,
};
// clang-format on

/**
 * Multiplies the matrix mf by a 4 components column vector [ src , 1 ] and writes the resulting 4 components to xyzDest
 * and wDest.
 *
 * \f[ \begin{bmatrix} \texttt{xyzDest} \\ \texttt{wDest} \\ \end{bmatrix}
 *      = [\texttt{mf}] \cdot
 *        \begin{bmatrix} \texttt{src} \\ 1 \\ \end{bmatrix}
 * \f]
 */
void SkinMatrix_Vec3fMtxFMultXYZW(MtxF* mf, Vec3f* src, Vec3f* xyzDest, float* wDest) {
    xyzDest->x = mf->xw + ((src->x * mf->xx) + (src->y * mf->xy) + (src->z * mf->xz));
    xyzDest->y = mf->yw + ((src->x * mf->yx) + (src->y * mf->yy) + (src->z * mf->yz));
    xyzDest->z = mf->zw + ((src->x * mf->zx) + (src->y * mf->zy) + (src->z * mf->zz));
    *wDest = mf->ww + ((src->x * mf->wx) + (src->y * mf->wy) + (src->z * mf->wz));
}

/**
 * Multiplies the matrix mf by a 4 components column vector [ src , 1 ] and writes the resulting xyz components to dest.
 *
 * \f[ \begin{bmatrix} \texttt{dest} \\ - \\ \end{bmatrix}
 *      = [\texttt{mf}] \cdot
 *        \begin{bmatrix} \texttt{src} \\ 1 \\ \end{bmatrix}
 * \f]
 */
void SkinMatrix_Vec3fMtxFMultXYZ(MtxF* mf, Vec3f* src, Vec3f* dest) {
    float mx = mf->xx;
    float my = mf->xy;
    float mz = mf->xz;
    float mw = mf->xw;

    dest->x = mw + ((src->x * mx) + (src->y * my) + (src->z * mz));
    mx = mf->yx;
    my = mf->yy;
    mz = mf->yz;
    mw = mf->yw;
    dest->y = mw + ((src->x * mx) + (src->y * my) + (src->z * mz));
    mx = mf->zx;
    my = mf->zy;
    mz = mf->zz;
    mw = mf->zw;
    dest->z = mw + ((src->x * mx) + (src->y * my) + (src->z * mz));
}

/**
 * Matrix multiplication, dest = mfA * mfB.
 * mfB and dest should not be the same matrix.
 */
void SkinMatrix_MtxFMtxFMult(MtxF* mfA, MtxF* mfB, MtxF* dest) {
    //---ROW1---
    float rx = mfA->xx;
    float ry = mfA->xy;
    float rz = mfA->xz;
    float rw = mfA->xw;
    //--------

    float cx = mfB->xx;
    float cy = mfB->yx;
    float cz = mfB->zx;
    float cw = mfB->wx;
    dest->xx = (rx * cx) + (ry * cy) + (rz * cz) + (rw * cw);

    cx = mfB->xy;
    cy = mfB->yy;
    cz = mfB->zy;
    cw = mfB->wy;
    dest->xy = (rx * cx) + (ry * cy) + (rz * cz) + (rw * cw);

    cx = mfB->xz;
    cy = mfB->yz;
    cz = mfB->zz;
    cw = mfB->wz;
    dest->xz = (rx * cx) + (ry * cy) + (rz * cz) + (rw * cw);

    cx = mfB->xw;
    cy = mfB->yw;
    cz = mfB->zw;
    cw = mfB->ww;
    dest->xw = (rx * cx) + (ry * cy) + (rz * cz) + (rw * cw);

    //---ROW2---
    rx = mfA->yx;
    ry = mfA->yy;
    rz = mfA->yz;
    rw = mfA->yw;
    //--------
    cx = mfB->xx;
    cy = mfB->yx;
    cz = mfB->zx;
    cw = mfB->wx;
    dest->yx = (rx * cx) + (ry * cy) + (rz * cz) + (rw * cw);

    cx = mfB->xy;
    cy = mfB->yy;
    cz = mfB->zy;
    cw = mfB->wy;
    dest->yy = (rx * cx) + (ry * cy) + (rz * cz) + (rw * cw);

    cx = mfB->xz;
    cy = mfB->yz;
    cz = mfB->zz;
    cw = mfB->wz;
    dest->yz = (rx * cx) + (ry * cy) + (rz * cz) + (rw * cw);

    cx = mfB->xw;
    cy = mfB->yw;
    cz = mfB->zw;
    cw = mfB->ww;
    dest->yw = (rx * cx) + (ry * cy) + (rz * cz) + (rw * cw);

    //---ROW3---
    rx = mfA->zx;
    ry = mfA->zy;
    rz = mfA->zz;
    rw = mfA->zw;
    //--------
    cx = mfB->xx;
    cy = mfB->yx;
    cz = mfB->zx;
    cw = mfB->wx;
    dest->zx = (rx * cx) + (ry * cy) + (rz * cz) + (rw * cw);

    cx = mfB->xy;
    cy = mfB->yy;
    cz = mfB->zy;
    cw = mfB->wy;
    dest->zy = (rx * cx) + (ry * cy) + (rz * cz) + (rw * cw);

    cx = mfB->xz;
    cy = mfB->yz;
    cz = mfB->zz;
    cw = mfB->wz;
    dest->zz = (rx * cx) + (ry * cy) + (rz * cz) + (rw * cw);

    cx = mfB->xw;
    cy = mfB->yw;
    cz = mfB->zw;
    cw = mfB->ww;
    dest->zw = (rx * cx) + (ry * cy) + (rz * cz) + (rw * cw);

    //---ROW4---
    rx = mfA->wx;
    ry = mfA->wy;
    rz = mfA->wz;
    rw = mfA->ww;
    //--------
    cx = mfB->xx;
    cy = mfB->yx;
    cz = mfB->zx;
    cw = mfB->wx;
    dest->wx = (rx * cx) + (ry * cy) + (rz * cz) + (rw * cw);

    cx = mfB->xy;
    cy = mfB->yy;
    cz = mfB->zy;
    cw = mfB->wy;
    dest->wy = (rx * cx) + (ry * cy) + (rz * cz) + (rw * cw);

    cx = mfB->xz;
    cy = mfB->yz;
    cz = mfB->zz;
    cw = mfB->wz;
    dest->wz = (rx * cx) + (ry * cy) + (rz * cz) + (rw * cw);

    cx = mfB->xw;
    cy = mfB->yw;
    cz = mfB->zw;
    cw = mfB->ww;
    dest->ww = (rx * cx) + (ry * cy) + (rz * cz) + (rw * cw);
}

/**
 * "Clear" in this file means the identity matrix.
 */
void SkinMatrix_GetClear(MtxF** mfp) {
    *mfp = &sMtxFClear;
}

void SkinMatrix_Clear(MtxF* mf) {
    mf->xx = 1.0f;
    mf->yy = 1.0f;
    mf->zz = 1.0f;
    mf->ww = 1.0f;
    mf->yx = 0.0f;
    mf->zx = 0.0f;
    mf->wx = 0.0f;
    mf->xy = 0.0f;
    mf->zy = 0.0f;
    mf->wy = 0.0f;
    mf->xz = 0.0f;
    mf->yz = 0.0f;
    mf->wz = 0.0f;
    mf->xw = 0.0f;
    mf->yw = 0.0f;
    mf->zw = 0.0f;
}

void SkinMatrix_MtxFCopy(MtxF* src, MtxF* dest) {
    dest->xx = src->xx;
    dest->yx = src->yx;
    dest->zx = src->zx;
    dest->wx = src->wx;
    dest->xy = src->xy;
    dest->yy = src->yy;
    dest->zy = src->zy;
    dest->wy = src->wy;
    dest->xz = src->xz;
    dest->yz = src->yz;
    dest->zz = src->zz;
    dest->wz = src->wz;
    dest->xw = src->xw;
    dest->yw = src->yw;
    dest->zw = src->zw;
    dest->ww = src->ww;
}

/**
 * Inverts a matrix using the Gauss-Jordan method.
 * returns 0 if successfully inverted
 * returns 2 if matrix non-invertible (0 determinant)
 */
int32_t SkinMatrix_Invert(MtxF* src, MtxF* dest) {
    MtxF mfCopy;
    int32_t i;
    float temp1 = { 0 };
    int32_t thisCol;

    SkinMatrix_MtxFCopy(src, &mfCopy);
    SkinMatrix_Clear(dest);
    for (thisCol = 0; thisCol < 4; thisCol++) {
        int32_t thisRow = thisCol;
        while ((thisRow < 4) && (fabsf(mfCopy.mf[thisCol][thisRow]) < 0.0005f)) {
            thisRow++;
        }
        if (thisRow == 4) {
            // Reaching row = 4 means the column is either all 0 or a duplicate column.
            // Therefore src is a singular matrix (0 determinant).

            osSyncPrintf(VT_COL(YELLOW, BLACK));
            osSyncPrintf("Skin_Matrix_InverseMatrix():逆行列つくれません\n");
            osSyncPrintf(VT_RST);
            return 2;
        }

        if (thisRow != thisCol) {
            // Diagonal element mf[thisCol][thisCol] is zero.
            // Swap the rows thisCol and thisRow.
            for (i = 0; i < 4; i++) {
                temp1 = mfCopy.mf[i][thisRow];
                mfCopy.mf[i][thisRow] = mfCopy.mf[i][thisCol];
                mfCopy.mf[i][thisCol] = temp1;

                float temp2 = dest->mf[i][thisRow];
                dest->mf[i][thisRow] = dest->mf[i][thisCol];
                dest->mf[i][thisCol] = temp2;
            }
        }

        // Scale this whole row such that the diagonal element is 1.
        temp1 = mfCopy.mf[thisCol][thisCol];
        for (i = 0; i < 4; i++) {
            mfCopy.mf[i][thisCol] /= temp1;
            dest->mf[i][thisCol] /= temp1;
        }

        for (thisRow = 0; thisRow < 4; thisRow++) {
            if (thisRow != thisCol) {
                temp1 = mfCopy.mf[thisCol][thisRow];
                for (i = 0; i < 4; i++) {
                    mfCopy.mf[i][thisRow] -= mfCopy.mf[i][thisCol] * temp1;
                    dest->mf[i][thisRow] -= dest->mf[i][thisCol] * temp1;
                }
            }
        }
    }
    return 0;
}

/**
 * Produces a matrix which scales x,y,z components of vectors or x,y,z rows of matrices (when applied on LHS)
 */
void SkinMatrix_SetScale(MtxF* mf, float x, float y, float z) {
    mf->yx = 0.0f;
    mf->zx = 0.0f;
    mf->wx = 0.0f;
    mf->xy = 0.0f;
    mf->zy = 0.0f;
    mf->wy = 0.0f;
    mf->xz = 0.0f;
    mf->yz = 0.0f;
    mf->wz = 0.0f;
    mf->xw = 0.0f;
    mf->yw = 0.0f;
    mf->zw = 0.0f;
    mf->ww = 1.0f;
    mf->xx = x;
    mf->yy = y;
    mf->zz = z;
}

/**
 * Produces a rotation matrix using ZYX Tait-Bryan angles.
 */
void SkinMatrix_SetRotateZYX(MtxF* mf, int16_t x, int16_t y, int16_t z) {
    float cos = { 0 };
    float sinZ = Math_SinS(z);
    float cosZ = Math_CosS(z);
    float xy;
    float sin = { 0 };
    float xz;
    float yy;
    float yz;

    mf->yy = cosZ;
    mf->xy = -sinZ;
    mf->wx = mf->wy = mf->wz = 0;
    mf->xw = mf->yw = mf->zw = 0;
    mf->ww = 1;

    if (y != 0) {
        sin = Math_SinS(y);
        cos = Math_CosS(y);

        mf->xx = cosZ * cos;
        mf->xz = cosZ * sin;

        mf->yx = sinZ * cos;
        mf->yz = sinZ * sin;
        mf->zx = -sin;
        mf->zz = cos;

    } else {
        mf->xx = cosZ;
        xz = sinZ; // required to match
        mf->yx = sinZ;
        mf->zx = mf->xz = mf->yz = 0;
        mf->zz = 1;
    }

    if (x != 0) {
        sin = Math_SinS(x);
        cos = Math_CosS(x);

        xy = mf->xy;
        xz = mf->xz;
        mf->xy = (xy * cos) + (xz * sin);
        mf->xz = (xz * cos) - (xy * sin);

        yz = mf->yz;
        yy = mf->yy;
        mf->yy = (yy * cos) + (yz * sin);
        mf->yz = (yz * cos) - (yy * sin);

        if (cos) {}
        mf->zy = mf->zz * sin;
        mf->zz = mf->zz * cos;
    } else {
        mf->zy = 0;
    }
}

/**
 * Produces a rotation matrix using YXZ Tait-Bryan angles.
 */
void SkinMatrix_SetRotateYXZ(MtxF* mf, int16_t x, int16_t y, int16_t z) {
    float cos = { 0 };
    float sinY = Math_SinS(y);
    float cosY = Math_CosS(y);
    float zx;
    float sin = { 0 };
    float zy;
    float xx;
    float xy;

    mf->xx = cosY;
    mf->zx = -sinY;
    mf->wz = 0;
    mf->wy = 0;
    mf->wx = 0;
    mf->zw = 0;
    mf->yw = 0;
    mf->xw = 0;
    mf->ww = 1;

    if (x != 0) {
        sin = Math_SinS(x);
        cos = Math_CosS(x);

        mf->zz = cosY * cos;
        mf->zy = cosY * sin;

        mf->xz = sinY * cos;
        mf->xy = sinY * sin;
        mf->yz = -sin;
        mf->yy = cos;

    } else {
        mf->zz = cosY;
        xy = sinY; // required to match
        mf->xz = sinY;
        mf->xy = mf->zy = mf->yz = 0;
        mf->yy = 1;
    }

    if (z != 0) {
        sin = Math_SinS(z);
        cos = Math_CosS(z);
        xx = mf->xx;
        xy = mf->xy;
        mf->xx = (xx * cos) + (xy * sin);
        mf->xy = xy * cos - (xx * sin);
        zy = mf->zy;
        zx = mf->zx;
        mf->zx = (zx * cos) + (zy * sin);
        mf->zy = (zy * cos) - (zx * sin);
        if (cos) {}
        mf->yx = mf->yy * sin;
        mf->yy = mf->yy * cos;
    } else {
        mf->yx = 0;
    }
}

/**
 * Produces a matrix which translates a vector by amounts in the x, y and z directions
 */
void SkinMatrix_SetTranslate(MtxF* mf, float x, float y, float z) {
    mf->yx = 0.0f;
    mf->zx = 0.0f;
    mf->wx = 0.0f;
    mf->xy = 0.0f;
    mf->zy = 0.0f;
    mf->wy = 0.0f;
    mf->xz = 0.0f;
    mf->yz = 0.0f;
    mf->wz = 0.0f;
    mf->xx = 1.0f;
    mf->yy = 1.0f;
    mf->zz = 1.0f;
    mf->ww = 1.0f;
    mf->xw = x;
    mf->yw = y;
    mf->zw = z;
}

/**
 * Produces a matrix which scales, then rotates (using ZYX Tait-Bryan angles), then translates.
 */
void SkinMatrix_SetTranslateRotateZYXScale(MtxF* dest, float scaleX, float scaleY, float scaleZ, int16_t rotX, int16_t rotY, int16_t rotZ,
                                           float translateX, float translateY, float translateZ) {
    MtxF mft1;
    MtxF mft2;

    SkinMatrix_SetTranslate(dest, translateX, translateY, translateZ);
    SkinMatrix_SetRotateZYX(&mft1, rotX, rotY, rotZ);
    SkinMatrix_MtxFMtxFMult(dest, &mft1, &mft2);
    SkinMatrix_SetScale(&mft1, scaleX, scaleY, scaleZ);
    SkinMatrix_MtxFMtxFMult(&mft2, &mft1, dest);
}

/**
 * Produces a matrix which scales, then rotates (using YXZ Tait-Bryan angles), then translates.
 */
void SkinMatrix_SetTranslateRotateYXZScale(MtxF* dest, float scaleX, float scaleY, float scaleZ, int16_t rotX, int16_t rotY, int16_t rotZ,
                                           float translateX, float translateY, float translateZ) {
    MtxF mft1;
    MtxF mft2;

    SkinMatrix_SetTranslate(dest, translateX, translateY, translateZ);
    SkinMatrix_SetRotateYXZ(&mft1, rotX, rotY, rotZ);
    SkinMatrix_MtxFMtxFMult(dest, &mft1, &mft2);
    SkinMatrix_SetScale(&mft1, scaleX, scaleY, scaleZ);
    SkinMatrix_MtxFMtxFMult(&mft2, &mft1, dest);
}

/**
 * Produces a matrix which rotates (using ZYX Tait-Bryan angles), then translates.
 */
void SkinMatrix_SetTranslateRotateZYX(MtxF* dest, int16_t rotX, int16_t rotY, int16_t rotZ, float translateX, float translateY,
                                      float translateZ) {
    MtxF rotation;
    MtxF translation;

    SkinMatrix_SetTranslate(&translation, translateX, translateY, translateZ);
    SkinMatrix_SetRotateZYX(&rotation, rotX, rotY, rotZ);
    SkinMatrix_MtxFMtxFMult(&translation, &rotation, dest);
}

void SkinMatrix_Vec3fToVec3s(Vec3f* src, Vec3s* dest) {
    dest->x = src->x;
    dest->y = src->y;
    dest->z = src->z;
}

void SkinMatrix_Vec3sToVec3f(Vec3s* src, Vec3f* dest) {
    dest->x = src->x;
    dest->y = src->y;
    dest->z = src->z;
}

void SkinMatrix_MtxFToMtx(MtxF* src, Mtx* dest) {
    FrameInterpolation_RecordSkinMatrixMtxFToMtx(src, dest);
    guMtxF2L(src, dest);
}

Mtx* SkinMatrix_MtxFToNewMtx(GraphicsContext* gfxCtx, MtxF* src) {
    Mtx* mtx = Graph_Alloc(gfxCtx, sizeof(Mtx));

    if (mtx == NULL) {
        osSyncPrintf("Skin_Matrix_to_Mtx_new() 確保失敗:NULLを返して終了\n", mtx);
        return NULL;
    }
    SkinMatrix_MtxFToMtx(src, mtx);
    return mtx;
}

/**
 * Produces a matrix which rotates by binary angle `angle` around a unit vector (`axisX`,`axisY`,`axisZ`).
 * NB: the rotation axis is assumed to be a unit vector.
 */
void SkinMatrix_SetRotateAxis(MtxF* mf, int16_t angle, float axisX, float axisY, float axisZ) {

    float sinA = Math_SinS(angle);
    float cosA = Math_CosS(angle);

    float xx = axisX * axisX;
    float yy = axisY * axisY;
    float zz = axisZ * axisZ;
    float xy = axisX * axisY;
    float yz = axisY * axisZ;
    float xz = axisX * axisZ;

    mf->xx = (1.0f - xx) * cosA + xx;
    mf->yx = (1.0f - cosA) * xy + axisZ * sinA;
    mf->zx = (1.0f - cosA) * xz - axisY * sinA;
    mf->wx = 0.0f;

    mf->xy = (1.0f - cosA) * xy - axisZ * sinA;
    mf->yy = (1.0f - yy) * cosA + yy;
    mf->zy = (1.0f - cosA) * yz + axisX * sinA;
    mf->wy = 0.0f;

    mf->xz = (1.0f - cosA) * xz + axisY * sinA;
    mf->yz = (1.0f - cosA) * yz - axisX * sinA;
    mf->zz = (1.0f - zz) * cosA + zz;
    mf->wz = 0.0f;

    mf->xw = mf->yw = mf->zw = 0.0f;
    mf->ww = 1.0f;
}

void func_800A8030(MtxF* mf, float* arg1) {

    float n = 2.0f / ((arg1[3] * arg1[3]) + ((arg1[2] * arg1[2]) + ((arg1[1] * arg1[1]) + (arg1[0] * arg1[0]))));
    float xNorm = arg1[0] * n;
    float yNorm = arg1[1] * n;
    float zNorm = arg1[2] * n;

    float wxNorm = arg1[3] * xNorm;
    float wyNorm = arg1[3] * yNorm;
    float wzNorm = arg1[3] * zNorm;
    float xxNorm = arg1[0] * xNorm;
    float xyNorm = arg1[0] * yNorm;
    float xzNorm = arg1[0] * zNorm;
    float yyNorm = arg1[1] * yNorm;
    float yzNorm = arg1[1] * zNorm;
    float zzNorm = arg1[2] * zNorm;

    mf->xx = (1.0f - (yyNorm + zzNorm));
    mf->yx = (xyNorm + wzNorm);
    mf->zx = (xzNorm - wyNorm);
    mf->wx = 0.0f;
    mf->xy = (xyNorm - wzNorm);
    mf->yy = (1.0f - (xxNorm + zzNorm));
    mf->zy = (yzNorm + wxNorm);
    mf->wy = 0.0f;
    mf->xz = (yzNorm + wyNorm);
    mf->yz = (yzNorm - wxNorm);
    mf->zz = (1.0f - (xxNorm + yyNorm));
    mf->wz = 0.0f;
    mf->xw = 0.0f;
    mf->yw = 0.0f;
    mf->ww = 1.0f;
    mf->zw = 0.0f;
}
