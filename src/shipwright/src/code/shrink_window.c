#include "global.h"

int32_t D_8012CED0 = 0;

int32_t sShrinkWindowVal = 0;
int32_t sShrinkWindowCurrentVal = 0;

void ShrinkWindow_SetVal(int32_t value) {
    if (HREG(80) == 0x13 && HREG(81) == 1) {
        osSyncPrintf("shrink_window_setval(%d)\n", value);
    }
    sShrinkWindowVal = value;
}

uint32_t ShrinkWindow_GetVal(void) {
    return sShrinkWindowVal;
}

void ShrinkWindow_SetCurrentVal(int32_t currentVal) {
    if (HREG(80) == 0x13 && HREG(81) == 1) {
        osSyncPrintf("shrink_window_setnowval(%d)\n", currentVal);
    }
    sShrinkWindowCurrentVal = currentVal;
}

uint32_t ShrinkWindow_GetCurrentVal(void) {
    return sShrinkWindowCurrentVal;
}

void ShrinkWindow_Init(void) {
    if (HREG(80) == 0x13 && HREG(81) == 1) {
        osSyncPrintf("shrink_window_init()\n");
    }
    D_8012CED0 = 0;
    sShrinkWindowVal = 0;
    sShrinkWindowCurrentVal = 0;
}

void ShrinkWindow_Destroy(void) {
    if (HREG(80) == 0x13 && HREG(81) == 1) {
        osSyncPrintf("shrink_window_cleanup()\n");
    }
    sShrinkWindowCurrentVal = 0;
}

void ShrinkWindow_Update(int32_t updateRate) {
    int32_t off = { 0 };

    if (updateRate == 3) {
        off = 10;
    } else {
        off = 30 / updateRate;
    }

    if (sShrinkWindowCurrentVal < sShrinkWindowVal) {
        if (D_8012CED0 != 1) {
            D_8012CED0 = 1;
        }

        if (sShrinkWindowCurrentVal + off < sShrinkWindowVal) {
            sShrinkWindowCurrentVal += off;
        } else {
            sShrinkWindowCurrentVal = sShrinkWindowVal;
        }
    } else if (sShrinkWindowVal < sShrinkWindowCurrentVal) {
        if (D_8012CED0 != 2) {
            D_8012CED0 = 2;
        }

        if (sShrinkWindowVal < sShrinkWindowCurrentVal - off) {
            sShrinkWindowCurrentVal -= off;
        } else {
            sShrinkWindowCurrentVal = sShrinkWindowVal;
        }
    } else {
        D_8012CED0 = 0;
    }

    if (HREG(80) == 0x13) {
        if (HREG(94) != 0x13) {
            HREG(94) = 0x13;
            HREG(81) = 0;
            HREG(82) = 0;
            HREG(83) = 0;
            HREG(84) = 0;
            HREG(85) = 0;
            HREG(86) = 0;
            HREG(87) = 0;
            HREG(88) = 0;
            HREG(89) = 0;
        }
        HREG(83) = D_8012CED0;
        HREG(84) = sShrinkWindowCurrentVal;
        HREG(85) = sShrinkWindowVal;
        HREG(86) = off;
    }
}
