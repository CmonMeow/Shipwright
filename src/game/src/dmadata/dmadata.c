#include "z64dma.h"

// Linker symbol declarations (used in the table below)
#define DEFINE_DMA_ENTRY(name)            \
    extern uint8_t _##name##SegmentRomStart[]; \
    extern uint8_t _##name##SegmentRomEnd[];

#include "tables/dmadata_table.h"

#undef DEFINE_DMA_ENTRY

// dmadata Table definition
#define DEFINE_DMA_ENTRY(name) \
    { (uintptr_t)_##name##SegmentRomStart, (uintptr_t)_##name##SegmentRomEnd, (uintptr_t)_##name##SegmentRomStart, 0 },

DmaEntry gDmaDataTable[] = {
#include "tables/dmadata_table.h"
    { 0 },
};

#undef DEFINE_DMA_ENTRY

// Additional padding?
uint8_t sDmaDataPadding[0xF0] = { 0 };
