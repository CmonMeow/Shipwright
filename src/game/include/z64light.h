#ifndef Z64LIGHT_H
#define Z64LIGHT_H

#include <runtime/libultra.h>
#include <runtime/libultra/gbi.h>
#include "z64math.h"
#include <runtime/color.h>

typedef struct {
    /* 0x0 */ int16_t x;
    /* 0x2 */ int16_t y;
    /* 0x4 */ int16_t z;
    /* 0x6 */ uint8_t color[3];
    /* 0x9 */ uint8_t drawGlow;
    /* 0xA */ int16_t radius;
} LightPoint; // size = 0xC

typedef struct {
    /* 0x0 */ int8_t x;
    /* 0x1 */ int8_t y;
    /* 0x2 */ int8_t z;
    /* 0x3 */ uint8_t color[3];
} LightDirectional; // size = 0x6

typedef union {
    LightPoint point;
    LightDirectional dir;
} LightParams; // size = 0xC

typedef struct {
    /* 0x0 */ uint8_t type;
    /* 0x2 */ LightParams params;
} LightInfo; // size = 0xE

typedef struct Lights {
    /* 0x00 */ uint8_t numLights;
    /* 0x08 */ Lightsn l;
} Lights; // size = 0x80

typedef struct LightNode {
    /* 0x0 */ LightInfo* info;
    /* 0x4 */ struct LightNode* prev;
    /* 0x8 */ struct LightNode* next;
} LightNode; // size = 0xC

typedef struct {
    /* 0x0 */ LightNode* listHead;
    /* 0x4 */ uint8_t ambientColor[3];
    /* 0x7 */ uint8_t fogColor[3];
    /* 0xA */ int16_t fogNear; // how close until fog starts taking effect. range 0 - 1000
    /* 0xC */ int16_t fogFar; // how far until fog starts to saturate. range 0 - 1000
} LightContext; // size = 0x10

typedef enum {
    /* 0x00 */ LIGHT_POINT_NOGLOW,
    /* 0x01 */ LIGHT_DIRECTIONAL,
    /* 0x02 */ LIGHT_POINT_GLOW
} LightType;

typedef void (*LightsBindFunc)(Lights* lights, LightParams* params, Vec3f* vec);

#endif
