#pragma once

#include <engine/resource/Resource.h>
#include "runtime/libultra.h"
#include "z64math.h"

namespace SOH {
enum class LimbType {
    Invalid,
    Standard,
    LOD,
    Skin,
    Curve,
    Legacy,
};

enum class ZLimbSkinType {
    SkinType_0,          // Segment = 0
    SkinType_4 = 4,      // Segment = segmented address // Struct_800A5E28
    SkinType_5 = 5,      // Segment = 0
    SkinType_DList = 11, // Segment = DList address
};

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
    /* 0x00 */ Vec3s jointPos; // Root is position in model space, children are relative to parent
    /* 0x06 */ uint8_t child;
    /* 0x07 */ uint8_t sibling;
    /* 0x08 */ Gfx* dLists[2]; // Near and far
} LodLimb;                     // size = 0x10

typedef struct {
    /* 0x00 */ Vec3s jointPos; // Root is position in model space, children are relative to parent
    /* 0x06 */ uint8_t child;
    /* 0x07 */ uint8_t sibling;
    /* 0x08 */ Gfx* dList;
} StandardLimb; // size = 0xC

typedef struct {
    /* 0x0000 */ uint8_t firstChildIdx;
    /* 0x0001 */ uint8_t nextLimbIdx;
    /* 0x0004 */ Gfx* dList[2];
} SkelCurveLimb; // size = 0xC

typedef struct {
    /* 0x00 */ Vec3s jointPos; // Root is position in model space, children are relative to parent
    /* 0x06 */ uint8_t child;
    /* 0x07 */ uint8_t sibling;
    /* 0x08 */ int32_t segmentType; // Type of data contained in segment
    /* 0x0C */ void* segment;   // Gfx* if segmentType is SKIN_LIMB_TYPE_NORMAL, SkinAnimatedLimbData* if segmentType is
                                // SKIN_LIMB_TYPE_ANIMATED, NULL otherwise
} SkinLimb;                     // size = 0x10

typedef struct {
    /* 0x00 */ uint16_t totalVtxCount; // total vertex count for all modif entries
    /* 0x02 */ uint16_t limbModifCount;
    /* 0x04 */ SkinLimbModif* limbModifications;
    /* 0x08 */ Gfx* dlist;
} SkinAnimatedLimbData; // size = 0xC

union SkeletonLimbData {
    LodLimb lodLimb;
    StandardLimb standardLimb;
    SkelCurveLimb skelCurveLimb;
    SkinLimb skinLimb;
};

class SkeletonLimb : public Engine::Resource<SkeletonLimbData> {
  public:
    using Resource::Resource;

    SkeletonLimb() : Resource(std::shared_ptr<Engine::ResourceInitData>()) {
    }

    SkeletonLimbData* GetPointer();
    size_t GetPointerSize();

    LimbType limbType;
    SkeletonLimbData limbData;

    ZLimbSkinType skinSegmentType;
    uint16_t skinVtxCnt;
    SkinAnimatedLimbData skinAnimLimbData;

    std::string skinDataDList;
    std::string skinDList;
    std::string skinDList2;

    float legTransX, legTransY, legTransZ; // Vec3f
    uint16_t rotX, rotY, rotZ;             // Vec3s

    std::string childPtr, siblingPtr, dListPtr, dList2Ptr;

    int16_t transX, transY, transZ;
    uint8_t childIndex, siblingIndex;

    uint32_t skinLimbModifCount;
    std::vector<SkinLimbModif> skinLimbModifArray;

    std::vector<std::vector<SkinVertex>> skinLimbModifVertexArrays;
    std::vector<std::vector<SkinTransformation>> skinLimbModifTransformationArrays;
};
} // namespace SOH