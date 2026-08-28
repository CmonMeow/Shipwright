#include "global.h"

#ifndef __GNUC__
#define __builtin_sqrtf sqrtf
#endif

float sqrtf(float f) {
    return __builtin_sqrtf(f);
}
