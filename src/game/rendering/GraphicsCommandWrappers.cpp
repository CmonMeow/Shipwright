#include "z64.h"

// TODO: Replace these C wrappers with typed graphics commands.

extern "C" {
char* ResourceMgr_LoadTexOrDListByName(char* filePath);
char* ResourceMgr_LoadIfDListByName(char* filePath);
Gfx* ResourceMgr_LoadGfxByName(char* path);
Vtx* ResourceMgr_LoadVtxByName(char* path);
int ResourceMgr_HasResourceSignature(const char* data);
}

extern "C" void gSPSegment(void* value, int segNum, uintptr_t target) {
    char* imgData = (char*)target;

    int res = ResourceMgr_HasResourceSignature(imgData);

    // TODO: Revisit texture caching after the texture-corruption issue is understood.
    // With HD textures, we need to pass the path to F3D, not the raw texture data.
    // Otherwise the needed metadata is not available for proper rendering...
    // This should *not* cause any crashes, but some testing may be needed...
    // UPDATE: To maintain compatability it will still do the old behavior if the resource is a display list.
    // That should not affect HD textures.
    if (res) {
        uintptr_t desiredTarget = (uintptr_t)ResourceMgr_LoadIfDListByName(imgData);

        if (desiredTarget)
            target = desiredTarget;
    }

    __gSPSegment(value, segNum, target);
}

extern "C" void gSPSegmentLoadRes(void* value, int segNum, uintptr_t target) {
    char* imgData = (char*)target;

    int res = ResourceMgr_HasResourceSignature(imgData);

    if (res) {
        target = (uintptr_t)ResourceMgr_LoadTexOrDListByName(imgData);
    }

    __gSPSegment(value, segNum, target);
}

extern "C" void gSPDisplayList(Gfx* pkt, Gfx* dl) {
    char* imgData = (char*)dl;

    if (ResourceMgr_HasResourceSignature(imgData) == 1) {

        // ResourceMgr_PushCurrentDirectory(imgData);
        // gsSPPushCD(pkt++, imgData);
        dl = ResourceMgr_LoadGfxByName(imgData);
    }

    __gSPDisplayList(pkt, dl);
}

extern "C" void gDPSetTileSizeInterp(Gfx* pkt, int t, float uls, float ult, float lrs, float lrt) {
    __gDPSetTileSizeInterp(pkt, t, 0, 0, 0, 0);
    pkt->words.w0 = _SHIFTL(G_SETTILESIZE_INTERP, 24, 8);
    pkt++;

    pkt->words.w0 = *(uint32_t*)&uls;
    pkt->words.w1 = *(uint32_t*)&ult;
    pkt++;

    pkt->words.w0 = *(uint32_t*)&lrs;
    pkt->words.w1 = *(uint32_t*)&lrt;
    pkt++;
}

extern "C" void gSPDisplayListOffset(Gfx* pkt, Gfx* dl, int offset) {
    char* imgData = (char*)dl;

    if (ResourceMgr_HasResourceSignature(imgData) == 1)
        dl = ResourceMgr_LoadGfxByName(imgData);

    __gSPDisplayList(pkt, dl + offset);
}

extern "C" void gSPVertex(Gfx* pkt, uintptr_t v, int n, int v0) {

    if (ResourceMgr_HasResourceSignature((const char*)v) == 1)
        v = (uintptr_t)ResourceMgr_LoadVtxByName((char*)v);

    __gSPVertex(pkt, v, n, v0);
}

extern "C" void gSPInvalidateTexCache(Gfx* pkt, uintptr_t texAddr) {
    char* imgData = (char*)texAddr;

    if (texAddr != 0 && ResourceMgr_HasResourceSignature(imgData)) {
        // Temporary solution to the mq/nonmq issue, this will be
        // handled better with LUS 1.0
        texAddr = (uintptr_t)ResourceMgr_LoadTexOrDListByName(imgData);
    }

    __gSPInvalidateTexCache(pkt, texAddr);
}
