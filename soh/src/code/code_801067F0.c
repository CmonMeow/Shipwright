#include "global.h"

// fmodf?
float func_801067F0(float arg0, float arg1) {
    int32_t sp4 = { 0 };

    if (arg1 == 0.0f) {
        return 0.0f;
    }
    sp4 = arg0 / arg1;
    return arg0 - (sp4 * arg1);
}
