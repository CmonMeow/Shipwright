#pragma once

#include <cstdint>
#include <vector>
#include <memory>
#include <engine/resource/Resource.h>
#include "SceneCommand.h"
#include "runtime/libultra.h"

namespace SOH {
typedef struct {
    /* 0x00 */ uint8_t type;
} PolygonBase;

typedef struct {
    /* 0x00 */ PolygonBase base;
    /* 0x01 */ uint8_t num; // number of dlist entries
    /* 0x04 */ void* start;
    /* 0x08 */ void* end;
} PolygonType0; // size = 0xC

typedef union {
    PolygonBase base;
    PolygonType0 polygon0;
} MeshHeader; // "Ground Shape"

typedef struct {
    /* 0x00 */ Gfx* opa;
    /* 0x04 */ Gfx* xlu;
} PolygonDlist; // size = 0x8

class SetMesh : public SceneCommand<MeshHeader> {
  public:
    using SceneCommand::SceneCommand;

    MeshHeader* GetPointer();
    size_t GetPointerSize();

    uint32_t numPoly;
    uint8_t data;
    uint8_t meshHeaderType;

    std::vector<std::string> opaPaths;
    std::vector<std::string> xluPaths;
    std::vector<PolygonDlist> dlists;
    MeshHeader meshHeader;
};
}; // namespace SOH
