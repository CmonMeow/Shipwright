#include "global.h"

// The code in this file is very similar to a spline system used in Super Mario 64 for cutscene camera movement

void func_800BB0A0(float u, Vec3f* pos, float* roll, float* viewAngle, float* point0, float* point1, float* point2, float* point3) {
    float coeff[4];

    u = CLAMP_MAX(u, 1.0f);

    coeff[0] = (1.0f - u) * (1.0f - u) * (1.0f - u) / 6.0f;
    coeff[1] = u * u * u / 2.0f - u * u + 2.0f / 3.0f;
    coeff[2] = -u * u * u / 2.0f + u * u / 2.0f + u / 2.0f + 1.0f / 6.0f;
    coeff[3] = u * u * u / 6.0f;

    pos->x = (coeff[0] * point0[0]) + (coeff[1] * point1[0]) + (coeff[2] * point2[0]) + (coeff[3] * point3[0]);
    pos->y = (coeff[0] * point0[1]) + (coeff[1] * point1[1]) + (coeff[2] * point2[1]) + (coeff[3] * point3[1]);
    pos->z = (coeff[0] * point0[2]) + (coeff[1] * point1[2]) + (coeff[2] * point2[2]) + (coeff[3] * point3[2]);
    *roll = (coeff[0] * point0[3]) + (coeff[1] * point1[3]) + (coeff[2] * point2[3]) + (coeff[3] * point3[3]);
    *viewAngle = (coeff[0] * point0[4]) + (coeff[1] * point1[4]) + (coeff[2] * point2[4]) + (coeff[3] * point3[4]);
}

int32_t func_800BB2B4(Vec3f* pos, float* roll, float* fov, CutsceneCameraPoint* point, int16_t* keyFrame, float* curFrame) {
    int32_t ret = false;
    float pointData[4][5];
    int32_t i;
    float progress = *curFrame;
    int32_t key = *keyFrame;
    float speed1 = 0.0f;
    float speed2 = 0.0f;
    float advance = { 0 };

    if (key < 0) {
        progress = 0.0f;
    }

    if ((point[key].continueFlag == -1) || (point[key + 1].continueFlag == -1) || (point[key + 2].continueFlag == -1)) {
        return true;
    }

    for (i = 0; i < 4; i++) {
        pointData[i][0] = point[key + i].pos.x;
        pointData[i][1] = point[key + i].pos.y;
        pointData[i][2] = point[key + i].pos.z;
        pointData[i][3] = point[key + i].cameraRoll;
        pointData[i][4] = point[key + i].viewAngle;
    }

    func_800BB0A0(progress, pos, roll, fov, pointData[0], pointData[1], pointData[2], pointData[3]);

    if (point[*keyFrame + 1].nextPointFrame != 0) {
        speed1 = 1.0f / point[*keyFrame + 1].nextPointFrame;
    }

    if (point[*keyFrame + 2].nextPointFrame != 0) {
        speed2 = 1.0f / point[*keyFrame + 2].nextPointFrame;
    }
    advance = (*curFrame * (speed2 - speed1)) + speed1;
    if (advance < 0.0f) {
        advance = 0;
    }
    *curFrame += advance;
    if (*curFrame >= 1) {
        if (point[++*keyFrame + 3].continueFlag == -1) {
            *keyFrame = 0;
            ret = true;
        }
        *curFrame -= 1;
    }

    return ret;
}
