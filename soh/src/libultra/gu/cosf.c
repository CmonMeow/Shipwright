#include <libultraship/libultra.h>
#include "global.h"

static const du P[] = {
    { 0x3FF00000, 0x00000000 }, { 0xBFC55554, 0xBC83656D }, { 0x3F8110ED, 0x3804C2A0 },
    { 0xBF29F6FF, 0xEEA56814 }, { 0x3EC5DBDF, 0x0E314BFE },
};

static const du rpi = { 0x3FD45F30, 0x6DC9C883 };

static const du pihi = { 0x400921FB, 0x50000000 };

static const du pilo = { 0x3E6110B4, 0x611A6263 };

static const fu zero = { 0x00000000 };

float cosf(float x) {
    int32_t ix = *(int32_t*)&x;
    int32_t xpt = (ix >> 22);

    xpt &= 0x1FF;

    if (xpt < 0x136) {
        float absx = (x > 0) ? x : -x;
        double dx = absx;

        double dn = dx * rpi.d + 0.5;
        int32_t n = ROUND(dn);
        dn = n;

        dn -= 0.5;

        dx -= dn * pihi.d;
        dx -= dn * pilo.d;

        double xSq = SQ(dx);

        double polyApprox = ((P[4].d * xSq + P[3].d) * xSq + P[2].d) * xSq + P[1].d;

        double result = dx + (dx * xSq) * polyApprox;

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
