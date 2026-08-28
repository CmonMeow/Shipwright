#include "global.h"

void Overlay_Relocate(void* allocatedVRamAddress, OverlayRelocationSection* overlayInfo, void* vRamAddress) {
    uint32_t sections[4] = { 0 };
    uint32_t i;
    uint32_t* luiRefs[32] = { 0 };
    uint32_t luiVals[32] = { 0 };
    uint32_t* luiInstRef = { 0 };
    uintptr_t allocu32 = (uintptr_t)allocatedVRamAddress;

    uint32_t relocOffset = 0;
    uint32_t relocatedValue = 0;
    uintptr_t unrelocatedAddress = 0;
    uintptr_t relocatedAddress = 0;

    if (gOverlayLogSeverity >= 3) {
        osSyncPrintf("DoRelocation(%08x, %08x, %08x)\n", allocatedVRamAddress, overlayInfo, vRamAddress);
        osSyncPrintf("text=%08x, data=%08x, rodata=%08x, bss=%08x\n", overlayInfo->textSize, overlayInfo->dataSize,
                     overlayInfo->rodataSize, overlayInfo->bssSize);
    }

    sections[0] = 0;
    sections[1] = allocu32;
    sections[2] = allocu32 + overlayInfo->textSize;
    sections[3] = sections[2] + overlayInfo->dataSize;

    for (i = 0; i < overlayInfo->nRelocations; i++) {
        uint32_t reloc = overlayInfo->relocations[i];
        uintptr_t* relocDataP = (uint32_t*)(sections[reloc >> 0x1E] + (reloc & 0xFFFFFF));
        uint32_t relocData = *relocDataP;
        switch (reloc & 0x3F000000) {
            case 0x2000000:
                /* R_MIPS_32
                 * Handles 32-bit address relocation.  Used in things such as
                 * jump tables.
                 */
                if ((*relocDataP & 0xF000000) == 0) {
                    luiInstRef = vRamAddress;
                    relocOffset = *relocDataP - (uintptr_t)luiInstRef;
                    relocatedValue = relocOffset + allocu32;
                    relocatedAddress = relocatedValue;
                    unrelocatedAddress = relocData;
                    *relocDataP = relocatedAddress;
                }
                break;
            case 0x4000000:
                /* R_MIPS_26
                 * Handles 26-bit address relocation, used for jumps and jals
                 */
                unrelocatedAddress = ((*relocDataP & 0x3FFFFFF) << 2) | 0x80000000;
                relocOffset = unrelocatedAddress - (uintptr_t)vRamAddress;
                relocatedValue = (*relocDataP & 0xFC000000) | (((allocu32 + relocOffset) & 0xFFFFFFF) >> 2);
                relocatedAddress = ((relocatedValue & 0x3FFFFFF) << 2) | 0x80000000;
                *relocDataP = relocatedValue;
                break;
            case 0x5000000:
                /* R_MIPS_HI16
                 * Handles relocation for a lui instruciton, store the reference to
                 * the instruction, and will update it in the R_MIPS_LO16 section.
                 */
                luiRefs[(*relocDataP >> 0x10) & 0x1F] = relocDataP;
                luiVals[(*relocDataP >> 0x10) & 0x1F] = *relocDataP;
                break;
            case 0x6000000:
                /* R_MIPS_LO16
                 * Updates the LUI instruction to reflect the relocated address.
                 * The full address is calculated from the LUI and lo parts, and then updated.
                 * if the lo part is negative, add 1 to the lui.
                 */
                uint32_t* regValP = &luiVals[((*relocDataP >> 0x15) & 0x1F)];
                uintptr_t vaddr = (*regValP << 0x10) + (int16_t)*relocDataP;
                luiInstRef = luiRefs[((*relocDataP >> 0x15) & 0x1F)];
                if (luiInstRef == NULL) {
                    break;
                }
                if ((vaddr & 0xF000000) == 0) {
                    relocOffset = vaddr - (uintptr_t)vRamAddress;
                    vaddr = (int16_t)relocData;
                    uint32_t isLoNeg = (((relocOffset + allocu32) & 0x8000) ? 1 : 0);
                    unrelocatedAddress = (*luiInstRef << 0x10) + vaddr;
                    *luiInstRef =
                        (*luiInstRef & 0xFFFF0000) | ((((relocOffset + allocu32) >> 0x10) & 0xFFFF) + isLoNeg);
                    relocatedValue = (*relocDataP & 0xFFFF0000) | ((relocOffset + allocu32) & 0xFFFF);

                    relocatedAddress = (*luiInstRef << 0x10) + (int16_t)relocatedValue;
                    *relocDataP = relocatedValue;
                }
                break;
        }

        uint32_t dbg = 0x10;
        switch (reloc & 0x3F000000) {
            case 0x2000000:
                dbg = 0x16;
            case 0x4000000:
                dbg += 0xA;
            case 0x6000000:
                if (gOverlayLogSeverity >= 3) {
                    osSyncPrintf("%02d %08x %08x %08x ", dbg, relocDataP, relocatedValue, relocatedAddress);
                    osSyncPrintf(" %08x %08x %08x %08x\n", ((uintptr_t)relocDataP + (uintptr_t)vRamAddress) - allocu32,
                                 relocData, unrelocatedAddress, relocOffset);
                }
        }
    }
}
