#include "global.h"

void __osViSwapContext(void) {
    register OSViMode* viMode;
    register OSViContext* viNext;
    uint32_t hStart = { 0 };
    uint32_t vstart = { 0 };
    register uint32_t s2;

    uint32_t field = 0;
    viNext = __osViNext;
    viMode = viNext->modep;
    field = HW_REG(VI_V_CURRENT_LINE_REG, uint32_t) & 1;
    s2 = osVirtualToPhysical(viNext->buffer);
    uint32_t origin = (viMode->fldRegs[field].origin) + s2;
    if (viNext->state & 2) {
        viNext->x.scale |= viMode->comRegs.xScale & ~0xFFF;
    } else {
        viNext->x.scale = viMode->comRegs.xScale;
    }
    if (viNext->state & 4) {
        uint32_t sp34 = (uint32_t)(viMode->fldRegs[field].yScale & 0xFFF);
        viNext->y.scale = viNext->y.factor * sp34;
        viNext->y.scale |= viMode->fldRegs[field].yScale & ~0xFFF;
    } else {
        viNext->y.scale = viMode->fldRegs[field].yScale;
    }

    vstart = (viMode->fldRegs[field].vStart - (__additional_scanline << 0x10)) + __additional_scanline;
    hStart = viMode->comRegs.hStart;

    if (viNext->state & 0x20) {
        hStart = 0;
    }
    if (viNext->state & 0x40) {
        viNext->y.scale = 0;
        origin = osVirtualToPhysical(viNext->buffer);
    }
    if (viNext->state & 0x80) {
        viNext->y.scale = (viNext->y.offset << 0x10) & 0x3FF0000;
        origin = osVirtualToPhysical(viNext->buffer);
    }
    HW_REG(VI_ORIGIN_REG, uint32_t) = origin;
    HW_REG(VI_WIDTH_REG, uint32_t) = viMode->comRegs.width;
    HW_REG(VI_BURST_REG, uint32_t) = viMode->comRegs.burst;
    HW_REG(VI_V_SYNC_REG, uint32_t) = viMode->comRegs.vSync;
    HW_REG(VI_H_SYNC_REG, uint32_t) = viMode->comRegs.hSync;
    HW_REG(VI_LEAP_REG, uint32_t) = viMode->comRegs.leap;
    HW_REG(VI_H_START_REG, uint32_t) = hStart;
    HW_REG(VI_V_START_REG, uint32_t) = vstart;
    HW_REG(VI_V_BURST_REG, uint32_t) = viMode->fldRegs[field].vBurst;
    HW_REG(VI_INTR_REG, uint32_t) = viMode->fldRegs[field].vIntr;
    HW_REG(VI_X_SCALE_REG, uint32_t) = viNext->x.scale;
    HW_REG(VI_Y_SCALE_REG, uint32_t) = viNext->y.scale;
    HW_REG(VI_CONTROL_REG, uint32_t) = viNext->features;
    __osViNext = __osViCurr;
    __osViCurr = viNext;
    *__osViNext = *__osViCurr;
}
