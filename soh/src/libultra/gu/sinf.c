#include "global.h"
#include <libultraship/libultra.h>

static const du P[] = {
    { 0x3FF00000, 0x00000000 }, { 0xBFC55554, 0xBC83656D }, { 0x3F8110ED, 0x3804C2A0 },
    { 0xBF29F6FF, 0xEEA56814 }, { 0x3EC5DBDF, 0x0E314BFE },
};

static const du rpi = { 0x3FD45F30, 0x6DC9C883 };

static const du pihi = { 0x400921FB, 0x50000000 };

static const du pilo = { 0x3E6110B4, 0x611A6263 };

static const fu zero = { 0x00000000 };

float sinf(float x) {
    double dx = { 0 };
    double xSq = { 0 };
    double polyApprox = { 0 };
    double result = { 0 };
    int32_t ix = *(int32_t*)&x;
    int32_t xpt = (ix >> 22);

    xpt &= 0x1FF;

    if (xpt < 0xFF) {
        dx = x;

        if (xpt >= 0xE6) {
            xSq = SQ(dx);
            polyApprox = ((P[4].d * xSq + P[3].d) * xSq + P[2].d) * xSq + P[1].d;
            result = dx + (dx * xSq) * polyApprox;
            return (float)result;
        }
        return x;
    }

    if (xpt < 0x136) {
        dx = x;
        double dn = dx * rpi.d;
        int32_t n = ROUND(dn);
        dn = n;

        dx -= dn * pihi.d;
        dx -= dn * pilo.d;
        xSq = SQ(dx);

        polyApprox = ((P[4].d * xSq + P[3].d) * xSq + P[2].d) * xSq + P[1].d;
        result = dx + (dx * xSq) * polyApprox;

        if (!(n & 1)) {
            return (float)result;
        }
        return -(float)result;
    }

    if (x != x) {
        return __libm_qnan_f;
    }
    return zero.f;
}
