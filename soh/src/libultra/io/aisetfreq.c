#include "global.h"

int32_t osAiSetFrequency(uint32_t frequency) {
    uint8_t bitrate = { 0 };
    float dacRateF = ((float)osViClock / frequency) + 0.5f;
    uint32_t dacRate = dacRateF;

    if (dacRate < 132) {
        return -1;
    }

    bitrate = (dacRate / 66);
    if (bitrate > 16) {
        bitrate = 16;
    }

    HW_REG(AI_DACRATE_REG, uint32_t) = dacRate - 1;
    HW_REG(AI_BITRATE_REG, uint32_t) = bitrate - 1;
    return osViClock / (int32_t)dacRate;
}
