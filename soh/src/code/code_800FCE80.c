#include "global.h"
#include "fp.h"

int32_t gUseAtanContFrac;

float Math_FTanF(float x) {
    float sin = sinf(x);
    float cos = cosf(x);

    return sin / cos;
}

float Math_FFloorF(float x) {
    return floorf(x);
}

float Math_FCeilF(float x) {
    return ceilf(x);
}

float Math_FRoundF(float x) {
    return roundf(x);
}

float Math_FTruncF(float x) {
    return truncf(x);
}

float Math_FNearbyIntF(float x) {
    return nearbyintf(x);
}

/* Arctangent approximation using a Taylor series (one quadrant) */
float Math_FAtanTaylorQF(float x) {
    static const float coeffs[] = {
        -1.0f / 3, +1.0f / 5, -1.0f / 7, +1.0f / 9, -1.0f / 11, +1.0f / 13, -1.0f / 15, +1.0f / 17, 0.0f,
    };

    float poly = x;
    float sq = SQ(x);
    float exp = x * sq;
    const float* c = coeffs;

    while (1) {
        float term = *c++ * exp;
        if (poly + term == poly) {
            break;
        }
        poly = poly + term;
        exp = exp * sq;
    }

    return poly;
}

/* Ditto for two quadrants */
float Math_FAtanTaylorF(float x) {
    float t = { 0 };
    float q = { 0 };

    if (x > 0.0f) {
        t = x;
    } else if (x < 0.0f) {
        t = -x;
    } else if (x == 0.0f) {
        return 0.0f;
    } else {
        return qNaN0x10000;
    }

    if (t <= M_SQRT2 - 1.0f) {
        return Math_FAtanTaylorQF(x);
    }

    if (t >= M_SQRT2 + 1.0f) {
        q = M_PI / 2 - Math_FAtanTaylorQF(1.0f / t);
    } else {
        q = M_PI / 4 - Math_FAtanTaylorQF((1.0f - t) / (1.0f + t));
    }

    if (x > 0.0f) {
        return q;
    } else {
        return -q;
    }
}

/* Arctangent approximation using a continued fraction */
float Math_FAtanContFracF(float x) {
    int32_t sector = { 0 };
    float z = { 0 };
    float conv = { 0 };
    float sq = { 0 };
    int32_t i;

    if (x >= -1.0f && x <= 1.0f) {
        sector = 0;
    } else if (x > 1.0f) {
        sector = 1;
        x = 1.0f / x;
    } else if (x < -1.0f) {
        sector = -1;
        x = 1.0f / x;
    } else {
        return qNaN0x10000;
    }

    sq = SQ(x);
    conv = 0.0f;
    z = 8.0f;
    for (i = 8; i != 0; i--) {
        conv = SQ(z) * sq / (2.0f * z + 1.0f + conv);
        z -= 1.0f;
    }
    conv = x / (1.0f + conv);

    if (sector == 0) {
        return conv;
    } else if (sector > 0) {
        return M_PI / 2 - conv;
    } else {
        return -M_PI / 2 - conv;
    }
}

float Math_FAtanF(float x) {
    if (!gUseAtanContFrac) {
        return Math_FAtanTaylorF(x);
    } else {
        return Math_FAtanContFracF(x);
    }
}

float Math_FAtan2F(float y, float x) {
    if (x == 0.0f) {
        if (y == 0.0f) {
            return 0.0f;
        } else if (y > 0.0f) {
            return M_PI / 2;
        } else if (y < 0.0f) {
            return -M_PI / 2;
        } else {
            return qNaN0x10000;
        }
    } else if (x >= 0.0f) {
        return Math_FAtanF(y / x);
    } else if (y < 0.0f) {
        return Math_FAtanF(y / x) - M_PI;
    } else {
        return M_PI - Math_FAtanF(-(y / x));
    }
}

float Math_FAsinF(float x) {
    return Math_FAtan2F(x, sqrtf(1.0f - SQ(x)));
}

float Math_FAcosF(float x) {
    return M_PI / 2 - Math_FAsinF(x);
}
