#ifndef MATH_H
#define MATH_H

#include <libultraship/libultra.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846f
#endif
#ifndef M_SQRT2
#define M_SQRT2 1.41421356237309504880f
#endif
#ifndef FLT_MAX
#define FLT_MAX 340282346638528859811704183484516925440.0f
#endif
#define SHT_MAX 32767.0f
#define SHT_MINV (1.0f / SHT_MAX)
#define DEGTORAD(x) (x * M_PI / 180.0f)

typedef union {
    struct {
        uint32_t hi;
        uint32_t lo;
    } word;

    double d;
} du;

typedef union {
    uint32_t i;
    float f;
} fu;

extern float __libm_qnan_f;

#endif
