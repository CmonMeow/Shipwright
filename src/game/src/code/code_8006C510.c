#include "global.h"

float func_8006C510(float arg0, float arg1, float arg2, float arg3, float arg4, float arg5) {
    float sq = SQ(arg0);
    float cube = sq * arg0;

    return (((cube + cube) - sq * 3.0f) + 1.0f) * arg2 + (sq * 3.0f - (cube + cube)) * arg3 +
           ((cube - (sq + sq)) + arg0) * arg4 * arg1 + (cube - sq) * arg5 * arg1;
}

float func_8006C5A8(float target, TransformData* transData, int32_t refIdx) {
    int32_t i;

    if (target <= transData->unk_02) {
        return transData->unk_08;
    }
    if (target >= transData[refIdx - 1].unk_02) {
        return transData[refIdx - 1].unk_08;
    }

    for (i = 0;; i++) {
        int32_t j = i + 1;
        if (transData[j].unk_02 > target) {
            if (transData[i].unk_00 & 1) {
                return transData[i].unk_08;
            } else if (transData[i].unk_00 & 2) {
                return transData[i].unk_08 +
                       ((target - (float)transData[i].unk_02) / ((float)transData[j].unk_02 - (float)transData[i].unk_02)) *
                           (transData[j].unk_08 - transData[i].unk_08);
            } else {
                float diff = (float)transData[j].unk_02 - (float)transData[i].unk_02;
                return func_8006C510((target - transData[i].unk_02) / ((float)transData[j].unk_02 - transData[i].unk_02),
                                     diff * (1.0f / 30.0f), transData[i].unk_08, transData[j].unk_08,
                                     transData[i].unk_06, transData[j].unk_04);
            }
        }
    }
}
