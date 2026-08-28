#include "global.h"

void Flags_UnsetAllEnv(PlayState* play) {
    uint8_t i;

    for (i = 0; i < 20; i++) {
        play->envFlags[i] = 0;
    }
}

void Flags_SetEnv(PlayState* play, int16_t flag) {
    int16_t index = flag / 16;
    int16_t bit = flag % 16;
    int16_t mask = 1 << bit;

    play->envFlags[index] |= mask;
}

void Flags_UnsetEnv(PlayState* play, int16_t flag) {
    int16_t index = flag / 16;
    int16_t bit = flag % 16;
    int16_t mask = (1 << bit) ^ 0xFFFF;

    play->envFlags[index] &= mask;
}

int32_t Flags_GetEnv(PlayState* play, int16_t flag) {
    int16_t index = flag / 16;
    int16_t bit = flag % 16;
    int16_t mask = 1 << bit;

    return (play->envFlags[index] & mask) != 0;
}
