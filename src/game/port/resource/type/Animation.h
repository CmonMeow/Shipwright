#pragma once

#include <engine/resource/Resource.h>
#include <runtime/libultra/types.h>

namespace SOH {
enum class AnimationType {
    Normal = 0,
    Link = 1,
    Curve = 2,
    Legacy = 3,
};

struct RotationIndex {
    uint16_t x, y, z;

    RotationIndex(uint16_t nX, uint16_t nY, uint16_t nZ) : x(nX), y(nY), z(nZ) {
    }
};

typedef struct {
    /* 0x0000 */ uint16_t unk_00; // appears to be flags
    /* 0x0002 */ int16_t unk_02;
    /* 0x0004 */ int16_t unk_04;
    /* 0x0006 */ int16_t unk_06;
    /* 0x0008 */ float unk_08;
} TransformData; // size = 0xC

typedef struct {
    /* 0x0000 */ uint8_t* refIndex;
    /* 0x0004 */ TransformData* transformData;
    /* 0x0008 */ int16_t* copyValues;
    /* 0x000C */ int16_t unk_0C;
    /* 0x000E */ int16_t unk_0E;
} TransformUpdateIndex; // size = 0x10

typedef struct {
    /* 0x00 */ int16_t frameCount;
} AnimationHeaderCommon;

// Index into the frame data table.
typedef struct {
    /* 0x00 */ uint16_t x;
    /* 0x02 */ uint16_t y;
    /* 0x04 */ uint16_t z;
} JointIndex; // size = 0x06

typedef struct {
    /* 0x00 */ AnimationHeaderCommon common;
    /* 0x04 */ int16_t* frameData;           // "tbl"
    /* 0x08 */ JointIndex* jointIndices; // "ref_tbl"
    /* 0x0C */ uint16_t staticIndexMax;
} AnimationHeader; // size = 0x10

typedef struct {
    /* 0x00 */ AnimationHeaderCommon common;
    /* 0x04 */ void* segment;
} LinkAnimationHeader; // size = 0x8

union AnimationData {
    AnimationHeader animationHeader;
    LinkAnimationHeader linkAnimationHeader;
    TransformUpdateIndex transformUpdateIndex;
};

class Animation : public Engine::Resource<AnimationData> {
  public:
    using Resource::Resource;

    Animation() : Resource(std::shared_ptr<Engine::ResourceInitData>()) {
    }

    AnimationData* GetPointer();
    size_t GetPointerSize();

    AnimationType type;
    AnimationData animationData;

    // NORMAL
    std::vector<uint16_t> rotationValues;
    std::vector<RotationIndex> rotationIndices;

    // CURVE
    std::vector<uint8_t> refIndexArr;
    std::vector<TransformData> transformDataArr;
    std::vector<int16_t> copyValuesArr;
};
}; // namespace SOH
