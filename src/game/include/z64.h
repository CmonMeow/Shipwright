#ifndef Z64_H
#define Z64_H

#include <runtime/libultra.h>
#include "unk.h" // this used to get pulled in via ultra64.h
#include "attributes.h"
#include "z64save.h"
#include "z64light.h"
#include "z64bgcheck.h"
#include "z64actor.h"
#include "z64player.h"
#include "z64audio.h"
#include "z64object.h"
#include "z64camera.h"
#include "z64environment.h"
#include "z64runtime_action.h"
#include "z64collision_check.h"
#include "z64scene.h"
#include "z64effect.h"
#include "z64item.h"
#include "z64animation.h"
#include "z64dma.h"
#include "z64math.h"
#include "z64skin.h"
#include "z64transition.h"
#include "z64interface.h"
#include "alignment.h"
#include "sequence.h"
#include "sfx.h"
#include <runtime/color.h>
#include "ichain.h"
#include "regs.h"
#include "gfx.h"

#if defined(__LP64__)
#define _SOH64
#endif

#define AUDIO_HEAP_SIZE  0x380000
#define SYSTEM_HEAP_SIZE (1024 * 1024 * 4)

#ifdef __cplusplus
namespace LUS
{
    class IResource;
    class Scene;
};
namespace Fast {
    class DisplayList;
};
#include <memory>
#endif

#define SCREEN_WIDTH  320
#define SCREEN_HEIGHT 240

#define REGION_NULL 0
#define REGION_US 1
#define REGION_JP 2
#define REGION_EU 3

#define Z_PRIORITY_MAIN        10
#define Z_PRIORITY_GRAPH       11
#define Z_PRIORITY_AUDIOMGR    12
#define Z_PRIORITY_PADMGR      14
#define Z_PRIORITY_SCHED       15
#define Z_PRIORITY_DMAMGR      16
#define Z_PRIORITY_IRQMGR      17

// NOTE: Once we start supporting other builds, this can be changed with an ifdef
#define REGION_NATIVE REGION_EU

#ifdef __cplusplus
extern "C"
{
#endif

typedef struct{
    /* 0x00 */ char unk[0x4];
    /* 0x04 */ MtxF mf;
} HorseStruct;

// Game Info aka. Static Context (dbg ram start: 80210A10)
// Data normally accessed through REG macros (see regs.h)
typedef struct {
    /* 0x00 */ int32_t  regPage;   // 1 is first page
    /* 0x04 */ int32_t  regGroup;  // "register" group (R, RS, RO, RP etc.)
    /* 0x08 */ int32_t  regCur;    // selected register within page
    /* 0x0C */ int32_t  dpadLast;
    /* 0x10 */ int32_t  repeat;
    /* 0x14 */ int16_t  data[REG_GROUPS * REG_PER_GROUP]; // 0xAE0 entries
} GameInfo; // size = 0x15D4

typedef struct {
    /* 0x00000 */ uint16_t headMagic; // GFXPOOL_HEAD_MAGIC
    /* 0x00008 */ Gfx polyOpaBuffer[0x2FC0];
    /* 0x0BF08 */ Gfx polyXluBuffer[0x1000];
    /* 0x0FF08 */ Gfx overlayBuffer[0x800];
    /* 0x11F08 */ Gfx workBuffer[0x100];
    /* 0x11308 */ Gfx unusedBuffer[0x40];
    /* 0x12408 */ uint16_t tailMagic; // GFXPOOL_TAIL_MAGIC
} GfxPool; // size = 0x24820

typedef struct {
    /* 0x0000 */ uint32_t    size;
    /* 0x0004 */ void*    bufp;
    /* 0x0008 */ void*    head;
    /* 0x000C */ void*    tail;
} TwoHeadArena; // size = 0x10

typedef struct {
    /* 0x0000 */ uint32_t    size;
    /* 0x0004 */ Gfx*   bufp;
    /* 0x0008 */ Gfx*   p;
    /* 0x000C */ Gfx*   d;
} TwoHeadGfxArena; // size = 0x10

typedef struct {
    /* 0x00 */ uint16_t* fb1;
    /* 0x04 */ uint16_t* swapBuffer;
    /* 0x08 */ OSViMode* viMode;
    /* 0x0C */ uint32_t features;
    /* 0x10 */ uint8_t unk_10;
    /* 0x11 */ int8_t updateRate;
    /* 0x12 */ int8_t updateRate2;
    /* 0x13 */ uint8_t unk_13;
    /* 0x14 */ float xScale;
    /* 0x18 */ float yScale;
} CfbInfo; // size = 0x1C

typedef struct OSScTask {
    /* 0x00 */ struct OSScTask* next;
    /* 0x04 */ uint32_t state;
    /* 0x08 */ uint32_t flags;
    /* 0x0C */ CfbInfo* framebuffer;
    /* 0x10 */ OSTask list;
    /* 0x50 */ OSMesgQueue* msgQ;
    /* 0x54 */ OSMesg msg;
} OSScTask;

typedef struct GraphicsContext {
    /* 0x0000 */ Gfx* polyOpaBuffer; // Pointer to "Zelda 0"
    /* 0x0004 */ Gfx* polyXluBuffer; // Pointer to "Zelda 1"
    /* 0x0008 */ char unk_008[0x08]; // Unused, could this be pointers to "Zelda 2" / "Zelda 3"
    /* 0x0010 */ Gfx* overlayBuffer; // Pointer to "Zelda 4"
    /* 0x0014 */ uint32_t unk_014;
    /* 0x0018 */ char unk_018[0x20];
    /* 0x0038 */ OSMesg msgBuff[0x08];
    /* 0x0058 */ OSMesgQueue* schedMsgQ;
    /* 0x005C */ OSMesgQueue queue;
    /* 0x0074 */ char unk_074[0x04];
    /* 0x0078 */ OSScTask task; // size of OSScTask might be wrong
    /* 0x00D0 */ char unk_0D0[0xE0];
    /* 0x01B0 */ Gfx* workBuffer;
    /* 0x01B4 */ TwoHeadGfxArena work;
    /* 0x01C4 */ char unk_01C4[0xC0];
    /* 0x0284 */ OSViMode* viMode;
    /* 0x0288 */ char unk_0288[0x20]; // Unused, could this be Zelda 2/3 ?
    /* 0x02A8 */ TwoHeadGfxArena overlay; // "Zelda 4"
    /* 0x02B8 */ TwoHeadGfxArena polyOpa; // "Zelda 0"
    /* 0x02C8 */ TwoHeadGfxArena polyXlu; // "Zelda 1"
    /* 0x02D8 */ uint32_t gfxPoolIdx;
    /* 0x02DC */ uint16_t* curFrameBuffer;
    /* 0x02E0 */ char unk_2E0[0x04];
    /* 0x02E4 */ uint32_t viFeatures;
    /* 0x02E8 */ int32_t fbIdx;
    /* 0x02EC */ void (*callback)(struct GraphicsContext*, void*);
    /* 0x02F0 */ void* callbackParam;
    /* 0x02F4 */ float xScale;
    /* 0x02F8 */ float yScale;
    /* 0x02FC */ char unk_2FC[0x04];
} GraphicsContext; // size = 0x300

typedef struct {
    /* 0x00 */ OSContPad cur;
    /* 0x06 */ OSContPad prev;
    /* 0x0C */ OSContPad press; // X/Y store delta from last frame
    /* 0x12 */ OSContPad rel; // X/Y store adjusted
} Input; // size = 0x18

typedef struct {
   /* 0x0000 */ int32_t topY;    // uly (upper left y)
   /* 0x0004 */ int32_t bottomY; // lry (lower right y)
   /* 0x0008 */ int32_t leftX;   // ulx (upper left x)
   /* 0x000C */ int32_t rightX;  // lrx (lower right x)
} Viewport; // size = 0x10

typedef struct {
    /* 0x0000 */ int32_t    magic; // string literal "VIEW" / 0x56494557
    /* 0x0004 */ GraphicsContext* gfxCtx;
    /* 0x0008 */ Viewport viewport;
    /* 0x0018 */ float    fovy;  // vertical field of view in degrees
    /* 0x001C */ float    zNear; // distance to near clipping plane
    /* 0x0020 */ float    zFar;  // distance to far clipping plane
    /* 0x0024 */ float    scale; // scale for matrix elements
    /* 0x0028 */ Vec3f  eye;
    /* 0x0034 */ Vec3f  lookAt;
    /* 0x0040 */ Vec3f  up;
    /* 0x0050 */ Vp     vp;
    /* 0x0060 */ Mtx    projection;
    /* 0x00A0 */ Mtx    viewing;
    /* 0x00E0 */ Mtx*   projectionPtr;
    /* 0x00E0 */ Mtx*   projectionFlippedPtr;
    /* 0x00E4 */ Mtx*   viewingPtr;
    /* 0x00E8 */ Vec3f  distortionOrientation;
    /* 0x00F4 */ Vec3f  distortionScale;
    /* 0x0100 */ float    distortionSpeed;
    /* 0x0104 */ Vec3f  curDistortionOrientation;
    /* 0x0110 */ Vec3f  curDistortionScale;
    /* 0x011C */ uint16_t    normal; // used to normalize the projection matrix
    /* 0x0120 */ int32_t    flags;
    /* 0x0124 */ int32_t    unk_124;
} View; // size = 0x128

typedef enum {
    /*  0 */ SETUPDL_0,
    /*  1 */ SETUPDL_1,
    /*  2 */ SETUPDL_2,
    /*  3 */ SETUPDL_3,
    /*  4 */ SETUPDL_4,
    /*  5 */ SETUPDL_5,
    /*  6 */ SETUPDL_6,
    /*  7 */ SETUPDL_7,
    /*  8 */ SETUPDL_8,
    /*  9 */ SETUPDL_9,
    /* 10 */ SETUPDL_10,
    /* 11 */ SETUPDL_11,
    /* 12 */ SETUPDL_12,
    /* 13 */ SETUPDL_13,
    /* 14 */ SETUPDL_14,
    /* 15 */ SETUPDL_15,
    /* 16 */ SETUPDL_16,
    /* 17 */ SETUPDL_17,
    /* 18 */ SETUPDL_18,
    /* 19 */ SETUPDL_19,
    /* 20 */ SETUPDL_20,
    /* 21 */ SETUPDL_21,
    /* 22 */ SETUPDL_22,
    /* 23 */ SETUPDL_23,
    /* 24 */ SETUPDL_24,
    /* 25 */ SETUPDL_25,
    /* 26 */ SETUPDL_26,
    /* 27 */ SETUPDL_27,
    /* 28 */ SETUPDL_28,
    /* 29 */ SETUPDL_29,
    /* 30 */ SETUPDL_30,
    /* 31 */ SETUPDL_31,
    /* 32 */ SETUPDL_32,
    /* 33 */ SETUPDL_33,
    /* 34 */ SETUPDL_34,
    /* 35 */ SETUPDL_35,
    /* 36 */ SETUPDL_36,
    /* 37 */ SETUPDL_37,
    /* 38 */ SETUPDL_38,
    /* 39 */ SETUPDL_39,
    /* 40 */ SETUPDL_40,
    /* 41 */ SETUPDL_41,
    /* 42 */ SETUPDL_42,
    /* 43 */ SETUPDL_43,
    /* 44 */ SETUPDL_44,
    /* 45 */ SETUPDL_45,
    /* 46 */ SETUPDL_46,
    /* 47 */ SETUPDL_47,
    /* 48 */ SETUPDL_48,
    /* 49 */ SETUPDL_49,
    /* 50 */ SETUPDL_50,
    /* 51 */ SETUPDL_51,
    /* 52 */ SETUPDL_52,
    /* 53 */ SETUPDL_53,
    /* 54 */ SETUPDL_54,
    /* 55 */ SETUPDL_55,
    /* 56 */ SETUPDL_56,
    /* 57 */ SETUPDL_57,
    /* 58 */ SETUPDL_58,
    /* 59 */ SETUPDL_59,
    /* 60 */ SETUPDL_60,
    /* 61 */ SETUPDL_61,
    /* 62 */ SETUPDL_62,
    /* 63 */ SETUPDL_63,
    /* 64 */ SETUPDL_64,
    /* 65 */ SETUPDL_65,
    /* 66 */ SETUPDL_66,
    /* 67 */ SETUPDL_67,
    /* 68 */ SETUPDL_68,
    /* 69 */ SETUPDL_69,
    /* 70 */ SETUPDL_70,
    /* 71 */ SETUPDL_MAX
} SetupDL;

typedef struct {
    /* 0x00 */ uint8_t   seqId;
    /* 0x01 */ uint8_t   natureAmbienceId;
} SequenceContext; // size = 0x2

typedef struct {
    /* 0x00 */ int32_t enabled;
    /* 0x04 */ int32_t timer;
} FrameAdvanceContext; // size = 0x8

typedef struct {
    /* 0x00 */ Vec3f    pos;
    /* 0x0C */ float      unk_0C; // radius?
    /* 0x10 */ Color_RGB8 color;
} TargetContextEntry; // size = 0x14

typedef struct {
    /* 0x00 */ Vec3f    naviRefPos; // possibly wrong
    /* 0x0C */ Vec3f    targetCenterPos;
    /* 0x18 */ Color_RGBAf naviInner;
    /* 0x28 */ Color_RGBAf naviOuter;
    /* 0x38 */ Actor*   arrowPointedActor;
    /* 0x3C */ Actor*   targetedActor;
    /* 0x40 */ float      unk_40;
    /* 0x44 */ float      unk_44;
    /* 0x48 */ int16_t      unk_48;
    /* 0x4A */ uint8_t       activeCategory;
    /* 0x4B */ uint8_t       unk_4B;
    /* 0x4C */ int8_t       unk_4C;
    /* 0x4D */ char     unk_4D[0x03];
    /* 0x50 */ TargetContextEntry arr_50[3];
    /* 0x8C */ Actor*   unk_8C;
    /* 0x90 */ Actor*   bgmEnemy; // The nearest enemy to player with the right flags that will trigger NA_BGM_ENEMY
    /* 0x94 */ Actor*   unk_94;
} TargetContext; // size = 0x98

typedef struct {
    /* 0x00 */ void*      texture;
    /* 0x04 */ int16_t      x;
    /* 0x06 */ int16_t      y;
    /* 0x08 */ uint8_t       width;
    /* 0x09 */ uint8_t       height;
    /* 0x0A */ uint8_t       durationTimer; // how long the title card appears for before fading
    /* 0x0B */ uint8_t       delayTimer; // how long the title card waits to appear
    /* 0x0C */ int16_t      alpha;
    /* ---- */ int16_t      intensityR; //Splited intensity per channel to support precise recolor
    /* ---- */ int16_t      intensityG;
    /* ---- */ int16_t      intensityB;
    /* ---- */ int16_t      isBossCard; //To detect if that a Boss name title card.
    /* ---- */ int16_t      hasTranslation; // to detect if the current title card has translation (used for bosses only)
} TitleCardContext; // size = 0x10

typedef struct {
    /* 0x00 */ int32_t    length; // number of actors loaded of this category
    /* 0x04 */ Actor* head; // pointer to head of the linked list of this category (most recent actor added)
} ActorListEntry; // size = 0x08

typedef struct {
    /* 0x0000 */ uint8_t     freezeFlashTimer;
    /* 0x0001 */ char   unk_01[0x01];
    /* 0x0002 */ uint8_t     unk_02;
    /* 0x0003 */ uint8_t     lensActive;
    /* 0x0004 */ char   unk_04[0x04];
    /* 0x0008 */ uint8_t     total; // total number of actors loaded
    /* 0x000C */ ActorListEntry actorLists[ACTORCAT_MAX];
    /* 0x006C */ TargetContext targetCtx;
    struct {
        /* 0x0104 */ uint32_t    swch;
        /* 0x0108 */ uint32_t    tempSwch;
        /* 0x010C */ uint32_t    unk0;
        /* 0x0110 */ uint32_t    unk1;
        /* 0x0114 */ uint32_t    chest;
        /* 0x0118 */ uint32_t    clear;
        /* 0x011C */ uint32_t    tempClear;
        /* 0x0120 */ uint32_t    collect;
        /* 0x0124 */ uint32_t    tempCollect;
    }                   flags;
    /* 0x0128 */ TitleCardContext titleCtx;
    /* 0x0138 */ char   unk_138[0x04];
    /* 0x013C */ void*  absoluteSpace; // Space used to allocate actor overlays of alloc type 1
} ActorContext; // size = 0x140

typedef struct {
    uint8_t state;
    uint16_t frames;
    CsCmdActorCue* linkAction;
    CsCmdActorCue* npcActions[10];
} PlayerActionContext;

typedef struct {
    /* 0x00 */ uint16_t countdown;
    /* 0x04 */ Vec3f worldPos;
    /* 0x10 */ Vec3f projectedPos;
} SoundSource; // size = 0x1C

typedef enum {
    /* 0x00 */ SKYBOX_NONE,
    /* 0x01 */ SKYBOX_NORMAL_SKY,
    /* 0x02 */ SKYBOX_BAZAAR,
    /* 0x03 */ SKYBOX_OVERCAST_SUNSET,
    /* 0x04 */ SKYBOX_MARKET_ADULT,
    /* 0x05 */ SKYBOX_CUTSCENE_MAP,
    /* 0x07 */ SKYBOX_HOUSE_LINK = 7,
    /* 0x09 */ SKYBOX_MARKET_CHILD_DAY = 9,
    /* 0x0A */ SKYBOX_MARKET_CHILD_NIGHT,
    /* 0x0B */ SKYBOX_HAPPY_MASK_SHOP,
    /* 0x0C */ SKYBOX_HOUSE_KNOW_IT_ALL_BROTHERS,
    /* 0x0E */ SKYBOX_HOUSE_OF_TWINS = 14,
    /* 0x0F */ SKYBOX_STABLES,
    /* 0x10 */ SKYBOX_HOUSE_KAKARIKO,
    /* 0x11 */ SKYBOX_KOKIRI_SHOP,
    /* 0x13 */ SKYBOX_GORON_SHOP = 19,
    /* 0x14 */ SKYBOX_ZORA_SHOP,
    /* 0x16 */ SKYBOX_POTION_SHOP_KAKARIKO = 22,
    /* 0x17 */ SKYBOX_POTION_SHOP_MARKET,
    /* 0x18 */ SKYBOX_BOMBCHU_SHOP,
    /* 0x1A */ SKYBOX_HOUSE_RICHARD = 26,
    /* 0x1B */ SKYBOX_HOUSE_IMPA,
    /* 0x1C */ SKYBOX_TENT,
    /* 0x1D */ SKYBOX_UNSET_1D,
    /* 0x20 */ SKYBOX_HOUSE_MIDO = 32,
    /* 0x21 */ SKYBOX_HOUSE_SARIA,
    /* 0x22 */ SKYBOX_HOUSE_ALLEY,
    /* 0x27 */ SKYBOX_UNSET_27 = 39
} SkyboxId;

typedef struct {
    char unk_00[0x128];
    int16_t skyboxId;
    void* textures[2][6];
    void* palettes[6];
    uint16_t palette_size;
    Gfx (*dListBuf)[150];
    Gfx* unk_138;
    Vtx* roomVtx;
    int16_t  unk_140;
    Vec3f rot;
    char unk_150[0x10];
} SkyboxContext;

typedef enum {
    /*  0 */ OCARINA_SONG_MINUET,
    /*  1 */ OCARINA_SONG_BOLERO,
    /*  2 */ OCARINA_SONG_SERENADE,
    /*  3 */ OCARINA_SONG_REQUIEM,
    /*  4 */ OCARINA_SONG_NOCTURNE,
    /*  5 */ OCARINA_SONG_PRELUDE,
    /*  6 */ OCARINA_SONG_SARIAS,
    /*  7 */ OCARINA_SONG_EPONAS,
    /*  8 */ OCARINA_SONG_LULLABY,
    /*  9 */ OCARINA_SONG_SUNS,
    /* 10 */ OCARINA_SONG_TIME,
    /* 11 */ OCARINA_SONG_STORMS,
    /* 12 */ OCARINA_SONG_SCARECROW,
    /* 13 */ OCARINA_SONG_MEMORY_GAME,
    /* 14 */ OCARINA_SONG_MAX,
    /* 14 */ OCARINA_SONG_SCARECROW_LONG = OCARINA_SONG_MAX // anything larger than 13 is considered the long scarecrow's song
} OcarinaSongId;

typedef enum {
    /* 0x00 */ OCARINA_ACTION_UNK_0, // acts like free play but never set
    /* 0x01 */ OCARINA_ACTION_FREE_PLAY,
    /* 0x02 */ OCARINA_ACTION_TEACH_MINUET, // Song demonstrations by teachers
    /* 0x03 */ OCARINA_ACTION_TEACH_BOLERO,
    /* 0x04 */ OCARINA_ACTION_TEACH_SERENADE,
    /* 0x05 */ OCARINA_ACTION_TEACH_REQUIEM,
    /* 0x06 */ OCARINA_ACTION_TEACH_NOCTURNE,
    /* 0x07 */ OCARINA_ACTION_TEACH_PRELUDE,
    /* 0x08 */ OCARINA_ACTION_TEACH_SARIA,
    /* 0x09 */ OCARINA_ACTION_TEACH_EPONA,
    /* 0x0A */ OCARINA_ACTION_TEACH_LULLABY,
    /* 0x0B */ OCARINA_ACTION_TEACH_SUNS,
    /* 0x0C */ OCARINA_ACTION_TEACH_TIME,
    /* 0x0D */ OCARINA_ACTION_TEACH_STORMS,
    /* 0x0E */ OCARINA_ACTION_UNK_E,
    /* 0x0F */ OCARINA_ACTION_PLAYBACK_MINUET, // Playing back a particular song
    /* 0x10 */ OCARINA_ACTION_PLAYBACK_BOLERO,
    /* 0x11 */ OCARINA_ACTION_PLAYBACK_SERENADE,
    /* 0x12 */ OCARINA_ACTION_PLAYBACK_REQUIEM,
    /* 0x13 */ OCARINA_ACTION_PLAYBACK_NOCTURNE,
    /* 0x14 */ OCARINA_ACTION_PLAYBACK_PRELUDE,
    /* 0x15 */ OCARINA_ACTION_PLAYBACK_SARIA,
    /* 0x16 */ OCARINA_ACTION_PLAYBACK_EPONA,
    /* 0x17 */ OCARINA_ACTION_PLAYBACK_LULLABY,
    /* 0x18 */ OCARINA_ACTION_PLAYBACK_SUNS,
    /* 0x19 */ OCARINA_ACTION_PLAYBACK_TIME,
    /* 0x1A */ OCARINA_ACTION_PLAYBACK_STORMS,
    /* 0x1B */ OCARINA_ACTION_UNK_1B,
    /* 0x1C */ OCARINA_ACTION_CHECK_MINUET, // Playing songs for check spots
    /* 0x1D */ OCARINA_ACTION_CHECK_BOLERO,
    /* 0x1E */ OCARINA_ACTION_CHECK_SERENADE,
    /* 0x1F */ OCARINA_ACTION_CHECK_REQUIEM,
    /* 0020 */ OCARINA_ACTION_CHECK_NOCTURNE,
    /* 0x21 */ OCARINA_ACTION_CHECK_PRELUDE,
    /* 0x22 */ OCARINA_ACTION_CHECK_SARIA,
    /* 0x23 */ OCARINA_ACTION_CHECK_EPONA,
    /* 0x24 */ OCARINA_ACTION_CHECK_LULLABY,
    /* 0x25 */ OCARINA_ACTION_CHECK_SUNS,
    /* 0x26 */ OCARINA_ACTION_CHECK_TIME,
    /* 0x27 */ OCARINA_ACTION_CHECK_STORMS,
    /* 0x28 */ OCARINA_ACTION_CHECK_SCARECROW, // Playing back the song as adult that was set as child
    /* 0x29 */ OCARINA_ACTION_FREE_PLAY_DONE,
    /* 0x2A */ OCARINA_ACTION_SCARECROW_LONG_RECORDING,
    /* 0x2B */ OCARINA_ACTION_SCARECROW_LONG_PLAYBACK,
    /* 0x2C */ OCARINA_ACTION_SCARECROW_RECORDING,
    /* 0x2D */ OCARINA_ACTION_SCARECROW_PLAYBACK,
    /* 0x2E */ OCARINA_ACTION_MEMORY_GAME,
    /* 0x2F */ OCARINA_ACTION_FROGS,
    /* 0x30 */ OCARINA_ACTION_CHECK_NOWARP, // Check for any of sarias - storms
    /* 0x31 */ OCARINA_ACTION_CHECK_NOWARP_DONE
} OcarinaSongActionIDs;

typedef enum {
    /* 0x00 */ OCARINA_MODE_00,
    /* 0x01 */ OCARINA_MODE_01,
    /* 0x02 */ OCARINA_MODE_02,
    /* 0x03 */ OCARINA_MODE_03,
    /* 0x04 */ OCARINA_MODE_04,
    /* 0x05 */ OCARINA_MODE_05,
    /* 0x06 */ OCARINA_MODE_06,
    /* 0x07 */ OCARINA_MODE_07,
    /* 0x08 */ OCARINA_MODE_08,
    /* 0x09 */ OCARINA_MODE_09,
    /* 0x0A */ OCARINA_MODE_0A,
    /* 0x0B */ OCARINA_MODE_0B,
    /* 0x0C */ OCARINA_MODE_0C,
    /* 0x0D */ OCARINA_MODE_0D,
    /* 0x0E */ OCARINA_MODE_0E,
    /* 0x0F */ OCARINA_MODE_0F
} OcarinaMode;

typedef enum {
    TEXTBOX_ICON_TRIANGLE,
    TEXTBOX_ICON_SQUARE,
    TEXTBOX_ICON_ARROW
} TextBoxIcon;

typedef enum {
    LANGUAGE_ENG,
    LANGUAGE_GER,
    LANGUAGE_FRA,
    LANGUAGE_JPN,
    LANGUAGE_MAX
} Language;

#define TODO_TRANSLATE "TranslateThis"

typedef enum {
    /* 0x00 */ MSGMODE_NONE,
    /* 0x01 */ MSGMODE_TEXT_START,
    /* 0x02 */ MSGMODE_TEXT_BOX_GROWING,
    /* 0x03 */ MSGMODE_TEXT_STARTING,
    /* 0x04 */ MSGMODE_TEXT_NEXT_MSG,
    /* 0x05 */ MSGMODE_TEXT_CONTINUING,
    /* 0x06 */ MSGMODE_TEXT_DISPLAYING,
    /* 0x07 */ MSGMODE_TEXT_AWAIT_INPUT,
    /* 0x08 */ MSGMODE_TEXT_DELAYED_BREAK,
    /* 0x09 */ MSGMODE_OCARINA_STARTING,
    /* 0x0A */ MSGMODE_SONG_DEMONSTRATION_STARTING,
    /* 0x0B */ MSGMODE_SONG_PLAYBACK_STARTING,
    /* 0x0C */ MSGMODE_OCARINA_PLAYING,
    /* 0x0D */ MSGMODE_OCARINA_CORRECT_PLAYBACK,
    /* 0x0E */ MSGMODE_OCARINA_FAIL, // Failed to play a valid song after entering 8 notes
    /* 0x0F */ MSGMODE_OCARINA_FAIL_NO_TEXT, // Never set, only compared against
    /* 0x10 */ MSGMODE_OCARINA_NOTES_DROP,
    /* 0x11 */ MSGMODE_SONG_PLAYED, // Played a full named song correctly
    /* 0x12 */ MSGMODE_SETUP_DISPLAY_SONG_PLAYED,
    /* 0x13 */ MSGMODE_DISPLAY_SONG_PLAYED,
    /* 0x14 */ MSGMODE_DISPLAY_SONG_PLAYED_TEXT_BEGIN,
    /* 0x15 */ MSGMODE_DISPLAY_SONG_PLAYED_TEXT,
    /* 0x16 */ MSGMODE_SONG_PLAYED_ACT_BEGIN,
    /* 0x17 */ MSGMODE_SONG_PLAYED_ACT, // Act on a played song
    /* 0x18 */ MSGMODE_SONG_DEMONSTRATION_SELECT_INSTRUMENT,
    /* 0x19 */ MSGMODE_SONG_DEMONSTRATION,
    /* 0x1A */ MSGMODE_SONG_DEMONSTRATION_DONE,
    /* 0x1B */ MSGMODE_SONG_PLAYBACK,
    /* 0x1C */ MSGMODE_SONG_PLAYBACK_SUCCESS,
    /* 0x1D */ MSGMODE_SONG_PLAYBACK_FAIL,
    /* 0x1E */ MSGMODE_SONG_PLAYBACK_NOTES_DROP,
    /* 0x1F */ MSGMODE_OCARINA_AWAIT_INPUT,
    /* 0x20 */ MSGMODE_UNK_20, // Never set and does nothing
    /* 0x21 */ MSGMODE_SCARECROW_LONG_RECORDING_START,
    /* 0x22 */ MSGMODE_SCARECROW_LONG_RECORDING_ONGOING,
    /* 0x23 */ MSGMODE_SCARECROW_LONG_PLAYBACK,
    /* 0x24 */ MSGMODE_SCARECROW_RECORDING_START,
    /* 0x25 */ MSGMODE_SCARECROW_RECORDING_ONGOING,
    /* 0x26 */ MSGMODE_SCARECROW_RECORDING_FAILED,
    /* 0x27 */ MSGMODE_SCARECROW_RECORDING_DONE,
    /* 0x28 */ MSGMODE_SCARECROW_PLAYBACK,
    /* 0x29 */ MSGMODE_MEMORY_GAME_START,
    /* 0x2A */ MSGMODE_MEMORY_GAME_LEFT_SKULLKID_PLAYING,
    /* 0x2B */ MSGMODE_MEMORY_GAME_LEFT_SKULLKID_WAIT,
    /* 0x2C */ MSGMODE_MEMORY_GAME_RIGHT_SKULLKID_PLAYING,
    /* 0x2D */ MSGMODE_MEMORY_GAME_RIGHT_SKULLKID_WAIT,
    /* 0x2E */ MSGMODE_MEMORY_GAME_PLAYER_PLAYING,
    /* 0x2F */ MSGMODE_MEMORY_GAME_ROUND_SUCCESS,
    /* 0x30 */ MSGMODE_MEMORY_GAME_START_NEXT_ROUND,
    /* 0x31 */ MSGMODE_FROGS_START,
    /* 0x32 */ MSGMODE_FROGS_PLAYING,
    /* 0x33 */ MSGMODE_FROGS_WAITING,
    /* 0x34 */ MSGMODE_TEXT_AWAIT_NEXT,
    /* 0x35 */ MSGMODE_TEXT_DONE,
    /* 0x36 */ MSGMODE_TEXT_CLOSING,
    /* 0x37 */ MSGMODE_PAUSED // Causes the message system to do nothing until external code sets a new message mode or calls a public function
} MessageMode;

typedef enum {
    /*  0 */ TEXT_STATE_NONE,
    /*  1 */ TEXT_STATE_DONE_HAS_NEXT,
    /*  2 */ TEXT_STATE_CLOSING,
    /*  3 */ TEXT_STATE_DONE_FADING,
    /*  4 */ TEXT_STATE_CHOICE,
    /*  5 */ TEXT_STATE_EVENT,
    /*  6 */ TEXT_STATE_DONE,
    /*  7 */ TEXT_STATE_SONG_DEMO_DONE,
    /*  8 */ TEXT_STATE_8,
    /*  9 */ TEXT_STATE_9,
    /* 10 */ TEXT_STATE_AWAITING_NEXT
} TextState;

#define TEXTBOX_ENDTYPE_DEFAULT     0x00
#define TEXTBOX_ENDTYPE_2_CHOICE    0x10
#define TEXTBOX_ENDTYPE_3_CHOICE    0x20
#define TEXTBOX_ENDTYPE_HAS_NEXT    0x30
#define TEXTBOX_ENDTYPE_PERSISTENT  0x40
#define TEXTBOX_ENDTYPE_EVENT       0x50
#define TEXTBOX_ENDTYPE_FADING      0x60

typedef struct {
    /* 0x0000 */ View   view;
    /* PC */ void*  textboxSegment; // original name: "fukidashiSegment"
    /* 0xE2B4 */ char   unk_E2B4[0x4];
    /* 0xE2B8 */ OcarinaStaff* ocarinaStaff; // original name : "info"
    /* 0xE2BC */ char   unk_E2BC[0x3C];
    /* 0xE2F8 */ uint16_t    textId;
    /* 0xE2FA */ uint16_t    choiceTextId;
    /* 0xE2FC */ uint8_t     textBoxProperties; // original name : "msg_disp_type"
    /* 0xE2FD */ uint8_t     textBoxType; // "Text Box Type"
    /* 0xE2FE */ uint8_t     textBoxPos; // text box position
    /* 0xE300 */ int32_t    msgLength; // original name : "msg_data"
    /* 0xE304 */ uint8_t     msgMode; // original name: "msg_mode"
    /* 0xE305 */ char   unk_E305[0x1];
    /* 0xE306 */ union {
                    uint8_t  msgBufDecoded[200];
                    uint16_t msgBufDecodedWide[100];
                 }; // decoded message buffer, may be smaller than this
    /* 0xE3CE */ uint16_t    msgBufPos; // original name : "rdp"
    /* 0xE3D0 */ uint16_t    unk_E3D0; // unused, only ever set to 0
    /* 0xE3D2 */ uint16_t    textDrawPos; // draw all decoded characters up to this buffer position
    /* 0xE3D4 */ uint16_t    decodedTextLen; // decoded message buffer length
    /* 0xE3D6 */ uint16_t    textUnskippable;
    /* 0xE3D8 */ int16_t    textPosX;
    /* 0xE3DA */ int16_t    textPosY;
    /* 0xE3DC */ int16_t    textColorR;
    /* 0xE3DE */ int16_t    textColorG;
    /* 0xE3E0 */ int16_t    textColorB;
    /* 0xE3E2 */ int16_t    textColorAlpha;
    /* 0xE3E4 */ uint8_t     textboxEndType; // original name : "select"
    /* 0xE3E5 */ uint8_t     choiceIndex;
    /* 0xE3E6 */ uint8_t     choiceNum; // textboxes that are not choice textboxes have a choiceNum of 1
    /* 0xE3E7 */ uint8_t     stateTimer;
    /* 0xE3E8 */ uint16_t    textDelayTimer;
    /* 0xE3EA */ uint16_t    textDelay;
    /* 0xE3EA */ uint16_t    lastPlayedSong; // original references : "Ocarina_Flog" , "Ocarina_Free"
    /* 0xE3EE */ uint16_t    ocarinaMode; // original name : "ocarina_mode"
    /* 0xE3F0 */ uint16_t    ocarinaAction; // original name : "ocarina_no"
    /* 0xE3F2 */ uint16_t    unk_E3F2; // this is like "lastPlayedSong" but set less often, original name : "chk_ocarina_no"
    /* 0xE3F4 */ uint16_t    unk_E3F4; // unused, only set to 0 in z_actor
    /* 0xE3F6 */ uint16_t    textboxBackgroundIdx;
    /* 0xE3F8 */ uint8_t     textboxBackgroundForeColorIdx;
    /* 0xE3F8 */ uint8_t     textboxBackgroundBackColorIdx;
    /* 0xE3F8 */ uint8_t     textboxBackgroundYOffsetIdx;
    /* 0xE3F8 */ uint8_t     textboxBackgroundUnkArg; // unused, set by the textbox background control character arguments
    /* 0xE3FC */ char   unk_E3FC[0x2];
    /* 0xE3FE */ int16_t    textboxColorRed;
    /* 0xE400 */ int16_t    textboxColorGreen;
    /* 0xE402 */ int16_t    textboxColorBlue;
    /* 0xE404 */ int16_t    textboxColorAlphaTarget;
    /* 0xE406 */ int16_t    textboxColorAlphaCurrent;
    /* 0xE408 */ Actor* talkActor;
    /* 0xE40C */ int16_t    disableWarpSongs; // warp song flag set by scene commands
    /* 0xE40E */ int16_t    unk_E40E; // ocarina related
    /* 0xE410 */ uint8_t     lastOcaNoteIdx;
} MessageContext; // size = 0xE418

typedef enum {
    /* 0x00 */ DO_ACTION_ATTACK,
    /* 0x01 */ DO_ACTION_CHECK,
    /* 0x02 */ DO_ACTION_ENTER,
    /* 0x03 */ DO_ACTION_RETURN,
    /* 0x04 */ DO_ACTION_OPEN,
    /* 0x05 */ DO_ACTION_JUMP,
    /* 0x06 */ DO_ACTION_DECIDE,
    /* 0x07 */ DO_ACTION_DIVE,
    /* 0x08 */ DO_ACTION_FASTER,
    /* 0x09 */ DO_ACTION_THROW,
    /* 0x0A */ DO_ACTION_NONE, // in do_action_static, the texture at this position is NAVI, however this value is in practice the "No Action" value
    /* 0x0B */ DO_ACTION_CLIMB,
    /* 0x0C */ DO_ACTION_DROP,
    /* 0x0D */ DO_ACTION_DOWN,
    /* 0x0E */ DO_ACTION_SAVE,
    /* 0x0F */ DO_ACTION_SPEAK,
    /* 0x10 */ DO_ACTION_NEXT,
    /* 0x11 */ DO_ACTION_GRAB,
    /* 0x12 */ DO_ACTION_STOP,
    /* 0x13 */ DO_ACTION_PUTAWAY,
    /* 0x14 */ DO_ACTION_REEL,
    /* 0x15 */ DO_ACTION_1,
    /* 0x16 */ DO_ACTION_2,
    /* 0x17 */ DO_ACTION_3,
    /* 0x18 */ DO_ACTION_4,
    /* 0x19 */ DO_ACTION_5,
    /* 0x1A */ DO_ACTION_6,
    /* 0x1B */ DO_ACTION_7,
    /* 0x1C */ DO_ACTION_8,
    /* 0x1D */ DO_ACTION_MAX
} DoAction;

typedef struct {
    /* 0x0000 */ View   view;
    /* 0x0128 */ Vtx*   actionVtx;
    /* 0x012C */ Vtx*   beatingHeartVtx;
    /* 0x0130 */ uint8_t*    parameterSegment;
    /* 0x0134 */ uintptr_t removedActionLabelSegment;
    /* 0x0138 */ uint8_t*    iconItemSegment;
    /* 0x013C */ char** mapSegment;
    /* 0x0140 */ uint8_t     mapPalette[32];
    /* 0x0160 */ DmaRequest dmaRequest_160;
    /* 0x0180 */ DmaRequest dmaRequest_180;
    /* 0x01A0 */ char   unk_1A0[0x20];
    /* 0x01C0 */ OSMesgQueue loadQueue;
    /* 0x01D8 */ OSMesg loadMsg;
    /* 0x01DC */ Viewport viewport;
    /* 0x01EC */ int16_t    unk_1EC;
    /* 0x01EE */ uint16_t    unk_1EE;
    /* 0x01F0 */ uint16_t    unk_1F0;
    /* 0x01F4 */ float    unk_1F4;
    /* 0x01F8 */ int16_t    naviCalling;
    /* 0x01FA */ int16_t    unk_1FA;
    /* 0x01FC */ int16_t    unk_1FC;
    /* 0x01FE */ int16_t    unk_1FE;
    /* 0x0200 */ int16_t    unk_200;
    /* 0x0202 */ int16_t    beatingHeartPrim[3];
    /* 0x0208 */ int16_t    beatingHeartEnv[3];
    /* 0x020E */ int16_t    heartsPrimR[2];
    /* 0x0212 */ int16_t    heartsPrimG[2];
    /* 0x0216 */ int16_t    heartsPrimB[2];
    /* 0x021A */ int16_t    heartsEnvR[2];
    /* 0x021E */ int16_t    heartsEnvG[2];
    /* 0x0222 */ int16_t    heartsEnvB[2];
    /* 0x0226 */ int16_t    unk_226;
    /* 0x0228 */ int16_t    unk_228;
    /* 0x022A */ int16_t    unk_22A;
    /* 0x022C */ int16_t    unk_22C;
    /* 0x022E */ int16_t    unk_22E;
    /* 0x0230 */ int16_t    unk_230;
    /* 0x0232 */ int16_t    counterDigits[4]; // used for key and rupee counters
    /* 0x023A */ uint8_t     numHorseBoosts;
    /* 0x023C */ uint16_t    unk_23C;
    /* 0x023E */ uint16_t    hbaAmmo; // ammo while playing the horseback archery minigame
    /* 0x0240 */ uint16_t    unk_240;
    /* 0x0242 */ uint16_t    unk_242;
    /* 0x0224 */ uint16_t    unk_244; // screen fill alpha?
    /* 0x0246 */ uint16_t    aAlpha; // also carrots alpha
    /* 0x0248 */ uint16_t    bAlpha; // also HBA score alpha
    /* 0x024A */ uint16_t    cLeftAlpha;
    /* 0x024C */ uint16_t    cDownAlpha;
    /* 0x024E */ uint16_t    cRightAlpha;
    /* 0x0250 */ uint16_t    healthAlpha; // also max C-Up alpha
    /* 0x0252 */ uint16_t    counterAlpha;
    /* 0x0254 */ uint16_t    minimapAlpha;
    /* 0x0256 */ int16_t    startAlpha;
    /* 0x0258 */ int16_t    unk_258;
    /* 0x025A */ int16_t    unk_25A;
    /* 0x025C */ int16_t    mapRoomNum;
    /* 0x025E */ int16_t    mapPaletteIndex; // "map_palete_no"
    /* 0x0260 */ uint8_t     unk_260;
    /* 0x0261 */ uint8_t     unk_261;
    // #region SOH [General]
    /*        */ char* mapSegmentName[2]; // Tracks the map segment texture by OTR sig name
    /*        */ uint8_t mapPalettesPulse[40][32]; // Used to have unique pointers per map pulse color for the shader backend. 40 for map pulse timer x2
    // #endregion
} InterfaceContext; // size = 0x270


typedef enum {
    /* 00 */ GAMEOVER_INACTIVE,
    /* 01 */ GAMEOVER_DEATH_START,
    /* 02 */ GAMEOVER_DEATH_WAIT_GROUND, // wait for link to fall and hit the ground
    /* 03 */ GAMEOVER_DEATH_DELAY_MENU, // wait for 1 second before showing the game over menu
    /* 04 */ GAMEOVER_DEATH_WAIT_RESPAWN // freeze the final death frame until multiplayer respawn
} GameOverState;

typedef struct {
    /* 0x00 */ uint16_t state;
} GameOverContext; // size = 0x2

typedef struct {
    /* 0x00 */ int16_t      id;
    /* 0x04 */ void*    segment;
    /* 0x08 */ DmaRequest  dmaRequest;
    /* 0x28 */ OSMesgQueue loadQueue;
    /* 0x40 */ OSMesg   loadMsg;
} ObjectStatus; // size = 0x44

typedef struct {
    /* 0x0000 */ void*  spaceStart;
    /* 0x0004 */ void*  spaceEnd; // original name: "endSegment"
    /* 0x0008 */ uint8_t     num; // number of objects in bank
    /* 0x0009 */ uint8_t     unk_09;
    /* 0x000A */ uint8_t     mainKeepIndex; // "gameplay_keep" index in bank
    /* 0x000B */ uint8_t     subKeepIndex; // "gameplay_field_keep" or "gameplay_dangeon_keep" index in bank
    /* 0x000C */ ObjectStatus status[OBJECT_EXCHANGE_BANK_MAX];
} ObjectContext; // size = 0x518

typedef struct {
    /* 0x00 */ Gfx* opa;
    /* 0x04 */ Gfx* xlu;
} PolygonDlist; // size = 0x8


#ifdef __cplusplus
#define Polygon _Polygon
#endif

typedef struct {
    /* 0x00 */ uint8_t    type;
} PolygonBase;

typedef struct {
    /* 0x00 */ PolygonBase base;
    /* 0x01 */ uint8_t    num; // number of dlist entries
    /* 0x04 */ void* start;
    /* 0x08 */ void* end;
} PolygonType0; // size = 0xC

typedef union {
    PolygonBase  base;
    PolygonType0 polygon0;
} MeshHeader; // "Ground Shape"

typedef enum {
    /* 0 */ LENS_MODE_HIDE_ACTORS, // lens actors are visible by default, and hidden by using lens (for example, fake walls)
    /* 1 */ LENS_MODE_SHOW_ACTORS // lens actors are invisible by default, and shown by using lens (for example, invisible enemies)
} LensMode;

typedef enum {
    /* 0 */ ROOM_BEHAVIOR_TYPE1_0,
    /* 1 */ ROOM_BEHAVIOR_TYPE1_1,
    /* 2 */ ROOM_BEHAVIOR_TYPE1_2,
    /* 3 */ ROOM_BEHAVIOR_TYPE1_3, // unused
    /* 4 */ ROOM_BEHAVIOR_TYPE1_4, // unused
    /* 5 */ ROOM_BEHAVIOR_TYPE1_5
} RoomBehaviorType1;

typedef enum {
    /* 0 */ ROOM_BEHAVIOR_TYPE2_0,
    /* 1 */ ROOM_BEHAVIOR_TYPE2_1,
    /* 2 */ ROOM_BEHAVIOR_TYPE2_2,
    /* 3 */ ROOM_BEHAVIOR_TYPE2_3,
    /* 4 */ ROOM_BEHAVIOR_TYPE2_4,
    /* 5 */ ROOM_BEHAVIOR_TYPE2_5,
    /* 6 */ ROOM_BEHAVIOR_TYPE2_6
} RoomBehaviorType2;

typedef struct {
    /* 0x00 */ int8_t   num;
    /* 0x01 */ uint8_t   unk_01;
    /* 0x02 */ uint8_t   behaviorType2;
    /* 0x03 */ uint8_t   behaviorType1;
    /* 0x04 */ int8_t   echo;
    /* 0x05 */ uint8_t   lensMode;
    /* 0x08 */ MeshHeader* meshHeader; // original name: "ground_shape"
    /* 0x0C */ void* segment;
    /* 0x10 */ char unk_10[0x4];
} Room; // size = 0x14

typedef struct {
    /* 0x00 */ Room  curRoom;
    /* 0x14 */ Room  prevRoom;
    /* 0x28 */ void* bufPtrs[2];
    /* 0x30 */ uint8_t    unk_30;
    /* 0x31 */ int8_t    status;
    /* 0x34 */ void* unk_34;
    /* 0x38 */ DmaRequest dmaRequest;
    /* 0x58 */ OSMesgQueue loadQueue;
    /* 0x70 */ OSMesg loadMsg;
    /* 0x74 */ int16_t unk_74[2]; // context-specific data used by the current scene draw config
    void* roomToLoad;
} RoomContext; // size = 0x78

typedef struct {
    /* 0x000 */ int16_t colATCount;
    /* 0x002 */ uint16_t sacFlags;
    /* 0x004 */ Collider* colAT[COLLISION_CHECK_AT_MAX];
    /* 0x0CC */ int32_t colACCount;
    /* 0x0D0 */ Collider* colAC[COLLISION_CHECK_AC_MAX];
    /* 0x1C0 */ int32_t colOCCount;
    /* 0x1C4 */ Collider* colOC[COLLISION_CHECK_OC_MAX];
    /* 0x28C */ int32_t colLineCount;
    /* 0x290 */ OcLine* colLine[COLLISION_CHECK_OC_LINE_MAX];
} CollisionCheckContext; // size = 0x29C

typedef struct ListAlloc {
    /* 0x00 */ struct ListAlloc* prev;
    /* 0x04 */ struct ListAlloc* next;
} ListAlloc; // size = 0x8

#define TRANS_TRIGGER_OFF 0 // transition is not active
#define TRANS_TRIGGER_START 20 // start transition (exiting an area)
#define TRANS_TRIGGER_END -20 // transition is ending (arriving in a new area)

typedef enum {
    TRANS_MODE_OFF,
    TRANS_MODE_SETUP,
    TRANS_MODE_INSTANCE_INIT,
    TRANS_MODE_INSTANCE_RUNNING,
} TransitionMode;

typedef enum {
    TRANS_TYPE_FADE_BLACK = 2,
} TransitionType;

#define TRANS_NEXT_TYPE_DEFAULT 0xFF // when `nextTransitionType` is set to default, the type will be taken from the entrance table for the ending transition

typedef struct {
    union {
        TransitionFade fade;
        char data[sizeof(TransitionFade)];
    };
    /* 0x228 */ int32_t   transitionType;
    /* 0x22C */ void* (*init)(void* transition);
    /* 0x230 */ void  (*destroy)(void* transition);
    /* 0x234 */ void  (*update)(void* transition, int32_t updateRate);
    /* 0x238 */ void  (*draw)(void* transition, Gfx** gfxP);
    /* 0x23C */ void  (*start)(void* transition);
    /* 0x240 */ void  (*setType)(void* transition, int32_t type);
    /* 0x244 */ void  (*setColor)(void* transition, uint32_t color);
    int32_t (*isDone)(void* transition);
} TransitionContext;

typedef struct {
    /* 0x00 */ int16_t   id;
    /* 0x02 */ Vec3s pos;
    /* 0x08 */ Vec3s rot;
    /* 0x0E */ int16_t   params;
} ActorEntry; // size = 0x10

typedef struct {
    /* 0x00 */ uint8_t spawn;
    /* 0x01 */ uint8_t room;
} EntranceEntry;

#define SRAM_SIZE 0x8000
#define SRAM_HEADER_SIZE 0x10

typedef enum {
    /* 0x00 */ SRAM_HEADER_SOUND,
    /* 0x01 */ SRAM_HEADER_ZTARGET,
    /* 0x02 */ SRAM_HEADER_LANGUAGE,
    /* 0x03 */ SRAM_HEADER_MAGIC // must be the value of `sZeldaMagic` for save to be considered valid
} SramHeaderField;

typedef struct GameAllocEntry {
    /* 0x00 */ struct GameAllocEntry* next;
    /* 0x04 */ struct GameAllocEntry* prev;
    /* 0x08 */ size_t size;
    /* 0x0C */ uint32_t unk_0C;
} GameAllocEntry; // size = 0x10

typedef struct {
    /* 0x00 */ GameAllocEntry base;
    /* 0x10 */ GameAllocEntry* head;
} GameAlloc; // size = 0x14

struct GameState;

typedef void (*GameStateFunc)(struct GameState* gameState);

typedef struct GameState {
    /* 0x00 */ GraphicsContext* gfxCtx;
    /* 0x04 */ GameStateFunc main;
    /* 0x08 */ GameStateFunc destroy; // "cleanup"
    /* 0x0C */ GameStateFunc init;
    /* 0x10 */ size_t size;
    /* 0x14 */ Input input[4];
    /* 0x74 */ TwoHeadArena tha;
    /* 0x84 */ GameAlloc alloc;
    /* 0x98 */ uint32_t running;
    /* 0x9C */ uint32_t frames;
    /* 0xA0 */ uint32_t unk_A0;
} GameState; // size = 0xA4


// Global Context (dbg ram start: 80212020)
typedef struct PlayState {
    /* 0x00000 */ GameState state;
    /* 0x000A4 */ int16_t sceneNum;
    /* 0x000A6 */ uint8_t sceneConfig;
    /* 0x000A7 */ char unk_A7[0x9];
    /* 0x000B0 */ void* sceneSegment;
    /* 0x000B8 */ View view;
    /* 0x001E0 */ Camera mainCamera;
    /* 0x0034C */ Camera subCameras[NUM_CAMS - SUBCAM_FIRST];
    /* 0x00790 */ Camera* cameraPtrs[NUM_CAMS];
    /* 0x007A0 */ int16_t activeCamera;
    /* 0x007A2 */ int16_t nextCamera;
    /* 0x007A4 */ SequenceContext sequenceCtx;
    /* 0x007A8 */ LightContext lightCtx;
    /* 0x007B8 */ FrameAdvanceContext frameAdvCtx;
    /* 0x007C0 */ CollisionContext colCtx;
    /* 0x01C24 */ ActorContext actorCtx;
    /* 0x01D64 */ PlayerActionContext playerActionCtx;
    /* 0x01DB4 */ SoundSource soundSources[16];
    /* 0x01F78 */ SkyboxContext skyboxCtx;
    /* 0x020D8 */ MessageContext msgCtx; // "message"
    /* 0x104F0 */ InterfaceContext interfaceCtx; // "parameter"
    /* 0x10A20 */ GameOverContext gameOverCtx;
    /* 0x10A24 */ EnvironmentContext envCtx;
    /* 0x10B20 */ AnimationContext animationCtx;
    /* 0x117A4 */ ObjectContext objectCtx;
    /* 0x11CBC */ RoomContext roomCtx;
    /* 0x11D3C */ void (*playerInit)(Player* player, struct PlayState* play, FlexSkeletonHeader* skelHeader);
    /* 0x11D40 */ void (*playerUpdate)(Player* player, struct PlayState* play, Input* input);
    /* 0x11D44 */ int32_t (*isPlayerDroppingFish)(struct PlayState* play);
    /* 0x11D48 */ int32_t (*startPlayerFishing)(struct PlayState* play);
    /* 0x11D4C */ int32_t (*grabPlayer)(struct PlayState* play, Player* player);
    /* 0x11D50 */ int32_t (*startPlayerCutscene)(struct PlayState* play, Actor* actor, int32_t mode);
    /* 0x11D54 */ void (*func_11D54)(Player* player, struct PlayState* play);
    /* 0x11D58 */ int32_t (*damagePlayer)(struct PlayState* play, int32_t damage);
    /* 0x11D5C */ void (*talkWithPlayer)(struct PlayState* play, Actor* actor);
    /* 0x11D60 */ MtxF viewProjectionMtxF;
    /* 0x11DA0 */ MtxF billboardMtxF;
    /* 0x11DE0 */ Mtx* billboardMtx;
    /* 0x11DE4 */ uint32_t gameplayFrames;
    /* 0x11DE8 */ uint8_t linkAgeOnLoad;
    /* 0x11DE9 */ uint8_t unk_11DE9;
    /* 0x11DEA */ uint8_t curSpawn;
    /* 0x11DEC */ uint8_t numRooms;
    /* 0x11DF0 */ RomFile* roomList;
    /* 0x11DF4 */ ActorEntry* linkActorEntry;
    /* 0x11DFC */ void* unk_11DFC;
    /* 0x11E00 */ EntranceEntry* setupEntranceList;
    /* 0x11E10 */ void* specialEffects;
    /* 0x11E14 */ uint8_t skyboxId;
    /* 0x11E15 */ int8_t transitionTrigger; // "fade_direction"
    /* 0x11E16 */ int16_t unk_11E16;
    /* 0x11E18 */ int16_t unk_11E18;
    /* 0x11E1A */ int16_t nextEntranceIndex;
    /* 0x11E1C */ char unk_11E1C[0x40];
    /* 0x11E5C */ int8_t shootingGalleryStatus;
    /* 0x11E5D */ int8_t bombchuBowlingStatus; // "bombchu_game_flag"
    /* 0x11E5E */ uint8_t transitionType;
    /* 0x11E60 */ CollisionCheckContext colChkCtx;
    /* 0x120FC */ uint16_t envFlags[20];
    /* 0x12174 */ char unk_12174[0x53];
    /* 0x121C7 */ int8_t unk_121C7;
    /* 0x121C8 */ TransitionContext transitionCtx;
    /* 0x12418 */ char unk_12418[0x3];
    /* 0x1241B */ uint8_t transitionMode; // "fbdemo_wipe_modem"
    /* 0x1241C */ TransitionFade transitionFade;
    /* 0x12428 */ char unk_12428[0x3];
    /* 0x1242B */ uint8_t unk_1242B;
    /* 0x1242C */ SceneTableEntry* loadedScene;
    /* 0x12430 */ char unk_12430[0xE8];
    // SOH [Custom Models] MTX tracker for flex based skeletons
    Mtx** flexLimbOverrideMTX;
} PlayState; // size = 0x12518


// Macros for `EntranceInfo.field`
#define ENTRANCE_INFO_CONTINUE_BGM_FLAG (1 << 15)
#define ENTRANCE_INFO_DISPLAY_TITLE_CARD_FLAG (1 << 14)
#define ENTRANCE_INFO_END_TRANS_TYPE_MASK 0x3F80
#define ENTRANCE_INFO_END_TRANS_TYPE_SHIFT 7
#define ENTRANCE_INFO_END_TRANS_TYPE(field)          \
    (((field) >> ENTRANCE_INFO_END_TRANS_TYPE_SHIFT) \
     & (ENTRANCE_INFO_END_TRANS_TYPE_MASK >> ENTRANCE_INFO_END_TRANS_TYPE_SHIFT))
#define ENTRANCE_INFO_START_TRANS_TYPE_MASK 0x7F
#define ENTRANCE_INFO_START_TRANS_TYPE_SHIFT 0
#define ENTRANCE_INFO_START_TRANS_TYPE(field)          \
    (((field) >> ENTRANCE_INFO_START_TRANS_TYPE_SHIFT) \
     & (ENTRANCE_INFO_START_TRANS_TYPE_MASK >> ENTRANCE_INFO_START_TRANS_TYPE_SHIFT))

typedef enum {
    DPM_UNK = 0,
    DPM_PLAYER = 1,
    DPM_ENEMY = 2,
    DPM_UNK3 = 3
} DynaPolyMoveFlag;

typedef struct {
    /* 0x00 */ AnimationHeader* animation;
    /* 0x04 */ float              playSpeed;
    /* 0x08 */ float              startFrame;
    /* 0x0C */ float              frameCount;
    /* 0x10 */ uint8_t               mode;
    /* 0x14 */ float              morphFrames;
} AnimationInfo; // size = 0x18

typedef struct {
    /* 0x00 */ AnimationHeader* animation;
    /* 0x04 */ float              frameCount;
    /* 0x08 */ uint8_t               mode;
    /* 0x0C */ float              morphFrames;
} AnimationFrameCountInfo; // size = 0x10

typedef struct {
    /* 0x00 */ AnimationHeader* animation;
    /* 0x04 */ float playSpeed;
    /* 0x08 */ uint8_t mode;
    /* 0x0C */ float morphFrames;
} AnimationSpeedInfo; // size = 0x10

typedef struct {
    /* 0x00 */ AnimationHeader* animation;
    /* 0x04 */ uint8_t mode;
    /* 0x08 */ float morphFrames;
} AnimationMinimalInfo; // size = 0xC

typedef struct {
    /* 0x00 */ int8_t  scene;
    /* 0x01 */ int8_t  spawn;
    /* 0x02 */ uint16_t field;
} EntranceInfo; // size = 0x4

typedef struct {
    /* 0x00 */ void*     loadedRamAddr;
    /* 0x04 */ uint32_t       vromStart; // if applicable
    /* 0x08 */ uint32_t       vromEnd;   // if applicable
    /* 0x0C */ void*     vramStart; // if applicable
    /* 0x10 */ void*     vramEnd;   // if applicable
    /* 0x14 */ UNK_PTR   unk_14;
    /* 0x18 */ void*     init;    // initializes and executes the given context
    /* 0x1C */ void*     destroy; // deconstructs the context, and sets the next context to load
    /* 0x20 */ UNK_PTR   unk_20;
    /* 0x24 */ UNK_PTR   unk_24;
    /* 0x28 */ UNK_TYPE4 unk_28;
    /* 0x2C */ uint32_t       instanceSize;
} GameStateOverlay; // size = 0x30

typedef struct PreNMIContext {
    /* 0x00 */ GameState state;
    /* 0xA4 */ uint32_t       timer;
    /* 0xA8 */ UNK_TYPE4 unk_A8;
} PreNMIContext; // size = 0xAC

typedef enum {
    MTXMODE_NEW,  // generates a new matrix
    MTXMODE_APPLY // applies transformation to the current matrix
} MatrixMode;

typedef struct FaultClient {
    /* 0x00 */ struct FaultClient* next;
    /* 0x04 */ uint32_t callback;
    /* 0x08 */ uint32_t param1;
    /* 0x0C */ uint32_t param2;
} FaultClient; // size = 0x10

typedef struct FaultAddrConvClient {
    /* 0x00 */ struct FaultAddrConvClient* next;
    /* 0x04 */ uint32_t callback;
    /* 0x08 */ uint32_t param;
} FaultAddrConvClient; // size = 0xC


typedef struct {
    /* 0x00 */ uint32_t (*callback)(uint32_t, uint32_t);
    /* 0x04 */ uint32_t param0;
    /* 0x08 */ uint32_t param1;
    /* 0x0C */ uint32_t ret;
    /* 0x10 */ OSMesgQueue* queue;
    /* 0x14 */ OSMesg msg;
} FaultClientContext; // size = 0x18

typedef struct FaultThreadStruct {
    /* 0x000 */ OSThread thread;
    /* 0x1B0 */ uint8_t unk_1B0[0x600];
    /* 0x7B0 */ OSMesgQueue queue;
    /* 0x7C8 */ OSMesg msg;
    /* 0x7CC */ uint8_t exitDebugger;
    /* 0x7CD */ uint8_t msgId;
    /* 0x7CE */ uint8_t faultHandlerEnabled;
    /* 0x7CF */ uint8_t faultActive;
    /* 0x7D0 */ OSThread* faultedThread;
    /* 0x7D4 */ void(*padCallback)(Input*);
    /* 0x7D8 */ FaultClient* clients;
    /* 0x7DC */ FaultAddrConvClient* addrConvClients;
    /* 0x7E0 */ uint8_t unk_7E0[4];
    /* 0x7E4 */ Input padInput;
    /* 0x7FC */ uint16_t colors[36];
    /* 0x844 */ void* fb;
    /* 0x848 */ uint32_t currClientThreadSp;
    /* 0x84C */ uint8_t unk_84C[4];
} FaultThreadStruct; // size = 0x850

typedef struct {
    /* 0x00 */ uint16_t* fb;
    /* 0x04 */ uint16_t w;
    /* 0x08 */ uint16_t h;
    /* 0x0A */ uint16_t yStart;
    /* 0x0C */ uint16_t yEnd;
    /* 0x0E */ uint16_t xStart;
    /* 0x10 */ uint16_t xEnd;
    /* 0x12 */ uint16_t foreColor;
    /* 0x14 */ uint16_t backColor;
    /* 0x14 */ uint16_t cursorX;
    /* 0x16 */ uint16_t cursorY;
    /* 0x18 */ const uint32_t* fontData;
    /* 0x1C */ uint8_t charW;
    /* 0x1D */ uint8_t charH;
    /* 0x1E */ int8_t charWPad;
    /* 0x1F */ int8_t charHPad;
    /* 0x20 */ uint16_t printColors[10];
    /* 0x34 */ uint8_t escCode; // bool
    /* 0x35 */ uint8_t osSyncPrintfEnabled;
    /* 0x38 */ void(*inputCallback)();
} FaultDrawer; // size = 0x3C

typedef struct {
    /* 0x00 */ PrintCallback callback;
    /* 0x04 */ Gfx* dList;
    /* 0x08 */ uint16_t posX;
    /* 0x0A */ uint16_t posY;
    /* 0x0C */ uint16_t baseX;
    /* 0x0E */ uint8_t baseY;
    /* 0x0F */ uint8_t flags;
    /* 0x10 */ Color_RGBA8_u32 color;
    /* 0x14 */ char unk_14[0x1C]; // unused
} GfxPrint; // size = 0x30

#define GFXP_FLAG_SHADOW   (1 << 2)
#define GFXP_FLAG_UPDATE   (1 << 3)
#define GFXP_FLAG_OPEN     (1 << 7)

typedef struct StackEntry {
    /* 0x00 */ struct StackEntry* next;
    /* 0x04 */ struct StackEntry* prev;
    /* 0x08 */ uintptr_t head;
    /* 0x0C */ uintptr_t tail;
    /* 0x10 */ uint32_t initValue;
    /* 0x14 */ int32_t minSpace;
    /* 0x18 */ const char* name;
} StackEntry;

typedef enum {
    STACK_STATUS_OK = 0,
    STACK_STATUS_WARNING = 1,
    STACK_STATUS_OVERFLOW = 2
} StackStatus;

typedef struct {
    /* 0x00 */ uint32_t magic; // IS64
    /* 0x04 */ uint32_t get;
    /* 0x08 */ uint8_t unk_08[0x14-0x08];
    /* 0x14 */ uint32_t put;
    /* 0x18 */ uint8_t unk_18[0x20-0x18];
    /* 0x20 */ uint8_t data[0x10000-0x20];
} ISVDbg;

typedef struct {
    /* 0x00 */ char name[0x18];
    /* 0x18 */ uint32_t mediaFormat;
    /* 0x1C */ union {
        struct {
            uint16_t cartId;
            uint8_t countryCode;
            uint8_t version;
        };
        uint32_t regionInfo;
    };
} LocaleCartInfo; // size = 0x20

typedef struct {
    /* 0x00 */ char magic[4]; // Yaz0
    /* 0x04 */ uint32_t decSize;
    /* 0x08 */ uint32_t compInfoOffset; // only used in mio0
    /* 0x0C */ uint32_t uncompDataOffset; // only used in mio0
    /* 0x10 */ uint32_t data[1];
} Yaz0Header; // size = 0x10 ("data" is not part of the header)

typedef struct {
    /* 0x00 */ int16_t type;
    /* 0x02 */ char  misc[0x1E];
} OSScMsg; // size = 0x20

typedef struct IrqMgrClient {
    /* 0x00 */ struct IrqMgrClient* prev;
    /* 0x04 */ OSMesgQueue* queue;
} IrqMgrClient;

typedef struct {
    /* 0x000 */ OSScMsg retraceMsg; // this apparently got moved from OSSched
    /* 0x020 */ OSScMsg prenmiMsg; // this apparently got moved from OSSched
    /* 0x040 */ OSScMsg nmiMsg;
    /* 0x060 */ OSMesgQueue queue;
    /* 0x078 */ OSMesg msgBuf[8];
    /* 0x098 */ OSThread thread;
    /* 0x248 */ IrqMgrClient* clients;
    /* 0x24C */ uint8_t resetStatus;
    /* 0x250 */ OSTime resetTime;
    /* 0x258 */ OSTimer timer;
    /* 0x278 */ OSTime retraceTime;
} IrqMgr; // size = 0x280

typedef struct PadMgr {
    /* 0x0000 */ OSContStatus padStatus[4];
    /* 0x0010 */ OSMesg serialMsgBuf[1];
    /* 0x0014 */ OSMesg lockMsgBuf[1];
    /* 0x0018 */ OSMesg interruptMsgBuf[4];
    /* 0x0028 */ OSMesgQueue serialMsgQ;
    /* 0x0040 */ OSMesgQueue lockMsgQ;
    /* 0x0058 */ OSMesgQueue interruptMsgQ;
    /* 0x0070 */ IrqMgrClient irqClient;
    /* 0x0078 */ IrqMgr* irqMgr;
    /* 0x0080 */ OSThread thread;
    /* 0x0230 */ Input inputs[4];
    /* 0x0290 */ OSContPad pads[4];
    /* 0x02A8 */ volatile uint8_t validCtrlrsMask;
    /* 0x02A9 */ uint8_t nControllers;
    /* 0x02AA */ uint8_t ctrlrIsConnected[4]; // "Key_switch" originally
    /* 0x02AE */ uint8_t pakType[4]; // 1 if rumble pack, 2 if mempak?
    /* 0x02B2 */ volatile uint8_t rumbleEnable[4];
    /* 0x02B6 */ uint8_t rumbleCounter[4]; // not clear exact meaning
    /* 0x02BC */ OSPfs pfs[4];
    /* 0x045C */ volatile uint8_t rumbleOffFrames;
    /* 0x045D */ volatile uint8_t rumbleOnFrames;
    /* 0x045E */ uint8_t preNMIShutdown;
    /* 0x0460 */ void (*retraceCallback)(struct PadMgr* padmgr, int32_t unk464);
    /* 0x0464 */ uint32_t retraceCallbackValue;
} PadMgr; // size = 0x468

// == Previously sched.h

#define OS_SC_NEEDS_RDP         0x0001
#define OS_SC_NEEDS_RSP         0x0002
#define OS_SC_DRAM_DLIST        0x0004
#define OS_SC_PARALLEL_TASK     0x0010
#define OS_SC_LAST_TASK         0x0020
#define OS_SC_SWAPBUFFER        0x0040

#define OS_SC_RCP_MASK          0x0003
#define OS_SC_TYPE_MASK         0x0007

typedef struct {
    /* 0x0000 */ uint16_t*   curBuffer;
    /* 0x0004 */ uint16_t*   nextBuffer;
} FrameBufferSwap;

typedef struct {
    /* 0x0000 */ OSMesgQueue  interruptQ;
    /* 0x0018 */ OSMesg       intBuf[8];
    /* 0x0038 */ OSMesgQueue  cmdQ;
    /* 0x0050 */ OSMesg       cmdMsgBuf[8];
    /* 0x0070 */ OSThread     thread;
    /* 0x0220 */ OSScTask*    audioListHead;
    /* 0x0224 */ OSScTask*    gfxListHead;
    /* 0x0228 */ OSScTask*    audioListTail;
    /* 0x022C */ OSScTask*    gfxListTail;
    /* 0x0230 */ OSScTask*    curRSPTask;
    /* 0x0234 */ OSScTask*    curRDPTask;
    /* 0x0238 */ int32_t          retraceCnt;
    /* 0x023C */ int32_t          doAudio;
    /* 0x0240 */ CfbInfo*     curBuf;
    /* 0x0244 */ CfbInfo*     pendingSwapBuf1;
    /* 0x0220 */ CfbInfo*     pendingSwapBuf2;
    /* 0x0220 */ UNK_TYPE4    unk_24C;
    /* 0x0250 */ IrqMgrClient irqClient;
} SchedContext; // size = 0x258

// ========================

#define OS_SC_RETRACE_MSG       1
#define OS_SC_DONE_MSG          2
#define OS_SC_NMI_MSG           3 // name is made up, 3 is OS_SC_RDP_DONE_MSG in the original sched.c
#define OS_SC_PRE_NMI_MSG       4

#define OS_SC_DP                0x0001
#define OS_SC_SP                0x0002
#define OS_SC_YIELD             0x0010
#define OS_SC_YIELDED           0x0020

typedef struct {
    /* 0x0000 */ IrqMgr*       irqMgr;
    /* 0x0004 */ SchedContext* sched;
    /* 0x0008 */ OSScTask      audioTask;
    /* 0x0060 */ char          unk_60[0x10];
    /* 0x0070 */ AudioTask*    rspTask;
    /* 0x0074 */ OSMesgQueue   unk_74;
    /* 0x008C */ OSMesg        unk_8C;
    /* 0x0090 */ OSMesgQueue   unk_90;
    /* 0x00A8 */ OSMesg        unk_A8;
    /* 0x00AC */ OSMesgQueue   unk_AC;
    /* 0x00C4 */ OSMesg        unk_C4;
    /* 0x00C8 */ OSMesgQueue   unk_C8;
    /* 0x00E0 */ OSMesg        unk_E0;
    /* 0x00E4 */ char          unk_E4[0x04];
    /* 0x00E8 */ OSThread      unk_E8;
} AudioMgr; // size = 0x298

struct ArenaNode;

typedef struct Arena {
    /* 0x00 */ struct ArenaNode* head;
    /* 0x04 */ void* start;
    /* 0x08 */ OSMesgQueue lock;
    /* 0x20 */ uint8_t unk_20;
    /* 0x21 */ uint8_t isInit;
    /* 0x22 */ uint8_t flag;
} Arena; // size = 0x24

typedef struct ArenaNode {
    /* 0x00 */ int16_t magic;
    /* 0x02 */ int16_t isFree;
    /* 0x04 */ size_t size;
    /* 0x08 */ struct ArenaNode* next;
    /* 0x0C */ struct ArenaNode* prev;
    // /* 0x10 */ const char* filename;
    // /* 0x14 */ int32_t line;
    // /* 0x18 */ OSId threadId;
    // /* 0x1C */ Arena* arena;
    // /* 0x20 */ OSTime time;
    // /* 0x28 */ uint8_t unk_28[0x30-0x28]; // probably padding
} ArenaNode; // size = 0x30

typedef struct OverlayRelocationSection {
    /* 0x00 */ uint32_t textSize;
    /* 0x04 */ uint32_t dataSize;
    /* 0x08 */ uint32_t rodataSize;
    /* 0x0C */ uint32_t bssSize;
    /* 0x10 */ uint32_t nRelocations;
    /* 0x14 */ uint32_t relocations[1];
} OverlayRelocationSection; // size >= 0x18

typedef struct {
    /* 0x00 */ uint32_t resetting;
    /* 0x04 */ uint32_t resetCount;
    /* 0x08 */ OSTime duration;
    /* 0x10 */ OSTime resetTime;
} PreNmiBuff; // size = 0x18 (actually osAppNmiBuffer is 0x40 bytes large but the rest is unused)

typedef struct {
    /* 0x00 */ int16_t unk_00;
    /* 0x02 */ int16_t unk_02;
    /* 0x04 */ int16_t unk_04;
} SubQuakeRequest14;

typedef struct {
    /* 0x00 */ int16_t randIdx;
    /* 0x02 */ int16_t countdownMax;
    /* 0x04 */ Camera* cam;
    /* 0x08 */ uint32_t callbackIdx;
    /* 0x0C */ int16_t y;
    /* 0x0E */ int16_t x;
    /* 0x10 */ int16_t zoom;
    /* 0x12 */ int16_t rotZ;
    /* 0x14 */ SubQuakeRequest14 unk_14;
    /* 0x1A */ int16_t speed;
    /* 0x1C */ int16_t unk_1C;
    /* 0x1E */ int16_t countdown;
    /* 0x20 */ int16_t camPtrIdx;
} QuakeRequest; // size = 0x24

typedef struct {
    /* 0x00 */ Vec3f vec1;
    /* 0x0C */ Vec3f vec2;
    /* 0x18 */ int16_t rotZ;
    /* 0x1A */ int16_t unk_1A;
    /* 0x1C */ int16_t zoom;
} ShakeInfo; // size = 0x1E

typedef struct {
    /* 0x00 */ Vec3f atOffset;
    /* 0x0C */ Vec3f eyeOffset;
    /* 0x18 */ int16_t rotZ;
    /* 0x1A */ int16_t unk_1A;
    /* 0x1C */ int16_t zoom;
    /* 0x20 */ float unk_20;
} QuakeCamCalc; // size = 0x24


#define UCODE_NULL      0
#define UCODE_F3DZEX    1
#define UCODE_UNK       2
#define UCODE_S2DEX     3

typedef struct {
    /* 0x00 */ uint32_t type;
    /* 0x04 */ void* ptr;
} UCodeInfo; // size = 0x8

typedef struct {
    /* 0x00 */ uint32_t segments[NUM_SEGMENTS];
    /* 0x40 */ Gfx* dlStack[18];
    /* 0x88 */ int32_t dlDepth;
    /* 0x8C */ uint32_t dlCnt;
    /* 0x90 */ uint32_t vtxCnt;
    /* 0x94 */ uint32_t spvtxCnt;
    /* 0x98 */ uint32_t tri1Cnt;
    /* 0x9C */ uint32_t tri2Cnt;
    /* 0xA0 */ uint32_t quadCnt;
    /* 0xA4 */ uint32_t lineCnt;
    /* 0xA8 */ uint32_t loaducodeCnt;
    /* 0xAC */ uint32_t pipeSyncRequired;
    /* 0xB0 */ uint32_t tileSyncRequired;
    /* 0xB4 */ uint32_t loadSyncRequired;
    /* 0xB8 */ uint32_t syncErr;
    /* 0xBC */ int32_t enableLog;
    /* 0xC0 */ int32_t ucodeType;
    /* 0xC4 */ int32_t ucodeInfoCount;
    /* 0xC8 */ UCodeInfo* ucodeInfo;
    /* 0xCC */ uint32_t modeH;
    /* 0xD0 */ uint32_t modeL;
    /* 0xD4 */ uint32_t geometryMode;
} UCodeDisas; // size = 0xD8

typedef struct {
    /* 0x000 */ uint8_t rumbleEnable[4];
    /* 0x004 */ uint8_t unk_04[0x40];
    /* 0x044 */ uint8_t unk_44[0x40];
    /* 0x084 */ uint8_t unk_84[0x40];
    /* 0x0C4 */ uint8_t unk_C4[0x40];
    /* 0x104 */ uint8_t unk_104;
    /* 0x105 */ uint8_t unk_105;
    /* 0x106 */ uint16_t unk_106;
    /* 0x108 */ uint16_t unk_108;
    /* 0x10A */ uint8_t unk_10A;
    /* 0x10B */ uint8_t unk_10B;
    /* 0x10C */ uint8_t unk_10C;
    /* 0x10D */ uint8_t unk_10D;
} UnkRumbleStruct; // size = 0x10E

typedef struct {
    /* 0x00 */ uint32_t value;
    /* 0x04 */ const char* name;
} F3dzexConst; // size = 0x8

typedef struct {
    /* 0x00 */ uint32_t value;
    /* 0x04 */ const char* setName;
    /* 0x08 */ const char* unsetName;
} F3dzexFlag; // size = 0x0C

typedef struct {
    /* 0x00 */ const char* name;
    /* 0x04 */ uint32_t value;
    /* 0x08 */ uint32_t mask;
} F3dzexRenderMode; // size = 0x0C

typedef struct {
    /* 0x00 */ const char* name;
    /* 0x04 */ uint32_t value;
} F3dzexSetModeMacroValue; // size = 0x8

typedef struct {
    /* 0x00 */ const char* name;
    /* 0x04 */ uint32_t shift;
    /* 0x08 */ uint32_t len;
    /* 0x0C */ F3dzexSetModeMacroValue values[4];
} F3dzexSetModeMacro; // size = 0x2C

typedef struct {
    /* 0x00 */ uint16_t* value;
    /* 0x04 */ const char* name;
} FlagSetEntry; // size = 0x08

typedef struct {
    /* 0x00 */ RomFile file;
    /* 0x08 */ RomFile palette;
} SkyboxFile; // size = 0x10

typedef struct {
    const char** textures;
    const char** palettes;
} SkyboxTableEntry;

typedef enum {
    LED_SOURCE_TUNIC_ORIGINAL,
    LED_SOURCE_HEALTH,
    LED_SOURCE_NAVI_ORIGINAL,
    LED_SOURCE_CUSTOM
} LEDColorSource;

#define ROM_FILE(name) \
    { 0, 0, #name }

#define ROM_FILE_EMPTY \
    { 0, 0, "" }

#define ROM_FILE_UNSET \
    { 0 }

#ifdef __cplusplus
};
#endif

#endif
