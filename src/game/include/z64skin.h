#ifndef Z64_SKIN_H
#define Z64_SKIN_H

#include "z64animation.h"

#ifdef __cplusplus
extern "C"
{
#endif

/**
 * Holds a compact version of a vertex used in the Skin system
 * It is used to initialise the Vtx used by an animated limb
 */
typedef struct {
    /* 0x00 */ uint16_t index;
    /* 0x02 */ int16_t s; // s and t are texture coordinates (also known as u and v)
    /* 0x04 */ int16_t t;
    /* 0x06 */ int8_t normX;
    /* 0x07 */ int8_t normY;
    /* 0x08 */ int8_t normZ;
    /* 0x09 */ uint8_t alpha;
} SkinVertex; // size = 0xA

/**
 * Describes a position displacement and a scale to be applied to a limb at index `limbIndex`
 */
typedef struct {
    /* 0x00 */ uint8_t limbIndex;
    /* 0x02 */ int16_t x;
    /* 0x04 */ int16_t y;
    /* 0x06 */ int16_t z;
    /* 0x08 */ uint8_t scale;
} SkinTransformation; // size = 0xA

typedef struct {
    /* 0x00 */ uint16_t vtxCount; // number of vertices in this modif entry
    /* 0x02 */ uint16_t transformCount;
    /* 0x04 */ uint16_t unk_4; // index of limbTransformations?
    /* 0x08 */ SkinVertex* skinVertices;
    /* 0x0C */ SkinTransformation* limbTransformations;
} SkinLimbModif; // size = 0x10

typedef struct {
    /* 0x00 */ uint16_t totalVtxCount; // total vertex count for all modif entries
    /* 0x02 */ uint16_t limbModifCount;
    /* 0x04 */ SkinLimbModif* limbModifications;
    /* 0x08 */ Gfx* dlist;
} SkinAnimatedLimbData; // size = 0xC

// ZAPD compatibility typedefs
// TODO: Remove when ZAPD adds support for them
typedef SkinVertex Struct_800A57C0;
typedef SkinTransformation Struct_800A598C_2;
typedef SkinAnimatedLimbData Struct_800A5E28;
typedef SkinLimbModif Struct_800A598C;

#define SKIN_LIMB_TYPE_ANIMATED 4
#define SKIN_LIMB_TYPE_NORMAL 11

typedef struct {
    /* 0x00 */ Vec3s jointPos; // Root is position in model space, children are relative to parent
    /* 0x06 */ uint8_t child;
    /* 0x07 */ uint8_t sibling;
    /* 0x08 */ int32_t segmentType; // Type of data contained in segment
    /* 0x0C */ void* segment; // Gfx* if segmentType is SKIN_LIMB_TYPE_NORMAL, SkinAnimatedLimbData* if segmentType is SKIN_LIMB_TYPE_ANIMATED, NULL otherwise
} SkinLimb; // size = 0x10

typedef struct {
    /* 0x000 */ uint8_t index; // alternates every draw cycle
    /* 0x004 */ Vtx* buf[2]; // number of vertices in buffer determined by `totalVtxCount`
} SkinLimbVtx; // size = 0xC

typedef struct {
    /* 0x000 */ SkeletonHeader* skeletonHeader;
    /* 0x004 */ MtxF mtx;
    /* 0x044 */ int32_t limbCount;
    /* 0x048 */ SkinLimbVtx* vtxTable; // double buffered list of vertices for each limb
    /* 0x04C */ SkelAnime skelAnime;
} Skin; // size = 0x90

typedef void (*SkinPostDraw)(struct Actor*, struct PlayState*, Skin*);
typedef int32_t (*SkinOverrideLimbDraw)(struct Actor*, struct PlayState*, int32_t, Skin*);

#define SKIN_DRAW_FLAG_CUSTOM_TRANSFORMS (1 << 0)
#define SKIN_DRAW_FLAG_CUSTOM_MATRIX     (1 << 1)


#ifdef __cplusplus
};
#endif

#endif
