#include "global.h"

// The latest generated random number, used to generate the next number in the sequence.
static uint32_t sRandInt = 1;

// Space to store a value to be re-interpreted as a float.
static uint32_t sRandFloat;

/**
 * Gets the next integer in the sequence of pseudo-random numbers.
 */
uint32_t Rand_Next(void) {
    return sRandInt = (sRandInt * 1664525) + 1013904223;
}

/**
 * Seeds the pseudo-random number generator by providing a starting value.
 */
void Rand_Seed(uint32_t seed) {
    sRandInt = seed;
}

/**
 * Returns a pseudo-random floating-point number between 0.0f and 1.0f, by generating
 * the next integer and masking it to an IEEE-754 compliant floating-point number
 * between 1.0f and 2.0f, returning the result subtract 1.0f.
 */
float Rand_ZeroOne(void) {
    sRandInt = (sRandInt * 1664525) + 1013904223;
    sRandFloat = ((sRandInt >> 9) | 0x3F800000);
    return *((float*)&sRandFloat) - 1.0f;
}

/**
 * Returns a pseudo-random floating-point number between -0.5f and 0.5f by the same
 * manner in which Rand_ZeroOne generates its result.
 */
float Rand_Centered(void) {
    sRandInt = (sRandInt * 1664525) + 1013904223;
    sRandFloat = ((sRandInt >> 9) | 0x3F800000);
    return *((float*)&sRandFloat) - 1.5f;
}

/**
 * Seeds a pseudo-random number at rndNum with a provided seed.
 */
void Rand_Seed_Variable(uint32_t* rndNum, uint32_t seed) {
    *rndNum = seed;
}

/**
 * Generates the next pseudo-random integer from the provided rndNum.
 */
uint32_t Rand_Next_Variable(uint32_t* rndNum) {
    return *rndNum = (*rndNum * 1664525) + 1013904223;
}

/**
 * Generates the next pseudo-random floating-point number between 0.0f and
 * 1.0f from the provided rndNum.
 */
float Rand_ZeroOne_Variable(uint32_t* rndNum) {
    uint32_t next = (*rndNum * 1664525) + 1013904223;

    // clang-format off
    *rndNum = next; sRandFloat = (next >> 9) | 0x3F800000;
    // clang-format on
    return *((float*)&sRandFloat) - 1.0f;
}

/**
 * Generates the next pseudo-random floating-point number between -0.5f and
 * 0.5f from the provided rndNum.
 */
float Rand_Centered_Variable(uint32_t* rndNum) {
    uint32_t next = (*rndNum * 1664525) + 1013904223;

    // clang-format off
    *rndNum = next; sRandFloat = (next >> 9) | 0x3F800000;
    // clang-format on
    return *((float*)&sRandFloat) - 1.5f;
}
