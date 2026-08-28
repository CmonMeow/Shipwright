#include "global.h"
#include <math.h>

float Math_CosS(int16_t angle) {
    return coss(angle) * SHT_MINV;
}

float Math_SinS(int16_t angle) {
    return sins(angle) * SHT_MINV;
}

float Math_AccurateCosS(int16_t angle) {
    return cosf(DEG_TO_RAD((float)(angle & 0xFFFC) / SHT_MAX) * 180.0f);
}

float Math_AccurateSinS(int16_t angle) {
    return sinf(DEG_TO_RAD((float)(angle & 0xFFFC) / SHT_MAX) * 180.0f);
}

/**
 * Changes pValue by step (scaled by the update rate) towards target, setting it equal when the target is reached.
 * Returns true when target is reached, false otherwise.
 */
int32_t Math_ScaledStepToS(int16_t* pValue, int16_t target, int16_t step) {
    if (step != 0) {
        float updateScale = R_UPDATE_RATE * 0.5f;

        if ((int16_t)(*pValue - target) > 0) {
            step = -step;
        }

        *pValue += (int16_t)(step * updateScale);

        if (((int16_t)(*pValue - target) * step) >= 0) {
            *pValue = target;
            return true;
        }
    } else if (target == *pValue) {
        return true;
    }

    return false;
}

/**
 * Changes pValue by step towards target, setting it equal when the target is reached.
 * Returns true when target is reached, false otherwise.
 */
int32_t Math_StepToS(int16_t* pValue, int16_t target, int16_t step) {
    if (step != 0) {
        if (target < *pValue) {
            step = -step;
        }

        *pValue += step;

        if (((*pValue - target) * step) >= 0) {
            *pValue = target;
            return true;
        }
    } else if (target == *pValue) {
        return true;
    }

    return false;
}

/**
 * Changes pValue by step towards target, setting it equal when the target is reached.
 * Returns true when target is reached, false otherwise.
 */
int32_t Math_StepToF(float* pValue, float target, float step) {
    if (step != 0.0f) {
        if (target < *pValue) {
            step = -step;
        }

        *pValue += step;

        if (((*pValue - target) * step) >= 0) {
            *pValue = target;
            return true;
        }
    } else if (target == *pValue) {
        return true;
    }

    return false;
}

/**
 *  Changes pValue by step. If pvalue reaches limit angle or its opposite, sets it equal to limit angle.
 * Returns true when limit angle or its opposite is reached, false otherwise.
 */
int32_t Math_StepUntilAngleS(int16_t* pValue, int16_t limit, int16_t step) {
    int16_t orig = *pValue;

    *pValue += step;

    if (((int16_t)(*pValue - limit) * (int16_t)(orig - limit)) <= 0) {
        *pValue = limit;
        return true;
    }

    return false;
}

/**
 * Changes pValue by step. If pvalue reaches limit, sets it equal to limit.
 * Returns true when limit is reached, false otherwise.
 */
int32_t Math_StepUntilS(int16_t* pValue, int16_t limit, int16_t step) {
    int16_t orig = *pValue;

    *pValue += step;

    if (((*pValue - limit) * (orig - limit)) <= 0) {
        *pValue = limit;
        return true;
    }

    return false;
}

/**
 * Changes pValue by step towards target angle, setting it equal when the target is reached.
 * Returns true when target is reached, false otherwise.
 */
int32_t Math_StepToAngleS(int16_t* pValue, int16_t target, int16_t step) {
    int32_t diff = target - *pValue;

    if (diff < 0) {
        step = -step;
    }

    if (diff >= 0x8000) {
        step = -step;
        diff = -0xFFFF - -diff;
    } else if (diff <= -0x8000) {
        diff += 0xFFFF;
        step = -step;
    }

    if (step != 0) {
        *pValue += step;

        if ((diff * step) <= 0) {
            *pValue = target;
            return true;
        }
    } else if (target == *pValue) {
        return true;
    }

    return false;
}

/**
 * Changes pValue by step. If pvalue reaches limit, sets it equal to limit.
 * Returns true when limit is reached, false otherwise.
 */
int32_t Math_StepUntilF(float* pValue, float limit, float step) {
    float orig = *pValue;

    *pValue += step;

    if (((*pValue - limit) * (orig - limit)) <= 0) {
        *pValue = limit;
        return true;
    }

    return false;
}

/**
 * Changes pValue toward target by incrStep if pValue is smaller and by decrStep if it is greater, setting it equal when
 * target is reached. Returns true when target is reached, false otherwise.
 */
int32_t Math_AsymStepToF(float* pValue, float target, float incrStep, float decrStep) {
    float step = (target >= *pValue) ? incrStep : decrStep;

    if (step != 0.0f) {
        if (target < *pValue) {
            step = -step;
        }

        *pValue += step;

        if (((*pValue - target) * step) >= 0) {
            *pValue = target;
            return 1;
        }
    } else if (target == *pValue) {
        return 1;
    }

    return 0;
}

void func_80077D10(float* arg0, int16_t* arg1, Input* input) {
    float relX = input->rel.stick_x;
    float relY = input->rel.stick_y;

    *arg0 = sqrtf(SQ(relX) + SQ(relY));
    *arg0 = (60.0f < *arg0) ? 60.0f : *arg0;

    *arg1 = RADF_TO_BINANG(atan2f(-relX, relY));
}

int16_t Rand_S16Offset(int16_t base, int16_t range) {
    return (int16_t)(Rand_ZeroOne() * range) + base;
}

int16_t Rand_S16OffsetStride(int16_t base, int16_t stride, int16_t range) {
    return (int16_t)(Rand_ZeroOne() * range) * stride + base;
}

void Math_Vec3f_Copy(Vec3f* dest, Vec3f* src) {
    dest->x = src->x;
    dest->y = src->y;
    dest->z = src->z;
}

void Math_Vec3s_ToVec3f(Vec3f* dest, Vec3s* src) {
    dest->x = src->x;
    dest->y = src->y;
    dest->z = src->z;
}

void Math_Vec3f_Sum(Vec3f* a, Vec3f* b, Vec3f* dest) {
    dest->x = a->x + b->x;
    dest->y = a->y + b->y;
    dest->z = a->z + b->z;
}

void Math_Vec3f_Diff(Vec3f* a, Vec3f* b, Vec3f* dest) {
    dest->x = a->x - b->x;
    dest->y = a->y - b->y;
    dest->z = a->z - b->z;
}

void Math_Vec3s_DiffToVec3f(Vec3f* dest, Vec3s* a, Vec3s* b) {
    dest->x = a->x - b->x;
    dest->y = a->y - b->y;
    dest->z = a->z - b->z;
}

void Math_Vec3f_Scale(Vec3f* vec, float scaleF) {
    vec->x *= scaleF;
    vec->y *= scaleF;
    vec->z *= scaleF;
}

float Math_Vec3f_DistXYZ(Vec3f* a, Vec3f* b) {
    float dx = b->x - a->x;
    float dy = b->y - a->y;
    float dz = b->z - a->z;

    return sqrtf(SQ(dx) + SQ(dy) + SQ(dz));
}

float Math_Vec3f_DistXYZAndStoreDiff(Vec3f* a, Vec3f* b, Vec3f* dest) {
    dest->x = b->x - a->x;
    dest->y = b->y - a->y;
    dest->z = b->z - a->z;

    return sqrtf(SQ(dest->x) + SQ(dest->y) + SQ(dest->z));
}

float Math_Vec3f_DistXZ(Vec3f* a, Vec3f* b) {
    float dx = b->x - a->x;
    float dz = b->z - a->z;

    return sqrtf(SQ(dx) + SQ(dz));
}

float Math_Vec3f_DiffY(Vec3f* a, Vec3f* b) {
    return b->y - a->y;
}

int16_t Math_Vec3f_Yaw(Vec3f* a, Vec3f* b) {
    float dx = b->x - a->x;
    float dz = b->z - a->z;

    return RADF_TO_BINANG(atan2f(dx, dz));
}

int16_t Math_Vec3f_Pitch(Vec3f* a, Vec3f* b) {
    return RADF_TO_BINANG(atan2f(a->y - b->y, Math_Vec3f_DistXZ(a, b)));
}

void IChain_Apply_u8(uint8_t* ptr, InitChainEntry* ichain);
void IChain_Apply_s8(uint8_t* ptr, InitChainEntry* ichain);
void IChain_Apply_u16(uint8_t* ptr, InitChainEntry* ichain);
void IChain_Apply_s16(uint8_t* ptr, InitChainEntry* ichain);
void IChain_Apply_u32(uint8_t* ptr, InitChainEntry* ichain);
void IChain_Apply_s32(uint8_t* ptr, InitChainEntry* ichain);
void IChain_Apply_f32(uint8_t* ptr, InitChainEntry* ichain);
void IChain_Apply_f32div1000(uint8_t* ptr, InitChainEntry* ichain);
void IChain_Apply_Vec3f(uint8_t* ptr, InitChainEntry* ichain);
void IChain_Apply_Vec3fdiv1000(uint8_t* ptr, InitChainEntry* ichain);
void IChain_Apply_Vec3s(uint8_t* ptr, InitChainEntry* ichain);

void (*sInitChainHandlers[])(uint8_t* ptr, InitChainEntry* ichain) = {
    IChain_Apply_u8,    IChain_Apply_s8,           IChain_Apply_u16,   IChain_Apply_s16,
    IChain_Apply_u32,   IChain_Apply_s32,          IChain_Apply_f32,   IChain_Apply_f32div1000,
    IChain_Apply_Vec3f, IChain_Apply_Vec3fdiv1000, IChain_Apply_Vec3s,
};

void Actor_ProcessInitChain(Actor* actor, InitChainEntry* ichain) {
    do {
        sInitChainHandlers[ichain->type]((uint8_t*)actor, ichain);
    } while ((ichain++)->cont);
}

void IChain_Apply_u8(uint8_t* ptr, InitChainEntry* ichain) {
    *(uint8_t*)(ptr + ichain->offset) = ichain->value;
}

void IChain_Apply_s8(uint8_t* ptr, InitChainEntry* ichain) {
    *(int8_t*)(ptr + ichain->offset) = ichain->value;
}

void IChain_Apply_u16(uint8_t* ptr, InitChainEntry* ichain) {
    *(uint16_t*)(ptr + ichain->offset) = ichain->value;
}

void IChain_Apply_s16(uint8_t* ptr, InitChainEntry* ichain) {
    *(int16_t*)(ptr + ichain->offset) = ichain->value;
}

void IChain_Apply_u32(uint8_t* ptr, InitChainEntry* ichain) {
    *(uint32_t*)(ptr + ichain->offset) = ichain->value;
}

void IChain_Apply_s32(uint8_t* ptr, InitChainEntry* ichain) {
    *(int32_t*)(ptr + ichain->offset) = ichain->value;
}

void IChain_Apply_f32(uint8_t* ptr, InitChainEntry* ichain) {
    *(float*)(ptr + ichain->offset) = ichain->value;
}

void IChain_Apply_f32div1000(uint8_t* ptr, InitChainEntry* ichain) {
    *(float*)(ptr + ichain->offset) = ichain->value / 1000.0f;
}

void IChain_Apply_Vec3f(uint8_t* ptr, InitChainEntry* ichain) {
    Vec3f* vec = (Vec3f*)(ptr + ichain->offset);
    float val = ichain->value;

    vec->z = val;
    vec->y = val;
    vec->x = val;
}

void IChain_Apply_Vec3fdiv1000(uint8_t* ptr, InitChainEntry* ichain) {
    Vec3f* vec = (Vec3f*)(ptr + ichain->offset);

    osSyncPrintf("pp=%x data=%f\n", vec, ichain->value / 1000.0f);
    float val = ichain->value / 1000.0f;

    vec->z = val;
    vec->y = val;
    vec->x = val;
}

void IChain_Apply_Vec3s(uint8_t* ptr, InitChainEntry* ichain) {
    Vec3s* vec = (Vec3s*)(ptr + ichain->offset);
    int16_t val = ichain->value;

    vec->z = val;
    vec->y = val;
    vec->x = val;
}

/**
 * Changes pValue by step towards target. If this step is more than fraction of the remaining distance, step by that
 * instead, with a minimum step of minStep. Returns remaining distance to target.
 */
float Math_SmoothStepToF(float* pValue, float target, float fraction, float step, float minStep) {
    if (*pValue != target) {
        float stepSize = (target - *pValue) * fraction;

        if ((stepSize >= minStep) || (stepSize <= -minStep)) {
            if (stepSize > step) {
                stepSize = step;
            }

            if (stepSize < -step) {
                stepSize = -step;
            }

            *pValue += stepSize;
        } else {
            if (stepSize < minStep) {
                *pValue += minStep;
                stepSize = minStep;

                if (target < *pValue) {
                    *pValue = target;
                }
            }
            if (stepSize > -minStep) {
                *pValue += -minStep;

                if (*pValue < target) {
                    *pValue = target;
                }
            }
        }
    }

    return fabsf(target - *pValue);
}

/**
 * Changes pValue by step towards target. If step is more than fraction of the remaining distance, step by that instead.
 */
void Math_ApproachF(float* pValue, float target, float fraction, float step) {
    if (*pValue != target) {
        float stepSize = (target - *pValue) * fraction;

        if (stepSize > step) {
            stepSize = step;
        } else if (stepSize < -step) {
            stepSize = -step;
        }

        *pValue += stepSize;
    }
}

/**
 * Changes pValue by step towards zero. If step is more than fraction of the remaining distance, step by that instead.
 */
void Math_ApproachZeroF(float* pValue, float fraction, float step) {
    float stepSize = *pValue * fraction;

    if (stepSize > step) {
        stepSize = step;
    } else if (stepSize < -step) {
        stepSize = -step;
    }

    *pValue -= stepSize;
}

/**
 * Changes pValue by step towards target angle in degrees. If this step is more than fraction of the remaining distance,
 * step by that instead, with a minimum step of minStep. Returns the value of the step taken.
 */
float Math_SmoothStepToDegF(float* pValue, float target, float fraction, float step, float minStep) {
    float stepSize = 0.0f;
    float diff = target - *pValue;

    if (*pValue != target) {
        if (diff > 180.0f) {
            diff = -(360.0f - diff);
        } else if (diff < -180.0f) {
            diff = 360.0f + diff;
        }

        stepSize = diff * fraction;

        if ((stepSize >= minStep) || (stepSize <= -minStep)) {
            if (stepSize > step) {
                stepSize = step;
            }

            if (stepSize < -step) {
                stepSize = -step;
            }

            *pValue += stepSize;
        } else {
            if (stepSize < minStep) {
                stepSize = minStep;
                *pValue += stepSize;
                if (*pValue > target) {
                    *pValue = target;
                }
            }
            if (stepSize > -minStep) {
                stepSize = -minStep;
                *pValue += stepSize;
                if (*pValue < target) {
                    *pValue = target;
                }
            }
        }
    }

    if (*pValue >= 360.0f) {
        *pValue -= 360.0f;
    }

    if (*pValue < 0.0f) {
        *pValue += 360.0f;
    }

    return stepSize;
}

/**
 * Changes pValue by step towards target. If this step is more than 1/scale of the remaining distance, step by that
 * instead, with a minimum step of minStep. Returns remaining distance to target.
 */
int16_t Math_SmoothStepToS(int16_t* pValue, int16_t target, int16_t scale, int16_t step, int16_t minStep) {
    int16_t stepSize = 0;
    int16_t diff = target - *pValue;

    if (*pValue != target) {
        stepSize = diff / scale;

        if ((stepSize > minStep) || (stepSize < -minStep)) {
            if (stepSize > step) {
                stepSize = step;
            }

            if (stepSize < -step) {
                stepSize = -step;
            }

            *pValue += stepSize;
        } else {
            if (diff >= 0) {
                *pValue += minStep;

                if ((int16_t)(target - *pValue) <= 0) {
                    *pValue = target;
                }
            } else {
                *pValue -= minStep;

                if ((int16_t)(target - *pValue) >= 0) {
                    *pValue = target;
                }
            }
        }
    }

    return diff;
}

/**
 * Changes pValue by step towards target. If step is more than 1/scale of the remaining distance, step by that instead.
 */
void Math_ApproachS(int16_t* pValue, int16_t target, int16_t scale, int16_t maxStep) {
    int16_t diff = target - *pValue;

    diff /= scale;

    if (diff > maxStep) {
        *pValue += maxStep;
    } else if (diff < -maxStep) {
        *pValue -= maxStep;
    } else {
        *pValue += diff;
    }
}

void Color_RGBA8_Copy(Color_RGBA8* dst, Color_RGBA8* src) {
    dst->r = src->r;
    dst->g = src->g;
    dst->b = src->b;
    dst->a = src->a;
}

void Sfx_PlaySfxCentered(uint16_t sfxId) {
    Audio_PlaySoundGeneral(sfxId, &gSfxDefaultPos, 4, &gSfxDefaultFreqAndVolScale, &gSfxDefaultFreqAndVolScale,
                           &gSfxDefaultReverb);
}

void Sfx_PlaySfxCentered2(uint16_t sfxId) {
    Audio_PlaySoundGeneral(sfxId, &gSfxDefaultPos, 4, &gSfxDefaultFreqAndVolScale, &gSfxDefaultFreqAndVolScale,
                           &gSfxDefaultReverb);
}

void Sfx_PlaySfxAtPos(Vec3f* arg0, uint16_t sfxId) {
    Audio_PlaySoundGeneral(sfxId, arg0, 4, &gSfxDefaultFreqAndVolScale, &gSfxDefaultFreqAndVolScale,
                           &gSfxDefaultReverb);
}
