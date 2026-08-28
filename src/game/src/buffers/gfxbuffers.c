#include "z64.h"

// 0x18000 bytes
uint64_t gGfxSPTaskOutputBuffer[0x3000];

// 0xC00 bytes
uint8_t gGfxSPTaskYieldBuffer[OS_YIELD_DATA_SIZE];

// 0x400 bytes
uint8_t gGfxSPTaskStack[0x400];

// 0x12410 bytes each; 0x24820 bytes total
GfxPool gGfxPools[2];
