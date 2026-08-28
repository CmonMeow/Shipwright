#include "global.h"

float sFactorialTbl[] = { 1.0f,    1.0f,     2.0f,      6.0f,       24.0f,       120.0f,      720.0f,
                        5040.0f, 40320.0f, 362880.0f, 3628800.0f, 39916800.0f, 479001600.0f };

float Math_FactorialF(float n) {
    float ret = 1.0f;
    int32_t i;

    for (i = n; i > 1; i--) {
        ret *= i;
    }
    return ret;
}

float Math_Factorial(int32_t n) {
    float ret = { 0 };
    int32_t i;

    if ((uint32_t)n > 12U) {
        ret = sFactorialTbl[12];
        for (i = 13; i <= n; i++) {
            ret *= i;
        }
    } else {
        ret = sFactorialTbl[n];
    }
    return ret;
}

float Math_PowF(float base, int32_t exp) {
    float ret = 1.0f;

    while (exp > 0) {
        exp--;
        ret *= base;
    }
    return ret;
}

float Math_SinF(float angle) {
    return sins((int16_t)(angle * (32767.0f / M_PI))) * SHT_MINV;
}

float Math_CosF(float angle) {
    return coss((int16_t)(angle * (32767.0f / M_PI))) * SHT_MINV;
}
