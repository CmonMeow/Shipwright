#pragma once

#include <engine/resource/Resource.h>

namespace Engine::Rendering {
union F3DVtx;
}

namespace Game::Resources {
typedef union ScalarData {
    uint8_t unsigned8;
    int8_t signed8;
    uint16_t unsigned16;
    int16_t signed16;
    uint32_t unsigned32;
    int32_t signed32;
    uint64_t unsigned64;
    int64_t signed64;
    float singlePrecision;
    double doublePrecision;
} ScalarData;

enum class ScalarType {
    ZSCALAR_NONE,
    ZSCALAR_S8,
    ZSCALAR_U8,
    ZSCALAR_X8,
    ZSCALAR_S16,
    ZSCALAR_U16,
    ZSCALAR_X16,
    ZSCALAR_S32,
    ZSCALAR_U32,
    ZSCALAR_X32,
    ZSCALAR_S64,
    ZSCALAR_U64,
    ZSCALAR_X64,
    ZSCALAR_F32,
    ZSCALAR_F64
};

// TODO: Replace this with a shared serialized-array type definition.
enum class ArrayResourceType {
    Error,
    Animation,
    Array,
    AltHeader,
    Background,
    Blob,
    CollisionHeader,
    Cutscene,
    DisplayList,
    Limb,
    LimbTable,
    Mtx,
    Path,
    PlayerAnimationData,
    Room,
    RoomCommand,
    Scalar,
    Scene,
    Skeleton,
    String,
    Symbol,
    Texture,
    TextureAnimation,
    TextureAnimationParams,
    Vector,
    Vertex,
    Audio
};

class Array : public Engine::Resource<void> {
  public:
    using Resource::Resource;

    Array();

    void* GetPointer() override;
    size_t GetPointerSize() override;

    ArrayResourceType ArrayType;
    ScalarType ArrayScalarType;
    size_t ArrayCount;
    // TODO: Store resource pointers directly.
    std::vector<ScalarData> Scalars;
    std::vector<Engine::Rendering::F3DVtx> Vertices;
};
} // namespace Game::Resources
