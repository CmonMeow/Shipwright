#ifndef Z64EFFECT_H
#define Z64EFFECT_H

#include <runtime/color.h>

struct GraphicsContext;
struct PlayState;

/* Effect Soft Sprites */

struct EffectSs;

typedef uint32_t (*EffectSsInitFunc)(struct PlayState* play, uint32_t index, struct EffectSs* effectSs, void* initParams);
typedef void (*EffectSsUpdateFunc)(struct PlayState* play, uint32_t index, struct EffectSs* effectSs);
typedef void (*EffectSsDrawFunc)(struct PlayState* play, uint32_t index, struct EffectSs* effectSs);

typedef struct {
    /* 0x00 */ uint32_t type;
    /* 0x04 */ EffectSsInitFunc init;
} EffectSsInit; // size = 0x08

typedef struct {
    /* 0x00 */ uintptr_t vromStart;
    /* 0x04 */ uintptr_t vromEnd;
    /* 0x08 */ void* vramStart;
    /* 0x0C */ void* vramEnd;
    /* 0x10 */ void* loadedRamAddr;
    /* 0x14 */ EffectSsInit* initInfo;
    /* 0x18 */ uint8_t unk_18;
} EffectSsOverlay; // size = 0x1C

typedef struct EffectSs {
    /* 0x00 */ Vec3f pos;
    /* 0x0C */ Vec3f velocity;
    /* 0x18 */ Vec3f accel;
    /* 0x24 */ EffectSsUpdateFunc update;
    /* 0x28 */ EffectSsDrawFunc draw;
    /* 0x2C */ Vec3f vec; // usage specific per effect
    /* 0x38 */ void* gfx; // mostly used for display lists, sometimes textures
    /* 0x3C */ Actor* actor; // interfacing actor, usually the actor that spawned the effect
    /* 0x40 */ int16_t regs[13]; // specific per effect
    /* 0x5A */ uint16_t flags;
    /* 0x5C */ int16_t life; // -1 means this entry is free
    /* 0x5E */ uint8_t priority; // Lower value means higher priority
    /* 0x5F */ uint8_t type;
    uint32_t epoch;
} EffectSs; // size = 0x60

typedef struct {
    /* 0x00 */ EffectSs* table; // "data_table"
    /* 0x04 */ int32_t searchStartIndex;
    /* 0x08 */ int32_t tableSize;
} EffectSsInfo; // size = 0x0C

/* G Effect Regs */

#define rgTexIdx regs[0]
#define rgScale regs[1]
#define rgTexIdxStep regs[2]
#define rgPrimColorR regs[3]
#define rgPrimColorG regs[4]
#define rgPrimColorB regs[5]
#define rgPrimColorA regs[6]
#define rgEnvColorR regs[7]
#define rgEnvColorG regs[8]
#define rgEnvColorB regs[9]
#define rgEnvColorA regs[10]
#define rgObjBankIdx regs[11]

#define DEFINE_EFFECT_SS(_0, enum) enum,
#define DEFINE_EFFECT_SS_UNSET(enum) enum,

typedef enum {
    #include "tables/effect_ss_table.h"
    /* 0x25 */ EFFECT_SS_TYPE_MAX // originally "EFFECT_SS2_TYPE_LAST_LABEL"
} EffectSsType;

#undef DEFINE_EFFECT_SS
#undef DEFINE_EFFECT_SS_UNSET

#endif
