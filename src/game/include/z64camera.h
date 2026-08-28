#ifndef Z64CAMERA_H
#define Z64CAMERA_H

#include <runtime/libultra.h>
#include "z64runtime_action.h"

#define CAM_STAT_CUT        0
#define CAM_STAT_WAIT       1
#define CAM_STAT_UNK3       3
#define CAM_STAT_ACTIVE     7
#define CAM_STAT_UNK100     0x100

#define NUM_CAMS 4
#define MAIN_CAM 0
#define SUBCAM_FIRST 1
#define SUBCAM_FREE 0
#define SUBCAM_NONE -1
#define SUBCAM_ACTIVE -1

#define ONEPOINT_CS_INFO(camera) ((Unique9OnePointCs*)camera->paramData)
#define PARENT_CAM(cam) ((cam)->play->cameraPtrs[(cam)->parentCamIdx])
#define CHILD_CAM(cam) ((cam)->play->cameraPtrs[(cam)->childCamIdx])

typedef enum {
    /* 0x00 */ CAM_SET_NONE,
    /* 0x01 */ CAM_SET_NORMAL0,
    /* 0x02 */ CAM_SET_NORMAL1,
    /* 0x03 */ CAM_SET_DUNGEON0,
    /* 0x04 */ CAM_SET_DUNGEON1,
    /* 0x05 */ CAM_SET_NORMAL3,
    /* 0x06 */ CAM_SET_UNUSED_6,
    /* 0x07 */ CAM_SET_BOSS_GOHMA, // "BOSS_GOMA" (unused)
    /* 0x08 */ CAM_SET_BOSS_DODONGO, // "BOSS_DODO" (unused)
    /* 0x09 */ CAM_SET_BOSS_BARINADE, // "BOSS_BARI" (unused)
    /* 0x0A */ CAM_SET_BOSS_PHANTOM_GANON, // "BOSS_FGANON"
    /* 0x0B */ CAM_SET_BOSS_VOLVAGIA, // "BOSS_BAL"
    /* 0x0C */ CAM_SET_BOSS_BONGO, // "BOSS_SHADES"
    /* 0x0D */ CAM_SET_BOSS_MORPHA, // "BOSS_MOFA" (unused)
    /* 0x0E */ CAM_SET_BOSS_TWINROVA_PLATFORM, // Upper main platform and 4 smaller platforms in the room of the Twinrova boss battle "TWIN0"
    /* 0x0F */ CAM_SET_BOSS_TWINROVA_FLOOR, // The floor in the room of the Twinrova boss battle "TWIN1"
    /* 0x10 */ CAM_SET_BOSS_GANONDORF, // "BOSS_GANON1"
    /* 0x11 */ CAM_SET_BOSS_GANON, // "BOSS_GANON2" (unused)
    /* 0x12 */ CAM_SET_TOWER_CLIMB, // Various climbing structures (collapse sequence stairs, spiral around sarias house, zora domain climb, etc...) "TOWER0"
    /* 0x13 */ CAM_SET_TOWER_UNUSED, // Unused but data is in Phantom Ganon's Lair (no surface uses it) "TOWER1"
    /* 0x14 */ CAM_SET_MARKET_BALCONY, // Activated in day child market by talking to NPC on balcony above bombchu bowling "FIXED0"
    /* 0x15 */ CAM_SET_CHU_BOWLING, // Fixes the camera to the bombchu bowling targets while playing the minigame "FIXED1"
    /* 0x16 */ CAM_SET_PIVOT_CRAWLSPACE, // Unknown. In scene data: closely associated with crawlspaces CIRCLE0"
    /* 0x17 */ CAM_SET_PIVOT_SHOP_BROWSING, // Shopping and browsing for items "CIRCLE2"
    /* 0x18 */ CAM_SET_PIVOT_IN_FRONT, // The camera used on Link's balcony in Kokiri forest. Data present in scene data for Deku Tree, GTG, Inside Ganon's Castle (TODO: may or may not be used) "CIRCLE3"
    /* 0x19 */ CAM_SET_PREREND_FIXED, // Camera is fixed in position and rotation "PREREND0"
    /* 0x1A */ CAM_SET_PREREND_PIVOT, // Camera is fixed in position with fixed pitch, but is free to rotate in the yaw direction 360 degrees "PREREND1"
    /* 0x1B */ CAM_SET_PREREND_SIDE_SCROLL, // Camera side-scrolls position to follow link. Only used in castle courtyard with the guards "PREREND3"
    /* 0x1C */ CAM_SET_DOOR0, // Custom room door transitions, used in fire and royal family tomb
    /* 0x1D */ CAM_SET_DOORC, // Generic room door transitions, camera moves and follows player as the door is open and closed
    /* 0x1E */ CAM_SET_CRAWLSPACE, // Used in all crawlspaces "RAIL3"
    /* 0x1F */ CAM_SET_START0, // Data is given in Temple of Time, but no surface uses it
    /* 0x20 */ CAM_SET_START1, // Scene/room door transitions that snap the camera to a fixed location (example: ganon's towers doors climbing up)
    /* 0x21 */ CAM_SET_FREE0, // Full manual control is given over the camera
    /* 0x22 */ CAM_SET_FREE2, // Various OnePoint Cutscenes, 10 total (example: falling chest)
    /* 0x23 */ CAM_SET_PIVOT_CORNER, // Inside the carpenter jail cells from theives hideout "CIRCLE4"
    /* 0x24 */ CAM_SET_PIVOT_WATER_SURFACE, // Player diving from the surface of the water to underwater "CIRCLE5"
    /* 0x25 */ CAM_SET_CS_0, // Various cutscenes "DEMO0"
    /* 0x26 */ CAM_SET_CS_TWISTED_HALLWAY, // Never set to, but checked in twisting hallway (Forest Temple) "DEMO1"
    /* 0x27 */ CAM_SET_FOREST_BIRDS_EYE, // Used in the falling ceiling room in forest temple "MORI1"
    /* 0x28 */ CAM_SET_SLOW_CHEST_CS, // Long cutscene when opening a big chest with a major item "ITEM0"
    /* 0x29 */ CAM_SET_ITEM_UNUSED, // Unused "ITEM1"
    /* 0x2A */ CAM_SET_CS_3, // Various cutscenes "DEMO3"
    /* 0x2B */ CAM_SET_CS_ATTENTION, // Attention cutscenes and the actor siofuki (water spout/jet) "DEMO4"
    /* 0x2C */ CAM_SET_BEAN_GENERIC, // All beans except lost woods "UFOBEAN"
    /* 0x2D */ CAM_SET_BEAN_LOST_WOODS, // Lost woods bean "LIFTBEAN"
    /* 0x2E */ CAM_SET_SCENE_UNUSED, // Unused "SCENE0"
    /* 0x2F */ CAM_SET_SCENE_TRANSITION, // Scene Transitions "SCENE1"
    /* 0x30 */ CAM_SET_FIRE_PLATFORM, // All the fire platforms that rise. Also used in non-mq spirit shortcut "HIDAN1"
    /* 0x31 */ CAM_SET_FIRE_STAIRCASE, // Used on fire staircase actor cutscene in shortcut room connecting vanilla hammer chest to the final goron small key "HIDAN2"
    /* 0x32 */ CAM_SET_FOREST_UNUSED, // Unused "MORI2"
    /* 0x33 */ CAM_SET_FOREST_DEFEAT_POE, // Used when defeating a poe sister "MORI3"
    /* 0x34 */ CAM_SET_BIG_OCTO, // Used by big octo miniboss in Jabu Jabu "TAKO"
    /* 0x35 */ CAM_SET_MEADOW_BIRDS_EYE, // Used only as child in Sacred Forest Meadow Maze "SPOT05A"
    /* 0x36 */ CAM_SET_MEADOW_UNUSED, // Unused from Sacred Forest Meadow "SPOT05B"
    /* 0x37 */ CAM_SET_FIRE_BIRDS_EYE, // Used in lower-floor maze in non-mq fire temple "HIDAN3"
    /* 0x38 */ CAM_SET_TURN_AROUND, // Put the camera in front of player and turn around to look at player from the front "ITEM2"
    /* 0x39 */ CAM_SET_PIVOT_VERTICAL, // Lowering platforms (forest temple bow room, Jabu final shortcut) "CAM_SET_PIVOT_VERTICAL"
    /* 0x3A */ CAM_SET_NORMAL2,
    /* 0x3B */ CAM_SET_FISHING, // Fishing pond by the lake
    /* 0x3C */ CAM_SET_CS_C, // Various cutscenes "DEMOC"
    /* 0x3D */ CAM_SET_JABU_TENTACLE, // Jabu-Jabu Parasitic Tenticle Rooms "UO_FIBER"
    /* 0x3E */ CAM_SET_DUNGEON2,
    /* 0x3F */ CAM_SET_DIRECTED_YAW, // Does not auto-update yaw, tends to keep the camera pointed at a certain yaw (used by biggoron and final spirit lowering platform) "TEPPEN"
    /* 0x40 */ CAM_SET_PIVOT_FROM_SIDE, // Fixed side view, allows rotation of camera (eg. Potion Shop, Meadow at fairy grotto) "CIRCLE7"
    /* 0x41 */ CAM_SET_NORMAL4,
    /* 0x42 */ CAM_SET_MAX
} CameraSettingType;

typedef enum {
    /* 0x00 */ CAM_MODE_NORMAL,
    /* 0x01 */ CAM_MODE_TARGET, // "PARALLEL"
    /* 0x02 */ CAM_MODE_FOLLOWTARGET, // "KEEPON"
    /* 0x03 */ CAM_MODE_TALK,
    /* 0x04 */ CAM_MODE_BATTLE,
    /* 0x05 */ CAM_MODE_CLIMB,
    /* 0x06 */ CAM_MODE_FIRSTPERSON,  // "SUBJECT"
    /* 0x07 */ CAM_MODE_BOWARROW,
    /* 0x08 */ CAM_MODE_BOWARROWZ,
    /* 0x09 */ CAM_MODE_HOOKSHOT, // "FOOKSHOT"
    /* 0x0A */ CAM_MODE_BOOMERANG,
    /* 0x0B */ CAM_MODE_SLINGSHOT, // "PACHINCO"
    /* 0x0C */ CAM_MODE_CLIMBZ,
    /* 0x0D */ CAM_MODE_JUMP,
    /* 0x0E */ CAM_MODE_HANG,
    /* 0x0F */ CAM_MODE_HANGZ,
    /* 0x10 */ CAM_MODE_FREEFALL,
    /* 0x11 */ CAM_MODE_CHARGE,
    /* 0x12 */ CAM_MODE_STILL,
    /* 0x13 */ CAM_MODE_PUSHPULL,
    /* 0x14 */ CAM_MODE_FOLLOWBOOMERANG, // "BOOKEEPON"
    /* 0x15 */ CAM_MODE_MAX
} CameraModeType;

typedef enum {
    /* 0x00 */ CAM_FUNC_NONE,
    /* 0x01 */ CAM_FUNC_NORM0,
    /* 0x02 */ CAM_FUNC_NORM1,
    /* 0x03 */ CAM_FUNC_NORM2,
    /* 0x04 */ CAM_FUNC_UNUSED_4,
    /* 0x05 */ CAM_FUNC_NORM4,
    /* 0x06 */ CAM_FUNC_PARA0,
    /* 0x07 */ CAM_FUNC_PARA1,
    /* 0x08 */ CAM_FUNC_PARA2,
    /* 0x09 */ CAM_FUNC_PARA3,
    /* 0x0A */ CAM_FUNC_PARA4,
    /* 0x0B */ CAM_FUNC_KEEP0,
    /* 0x0C */ CAM_FUNC_KEEP1,
    /* 0x0D */ CAM_FUNC_KEEP2,
    /* 0x0E */ CAM_FUNC_KEEP3,
    /* 0x0F */ CAM_FUNC_KEEP4,
    /* 0x10 */ CAM_FUNC_SUBJ0,
    /* 0x11 */ CAM_FUNC_SUBJ1,
    /* 0x12 */ CAM_FUNC_SUBJ2,
    /* 0x13 */ CAM_FUNC_SUBJ3,
    /* 0x14 */ CAM_FUNC_SUBJ4,
    /* 0x15 */ CAM_FUNC_JUMP0,
    /* 0x16 */ CAM_FUNC_JUMP1,
    /* 0x17 */ CAM_FUNC_JUMP2,
    /* 0x18 */ CAM_FUNC_JUMP3,
    /* 0x19 */ CAM_FUNC_JUMP4,
    /* 0x1A */ CAM_FUNC_BATT0,
    /* 0x1B */ CAM_FUNC_BATT1,
    /* 0x1C */ CAM_FUNC_BATT2,
    /* 0x1D */ CAM_FUNC_BATT3,
    /* 0x1E */ CAM_FUNC_BATT4,
    /* 0x1F */ CAM_FUNC_FIXD0,
    /* 0x20 */ CAM_FUNC_FIXD1,
    /* 0x21 */ CAM_FUNC_FIXD2,
    /* 0x22 */ CAM_FUNC_FIXD3,
    /* 0x23 */ CAM_FUNC_FIXD4,
    /* 0x24 */ CAM_FUNC_DATA0,
    /* 0x25 */ CAM_FUNC_DATA1,
    /* 0x26 */ CAM_FUNC_DATA2,
    /* 0x27 */ CAM_FUNC_DATA3,
    /* 0x28 */ CAM_FUNC_DATA4,
    /* 0x29 */ CAM_FUNC_UNIQ0,
    /* 0x2A */ CAM_FUNC_UNIQ1,
    /* 0x2B */ CAM_FUNC_UNIQ2,
    /* 0x2C */ CAM_FUNC_UNIQ3,
    /* 0x2D */ CAM_FUNC_UNIQ4,
    /* 0x2E */ CAM_FUNC_UNIQ5,
    /* 0x2F */ CAM_FUNC_UNIQ6,
    /* 0x30 */ CAM_FUNC_UNIQ7,
    /* 0x31 */ CAM_FUNC_UNIQ8,
    /* 0x32 */ CAM_FUNC_UNIQ9,
    /* 0x33 */ CAM_FUNC_DEMO0,
    /* 0x34 */ CAM_FUNC_DEMO1,
    /* 0x35 */ CAM_FUNC_DEMO2,
    /* 0x36 */ CAM_FUNC_DEMO3,
    /* 0x37 */ CAM_FUNC_DEMO4,
    /* 0x38 */ CAM_FUNC_DEMO5,
    /* 0x39 */ CAM_FUNC_DEMO6,
    /* 0x3A */ CAM_FUNC_DEMO7,
    /* 0x3B */ CAM_FUNC_DEMO8,
    /* 0x3C */ CAM_FUNC_DEMO9,
    /* 0x3D */ CAM_FUNC_SPEC0,
    /* 0x3E */ CAM_FUNC_SPEC1,
    /* 0x3F */ CAM_FUNC_SPEC2,
    /* 0x40 */ CAM_FUNC_SPEC3,
    /* 0x41 */ CAM_FUNC_SPEC4,
    /* 0x42 */ CAM_FUNC_SPEC5,
    /* 0x43 */ CAM_FUNC_SPEC6,
    /* 0x44 */ CAM_FUNC_SPEC7,
    /* 0x45 */ CAM_FUNC_SPEC8,
    /* 0x46 */ CAM_FUNC_SPEC9,
    /* 0x47 */ CAM_FUNC_MAX
} CameraFuncType;

typedef enum {
    /* 0x00 */ CAM_DATA_Y_OFFSET,
    /* 0x01 */ CAM_DATA_EYE_DIST,
    /* 0x02 */ CAM_DATA_EYE_DIST_NEXT,
    /* 0x03 */ CAM_DATA_PITCH_TARGET,
    /* 0x04 */ CAM_DATA_YAW_UPDATE_RATE_TARGET,
    /* 0x05 */ CAM_DATA_XZ_UPDATE_RATE_TARGET,
    /* 0x06 */ CAM_DATA_MAX_YAW_UPDATE,
    /* 0x07 */ CAM_DATA_FOV,
    /* 0x08 */ CAM_DATA_AT_LERP_STEP_SCALE,
    /* 0x09 */ CAM_DATA_FLAGS,
    /* 0x0A */ CAM_DATA_YAW_TARGET,
    /* 0x0B */ CAM_DATA_GROUND_Y_OFFSET,
    /* 0x0C */ CAM_DATA_GROUND_AT_LERP_STEP_SCALE,
    /* 0x0D */ CAM_DATA_SWING_YAW_INIT,
    /* 0x0E */ CAM_DATA_SWING_YAW_FINAL,
    /* 0x0F */ CAM_DATA_SWING_PITCH_INIT,
    /* 0x10 */ CAM_DATA_SWING_PITCH_FINAL,
    /* 0x11 */ CAM_DATA_SWING_PITCH_ADJ,
    /* 0x12 */ CAM_DATA_MIN_MAX_DIST_FACTOR,
    /* 0x13 */ CAM_DATA_AT_OFFSET_X,
    /* 0x14 */ CAM_DATA_AT_OFFSET_Y,
    /* 0x15 */ CAM_DATA_AT_OFFSET_Z,
    /* 0x16 */ CAM_DATA_UNK_22,
    /* 0x17 */ CAM_DATA_UNK_23,
    /* 0x18 */ CAM_DATA_FOV_SCALE,
    /* 0x19 */ CAM_DATA_YAW_SCALE,
    /* 0x1A */ CAM_DATA_UNK_26,
    /* 0x1B */ CAM_DATA_MAX
} CameraDataType;

#define CAM_FUNCDATA_FLAGS(flags) \
    { flags, CAM_DATA_FLAGS }

typedef struct {
    /* 0x00 */ Vec3f collisionClosePoint;
    /* 0x0C */ CollisionPoly* atEyePoly;
    /* 0x10 */ float swingUpdateRate;
    /* 0x14 */ int16_t unk_14;
    /* 0x16 */ int16_t unk_16;
    /* 0x18 */ int16_t unk_18;
    /* 0x1A */ int16_t swingUpdateRateTimer;
} SwingAnimation; // size = 0x1C

typedef struct {
    /* 0x00 */ SwingAnimation swing;
    /* 0x1C */ float yOffset;
    /* 0x20 */ float unk_20;
    /* 0x24 */ int16_t slopePitchAdj;
    /* 0x26 */ int16_t swingYawTarget;
    /* 0x28 */ int16_t unk_28;
    /* 0x2A */ int16_t startSwingTimer;
} Normal1Anim; // size = 0x2C

typedef struct {
    /* 0x00 */ float yOffset;
    /* 0x04 */ float distMin;
    /* 0x08 */ float distMax;
    /* 0x0C */ float unk_0C;
    /* 0x10 */ float unk_10;
    /* 0x14 */ float unk_14;
    /* 0x18 */ float fovTarget;
    /* 0x1C */ float atLERPScaleMax;
    /* 0x20 */ int16_t pitchTarget;
    /* 0x22 */ int16_t interfaceFlags;
    /* 0x24 */ Normal1Anim anim;
} Normal1; // size = 0x50

#define CAM_FUNCDATA_NORM1(yOffset, eyeDist, eyeDistNext, pitchTarget, yawUpdateRateTarget, xzUpdateRateTarget, maxYawUpdate, fov, atLerpStepScale, flags) \
    { yOffset, CAM_DATA_Y_OFFSET }, \
    { eyeDist, CAM_DATA_EYE_DIST }, \
    { eyeDistNext, CAM_DATA_EYE_DIST_NEXT }, \
    { pitchTarget, CAM_DATA_PITCH_TARGET }, \
    { yawUpdateRateTarget, CAM_DATA_YAW_UPDATE_RATE_TARGET }, \
    { xzUpdateRateTarget, CAM_DATA_XZ_UPDATE_RATE_TARGET }, \
    { maxYawUpdate, CAM_DATA_MAX_YAW_UPDATE }, \
    { fov, CAM_DATA_FOV }, \
    { atLerpStepScale, CAM_DATA_AT_LERP_STEP_SCALE }, \
    { flags, CAM_DATA_FLAGS }

#define CAM_FUNCDATA_NORM1_ALT(yOffset, eyeDist, eyeDistNext, pitchTarget, yawUpdateRateTarget, xzUpdateRateTarget, maxYawUpdate, fov, atLerpStepScale, flags) \
    { yOffset, CAM_DATA_Y_OFFSET }, \
    { eyeDist, CAM_DATA_EYE_DIST }, \
    { eyeDistNext, CAM_DATA_EYE_DIST_NEXT }, \
    { pitchTarget, CAM_DATA_PITCH_TARGET }, \
    { yawUpdateRateTarget, CAM_DATA_YAW_UPDATE_RATE_TARGET }, \
    { xzUpdateRateTarget, CAM_DATA_UNK_26 }, \
    { maxYawUpdate, CAM_DATA_MAX_YAW_UPDATE }, \
    { fov, CAM_DATA_FOV }, \
    { atLerpStepScale, CAM_DATA_AT_LERP_STEP_SCALE }, \
    { flags, CAM_DATA_FLAGS }

typedef struct {
    /* 0x00 */ Vec3f unk_00;
    /* 0x0C */ Vec3f unk_0C;
    /* 0x18 */ float unk_18;
    /* 0x1C */ float unk_1C;
    /* 0x20 */ int16_t unk_20;
    /* 0x22 */ int16_t unk_22;
    /* 0x24 */ float unk_24;
    /* 0x28 */ int16_t unk_28;
} Normal2Anim; // size = 0x2A

typedef struct {
    /* 0x00 */ float unk_00;
    /* 0x04 */ float unk_04;
    /* 0x08 */ float unk_08;
    /* 0x0C */ float unk_0C;
    /* 0x10 */ float unk_10;
    /* 0x14 */ float unk_14;
    /* 0x18 */ float unk_18;
    /* 0x1C */ int16_t unk_1C;
    /* 0x1E */ int16_t interfaceFlags;
    /* 0x20 */ Normal2Anim anim;
} Normal2; // size = 0x4A

#define CAM_FUNCDATA_NORM2(yOffset, eyeDist, eyeDistNext, unk_23, yawUpdateRateTarget, maxYawUpdate, fov, atLerpStepScale, flags) \
    { yOffset, CAM_DATA_Y_OFFSET }, \
    { eyeDist, CAM_DATA_EYE_DIST }, \
    { eyeDistNext, CAM_DATA_EYE_DIST_NEXT }, \
    { unk_23, CAM_DATA_UNK_23 }, \
    { yawUpdateRateTarget, CAM_DATA_YAW_UPDATE_RATE_TARGET }, \
    { maxYawUpdate, CAM_DATA_MAX_YAW_UPDATE }, \
    { fov, CAM_DATA_FOV }, \
    { atLerpStepScale, CAM_DATA_AT_LERP_STEP_SCALE }, \
    { flags, CAM_DATA_FLAGS }

typedef struct {
    /* 0x00 */ Vec3f unk_00;
    /* 0x0C */ float yTarget;
    /* 0x10 */ int16_t unk_10;
    /* 0x12 */ int16_t yawTarget;
    /* 0x14 */ int16_t pitchTarget;
    /* 0x16 */ int16_t unk_16;
    /* 0x18 */ int16_t animTimer;
} Parallel1Anim; // size = 0x1A

typedef struct {
    /* 0x00 */ float yOffset;
    /* 0x04 */ float distTarget;
    /* 0x08 */ float unk_08;
    /* 0x0C */ float unk_0C;
    /* 0x10 */ float fovTarget;
    /* 0x14 */ float unk_14;
    /* 0x18 */ float unk_18;
    /* 0x1C */ float unk_1C;
    /* 0x20 */ int16_t pitchTarget;
    /* 0x22 */ int16_t yawTarget;
    /* 0x24 */ int16_t interfaceFlags;
    /* 0x28 */ Parallel1Anim anim;
} Parallel1; // size = 0x42

#define CAM_FUNCDATA_PARA1(yOffset, eyeDist, pitchTarget, yawTarget, yawUpdateRateTarget, xzUpdateRateTarget, fov, atLerpStepScale, flags, groundYOffset, groundAtLerpStepScale) \
    { yOffset, CAM_DATA_Y_OFFSET }, \
    { eyeDist, CAM_DATA_EYE_DIST }, \
    { pitchTarget, CAM_DATA_PITCH_TARGET }, \
    { yawTarget, CAM_DATA_YAW_TARGET }, \
    { yawUpdateRateTarget, CAM_DATA_YAW_UPDATE_RATE_TARGET }, \
    { xzUpdateRateTarget, CAM_DATA_XZ_UPDATE_RATE_TARGET }, \
    { fov, CAM_DATA_FOV }, \
    { atLerpStepScale, CAM_DATA_AT_LERP_STEP_SCALE }, \
    { flags, CAM_DATA_FLAGS }, \
    { groundYOffset, CAM_DATA_GROUND_Y_OFFSET }, \
    { groundAtLerpStepScale, CAM_DATA_GROUND_AT_LERP_STEP_SCALE }

typedef struct {

    /* 0x00 */ SwingAnimation swing;
    /* 0x1C */ float unk_1C;
    /* 0x20 */ VecSph unk_20;
} Jump1Anim; // size = 0x28

typedef struct {
    /* 0x00 */ float atYOffset;
    /* 0x04 */ float distMin;
    /* 0x08 */ float distMax;
    /* 0x0C */ float yawUpateRateTarget;
    /* 0x10 */ float maxYawUpdate;
    /* 0x14 */ float unk_14; // never used.
    /* 0x18 */ float atLERPScaleMax;
    /* 0x1C */ int16_t interfaceFlags;
    /* 0x20 */ Jump1Anim anim;
} Jump1; // size = 0x48

#define CAM_FUNCDATA_JUMP1(yOffset, eyeDist, eyeDistNext, yawUpdateRateTarget, maxYawUpdate, fov, atLerpStepScale, flags) \
    { yOffset, CAM_DATA_Y_OFFSET }, \
    { eyeDist, CAM_DATA_EYE_DIST }, \
    { eyeDistNext, CAM_DATA_EYE_DIST_NEXT }, \
    { yawUpdateRateTarget, CAM_DATA_YAW_UPDATE_RATE_TARGET }, \
    { maxYawUpdate, CAM_DATA_MAX_YAW_UPDATE }, \
    { fov, CAM_DATA_FOV }, \
    { atLerpStepScale, CAM_DATA_AT_LERP_STEP_SCALE }, \
    { flags, CAM_DATA_FLAGS }

typedef struct {
    /* 0x0 */ float floorY;
    /* 0x4 */ int16_t yawTarget;
    /* 0x6 */ int16_t initYawDiff; // unused, set but not read.
    /* 0x8 */ int16_t yawAdj;
    /* 0xA */ int16_t onFloor; // unused, set but not read
    /* 0xC */ int16_t animTimer;
} Jump2Anim; // size = 0x10

typedef struct {
    /* 0x00 */ float atYOffset;
    /* 0x04 */ float minDist;
    /* 0x08 */ float maxDist;
    /* 0x0C */ float minMaxDistFactor;
    /* 0x10 */ float yawUpdRateTarget;
    /* 0x14 */ float xzUpdRateTarget;
    /* 0x18 */ float fovTarget;
    /* 0x1C */ float atLERPStepScale;
    /* 0x20 */ int16_t interfaceFlags;
    /* 0x24 */ Jump2Anim anim;
} Jump2; // size = 0x34

#define CAM_FUNCDATA_JUMP2(yOffset, eyeDist, eyeDistNext, minMaxDistFactor, yawUpdateRateTarget, xzUpdateRateTarget, fov, atLerpStepScale, flags) \
    { yOffset, CAM_DATA_Y_OFFSET }, \
    { eyeDist, CAM_DATA_EYE_DIST }, \
    { eyeDistNext, CAM_DATA_EYE_DIST_NEXT }, \
    { minMaxDistFactor, CAM_DATA_MIN_MAX_DIST_FACTOR }, \
    { yawUpdateRateTarget, CAM_DATA_YAW_UPDATE_RATE_TARGET }, \
    { xzUpdateRateTarget, CAM_DATA_XZ_UPDATE_RATE_TARGET }, \
    { fov, CAM_DATA_FOV }, \
    { atLerpStepScale, CAM_DATA_AT_LERP_STEP_SCALE }, \
    { flags, CAM_DATA_FLAGS }

typedef struct {
    /* 0x00 */ SwingAnimation swing;
    /* 0x1C */ float unk_1C;
    /* 0x20 */ int16_t animTimer;
    /* 0x22 */ int16_t mode;
} Jump3Anim; // size = 0x24

typedef struct {
    /* 0x00 */ float yOffset;
    /* 0x04 */ float distMin;
    /* 0x08 */ float distMax;
    /* 0x0C */ float swingUpdateRate;
    /* 0x10 */ float unk_10;
    /* 0x14 */ float unk_14;
    /* 0x18 */ float fovTarget;
    /* 0x1C */ float unk_1C;
    /* 0x20 */ int16_t pitchTarget;
    /* 0x22 */ int16_t interfaceFlags;
    /* 0x24 */ Jump3Anim anim;
} Jump3; // size = 0x48

#define CAM_FUNCDATA_JUMP3(yOffset, eyeDist, eyeDistNext, pitchTarget, yawUpdateRateTarget, xzUpdateRateTarget, maxYawUpdate, fov, atLerpStepScale, flags) \
    { yOffset, CAM_DATA_Y_OFFSET }, \
    { eyeDist, CAM_DATA_EYE_DIST }, \
    { eyeDistNext, CAM_DATA_EYE_DIST_NEXT }, \
    { pitchTarget, CAM_DATA_PITCH_TARGET }, \
    { yawUpdateRateTarget, CAM_DATA_YAW_UPDATE_RATE_TARGET }, \
    { xzUpdateRateTarget, CAM_DATA_XZ_UPDATE_RATE_TARGET }, \
    { maxYawUpdate, CAM_DATA_MAX_YAW_UPDATE }, \
    { fov, CAM_DATA_FOV }, \
    { atLerpStepScale, CAM_DATA_AT_LERP_STEP_SCALE }, \
    { flags, CAM_DATA_FLAGS }

typedef struct {
    /* 0x00 */ float initialEyeToAtDist;
    /* 0x04 */ float roll;
    /* 0x08 */ float yPosOffset;
    /* 0x0C */ Actor* target;
    /* 0x10 */ float unk_10;
    /* 0x14 */ int16_t unk_14; // unused
    /* 0x16 */ int16_t initialEyeToAtYaw;
    /* 0x18 */ int16_t initialEyeToAtPitch;
    /* 0x1A */ int16_t animTimer;
    /* 0x1C */ int16_t chargeTimer;
} Battle1Anim; // size = 0x1E

typedef struct {
    /* 0x00 */ float yOffset;
    /* 0x04 */ float distance;
    /* 0x08 */ float swingYawInitial;
    /* 0x0C */ float swingYawFinal;
    /* 0x10 */ float swingPitchInitial;
    /* 0x14 */ float swingPitchFinal;
    /* 0x18 */ float swingPitchAdj;
    /* 0x1C */ float fov;
    /* 0x20 */ float atLERPScaleOnGround;
    /* 0x24 */ float yOffsetOffGround;
    /* 0x28 */ float atLERPScaleOffGround;
    /* 0x2C */ int16_t flags;
    /* 0x30 */ Battle1Anim anim;
} Battle1; // size = 0x50

#define CAM_FUNCDATA_BATT1(yOffset, eyeDist, swingYawInit, swingYawFinal, swingPitchInit, swingPitchFinal, swingPitchAdj, fov, atLerpStepScale, flags, groundYOffset, groundAtLerpStepScale) \
    { yOffset, CAM_DATA_Y_OFFSET }, \
    { eyeDist, CAM_DATA_EYE_DIST }, \
    { swingYawInit, CAM_DATA_SWING_YAW_INIT }, \
    { swingYawFinal, CAM_DATA_SWING_YAW_FINAL }, \
    { swingPitchInit, CAM_DATA_SWING_PITCH_INIT }, \
    { swingPitchFinal, CAM_DATA_SWING_PITCH_FINAL }, \
    { swingPitchAdj, CAM_DATA_SWING_PITCH_ADJ }, \
    { fov, CAM_DATA_FOV }, \
    { atLerpStepScale, CAM_DATA_AT_LERP_STEP_SCALE }, \
    { flags, CAM_DATA_FLAGS }, \
    { groundYOffset, CAM_DATA_GROUND_Y_OFFSET }, \
    { groundAtLerpStepScale, CAM_DATA_GROUND_AT_LERP_STEP_SCALE }

typedef struct {
    /* 0x0 */ int16_t animTimer;
} Battle4Anim; // size = 0x2

typedef struct {
    /* 0x00 */ float yOffset;
    /* 0x04 */ float rTarget;
    /* 0x08 */ int16_t pitchTarget;
    /* 0x0C */ float lerpUpdateRate;
    /* 0x10 */ float fovTarget;
    /* 0x14 */ float atLERPTarget;
    /* 0x18 */ int16_t interfaceFlags;
    /* 0x1A */ int16_t unk_1A;
    /* 0x1C */ Battle4Anim anim;
} Battle4; // size = 0x20

#define CAM_FUNCDATA_BATT4(yOffset, eyeDist, pitchTarget, yawUpdateRateTarget, fov, atLerpStepScale, flags) \
    { yOffset, CAM_DATA_Y_OFFSET }, \
    { eyeDist, CAM_DATA_EYE_DIST }, \
    { pitchTarget, CAM_DATA_PITCH_TARGET }, \
    { yawUpdateRateTarget, CAM_DATA_YAW_UPDATE_RATE_TARGET }, \
    { fov, CAM_DATA_FOV }, \
    { atLerpStepScale, CAM_DATA_AT_LERP_STEP_SCALE }, \
    { flags, CAM_DATA_FLAGS }

typedef struct {
    /* 0x00 */ float unk_00;
    /* 0x04 */ float unk_04;
    /* 0x08 */ float unk_08;
    /* 0x0C */ Actor* unk_0C;
    /* 0x10 */ int16_t unk_10;
    /* 0x12 */ int16_t unk_12;
    /* 0x14 */ int16_t unk_14;
    /* 0x16 */ int16_t unk_16;
} Keep1Anim; // size = 0x18

typedef struct {
    /* 0x00 */ float unk_00;
    /* 0x04 */ float unk_04;
    /* 0x08 */ float unk_08;
    /* 0x0C */ float unk_0C;
    /* 0x10 */ float unk_10;
    /* 0x14 */ float unk_14;
    /* 0x18 */ float unk_18;
    /* 0x1C */ float unk_1C;
    /* 0x20 */ float unk_20;
    /* 0x24 */ float unk_24;
    /* 0x28 */ float unk_28;
    /* 0x2C */ float unk_2C;
    /* 0x30 */ int16_t interfaceFlags;
    /* 0x34 */ Keep1Anim anim;
} KeepOn1; // size = 0x4C

#define CAM_FUNCDATA_KEEP1(yOffset, eyeDist, eyeDistNext, swingYawInit, swingYawFinal, swingPitchInit, swingPitchFinal, swingPitchAdj, fov, atLerpStepScale, flags, groundYOffset, groundAtLerpStepScale) \
    { yOffset, CAM_DATA_Y_OFFSET }, \
    { eyeDist, CAM_DATA_EYE_DIST }, \
    { eyeDistNext, CAM_DATA_EYE_DIST_NEXT }, \
    { swingYawInit, CAM_DATA_SWING_YAW_INIT }, \
    { swingYawFinal, CAM_DATA_SWING_YAW_FINAL }, \
    { swingPitchInit, CAM_DATA_SWING_PITCH_INIT }, \
    { swingPitchFinal, CAM_DATA_SWING_PITCH_FINAL }, \
    { swingPitchAdj, CAM_DATA_SWING_PITCH_ADJ }, \
    { fov, CAM_DATA_FOV }, \
    { atLerpStepScale, CAM_DATA_AT_LERP_STEP_SCALE }, \
    { flags, CAM_DATA_FLAGS }, \
    { groundYOffset, CAM_DATA_GROUND_Y_OFFSET }, \
    { groundAtLerpStepScale, CAM_DATA_GROUND_AT_LERP_STEP_SCALE }

typedef struct {
    /* 0x00 */ Vec3f eyeToAtTarget; // esentially a VecSph, but all floats.
    /* 0x0C */ Actor* target;
    /* 0x10 */ Vec3f atTarget;
    /* 0x1C */ int16_t animTimer;
} Keep3Anim; // size = 0x20

typedef struct {
    /* 0x00 */ float yOffset;
    /* 0x04 */ float minDist;
    /* 0x08 */ float maxDist;
    /* 0x0C */ float swingYawInital;
    /* 0x10 */ float swingYawFinal;
    /* 0x14 */ float swingPitchInitial;
    /* 0x18 */ float swingPitchFinal;
    /* 0x1C */ float swingPitchAdj;
    /* 0x20 */ float fovTarget;
    /* 0x24 */ float atLERPScaleMax;
    /* 0x28 */ int16_t initTimer;
    /* 0x2A */ int16_t flags;
    /* 0x2C */ Keep3Anim anim;
} KeepOn3; // size = 0x4C

#define CAM_FUNCDATA_KEEP3(yOffset, eyeDist, eyeDistNext, swingYawInit, swingYawFinal, swingPitchInit, swingPitchFinal, swingPitchAdj, fov, atLerpStepScale, yawUpdateRateTarget, flags) \
    { yOffset, CAM_DATA_Y_OFFSET }, \
    { eyeDist, CAM_DATA_EYE_DIST }, \
    { eyeDistNext, CAM_DATA_EYE_DIST_NEXT }, \
    { swingYawInit, CAM_DATA_SWING_YAW_INIT }, \
    { swingYawFinal, CAM_DATA_SWING_YAW_FINAL }, \
    { swingPitchInit, CAM_DATA_SWING_PITCH_INIT }, \
    { swingPitchFinal, CAM_DATA_SWING_PITCH_FINAL }, \
    { swingPitchAdj, CAM_DATA_SWING_PITCH_ADJ }, \
    { fov, CAM_DATA_FOV }, \
    { atLerpStepScale, CAM_DATA_AT_LERP_STEP_SCALE }, \
    { yawUpdateRateTarget, CAM_DATA_YAW_UPDATE_RATE_TARGET }, \
    { flags, CAM_DATA_FLAGS }

typedef struct {
    /* 0x00 */ float unk_00;
    /* 0x04 */ float unk_04;
    /* 0x08 */ float unk_08;
    /* 0x0C */ int16_t unk_0C;
    /* 0x0E */ int16_t unk_0E;
    /* 0x10 */ int16_t unk_10;
    /* 0x12 */ int16_t unk_12;
    /* 0x14 */ int16_t unk_14;
} KeepOn4_Unk20; // size = 0x14

typedef struct {
    /* 0x00 */ float unk_00;
    /* 0x04 */ float unk_04;
    /* 0x08 */ float unk_08;
    /* 0x0C */ float unk_0C;
    /* 0x10 */ float unk_10;
    /* 0x14 */ float unk_14;
    /* 0x18 */ float unk_18;
    /* 0x1C */ int16_t unk_1C;
    /* 0x1E */ int16_t unk_1E;
    /* 0x20 */ KeepOn4_Unk20 unk_20;
} KeepOn4; // size = 0x34

#define CAM_FUNCDATA_KEEP4(yOffset, eyeDist, pitchTarget, yawTarget, atOffsetZ, fov, flags, yawUpdateRateTarget, unk_22) \
    { yOffset, CAM_DATA_Y_OFFSET }, \
    { eyeDist, CAM_DATA_EYE_DIST }, \
    { pitchTarget, CAM_DATA_PITCH_TARGET }, \
    { yawTarget, CAM_DATA_YAW_TARGET }, \
    { atOffsetZ, CAM_DATA_AT_OFFSET_Z }, \
    { fov, CAM_DATA_FOV }, \
    { flags, CAM_DATA_FLAGS }, \
    { yawUpdateRateTarget, CAM_DATA_YAW_UPDATE_RATE_TARGET }, \
    { unk_22, CAM_DATA_UNK_22 }

typedef struct {
    /* 0x0 */ float fovTarget;
    /* 0x4 */ int16_t animTimer;
} KeepOn0Anim; // size = 0x8

typedef struct {
    /* 0x00 */ float fovScale;
    /* 0x04 */ float yawScale;
    /* 0x08 */ int16_t timerInit;
    /* 0x0A */ int16_t interfaceFlags;
    /* 0x0C */ KeepOn0Anim anim;
} KeepOn0; // size = 0x14

#define CAM_FUNCDATA_KEEP0(fovScale, yawScale, yawUpdateRateTarget, flags) \
    { fovScale, CAM_DATA_FOV_SCALE }, \
    { yawScale, CAM_DATA_YAW_SCALE }, \
    { yawUpdateRateTarget, CAM_DATA_YAW_UPDATE_RATE_TARGET }, \
    { flags, CAM_DATA_FLAGS }

typedef struct {
    /* 0x00 */ PosRot eyePosRotTarget;
    /* 0x14 */ int16_t fov;
} Fixed1Anim; // size = 0x18

typedef struct {
    /* 0x00 */ float unk_00; // seems to be unused?
    /* 0x04 */ float lerpStep;
    /* 0x08 */ float fov;
    /* 0x0C */ int16_t interfaceFlags;
    /* 0x10 */ Fixed1Anim anim;
} Fixed1; // size = 0x28

#define CAM_FUNCDATA_FIXD1(yOffset, yawUpdateRateTarget, fov, flags) \
    { yOffset, CAM_DATA_Y_OFFSET }, \
    { yawUpdateRateTarget, CAM_DATA_YAW_UPDATE_RATE_TARGET }, \
    { fov, CAM_DATA_FOV }, \
    { flags, CAM_DATA_FLAGS }

typedef struct {
    /* 0x0 */ Vec3f eye;
    /* 0xC */ int16_t fov;
} Fixed2InitParams; // size = 0x10

typedef struct {
    /* 0x00 */ float yOffset;
    /* 0x04 */ float eyeStepScale;
    /* 0x08 */ float posStepScale;
    /* 0x0C */ float fov;
    /* 0x10 */ int16_t interfaceFlags;
    /* 0x14 */ Fixed2InitParams initParams;
} Fixed2; // size = 0x24

#define CAM_FUNCDATA_FIXD2(yOffset, yawUpdateRateTarget, xzUpdateRateTarget, fov, flags) \
    { yOffset, CAM_DATA_Y_OFFSET }, \
    { yawUpdateRateTarget, CAM_DATA_YAW_UPDATE_RATE_TARGET }, \
    { xzUpdateRateTarget, CAM_DATA_XZ_UPDATE_RATE_TARGET }, \
    { fov, CAM_DATA_FOV }, \
    { flags, CAM_DATA_FLAGS }

typedef struct {
    /* 0x0 */ Vec3s rot;
    /* 0x6 */ int16_t fov;
    /* 0x8 */ int16_t updDirTimer;
    /* 0xA */ int16_t jfifId;
} Fixed3Anim; // size = 0xC

typedef struct {
    /* 0x0 */ int16_t interfaceFlags;
    /* 0x4 */ Fixed3Anim anim;
} Fixed3; // size = 0x10

typedef struct {
    /* 0x0 */ Vec3f eyeTarget;
    /* 0xC */ float followSpeed;
} Fixed4Anim; // size = 0x10

typedef struct {
    /* 0x00 */ float yOffset;
    /* 0x04 */ float speedToEyePos;
    /* 0x08 */ float followSpeed;
    /* 0x0C */ float fov;
    /* 0x10 */ int16_t interfaceFlags;
    /* 0x14 */ Fixed4Anim anim;
} Fixed4; // size = 0x24

#define CAM_FUNCDATA_FIXD4(yOffset, yawUpdateRateTarget, xzUpdateRateTarget, fov, flags) \
    { yOffset, CAM_DATA_Y_OFFSET }, \
    { yawUpdateRateTarget, CAM_DATA_YAW_UPDATE_RATE_TARGET }, \
    { xzUpdateRateTarget, CAM_DATA_XZ_UPDATE_RATE_TARGET }, \
    { fov, CAM_DATA_FOV }, \
    { flags, CAM_DATA_FLAGS }

typedef struct {
    /* 0x0 */ float r;
    /* 0x4 */ int16_t yaw;
    /* 0x6 */ int16_t pitch;
    /* 0x8 */ int16_t animTimer;
} Subj3Anim; // size = 0xC

typedef struct {
    /* 0x00 */ float eyeNextYOffset;
    /* 0x04 */ float eyeDist;
    /* 0x08 */ float eyeNextDist;
    /* 0x0C */ float unk_0C; // unused
    /* 0x10 */ Vec3f atOffset;
    /* 0x1C */ float fovTarget;
    /* 0x20 */ int16_t interfaceFlags;
    /* 0x24 */ Subj3Anim anim;
} Subj3; // size = 0x30

#define CAM_FUNCDATA_SUBJ3(yOffset, eyeDist, eyeDistNext, yawUpdateRateTarget, atOffsetX, atOffsetY, atOffsetZ, fov, flags) \
    { yOffset, CAM_DATA_Y_OFFSET }, \
    { eyeDist, CAM_DATA_EYE_DIST }, \
    { eyeDistNext, CAM_DATA_EYE_DIST_NEXT }, \
    { yawUpdateRateTarget, CAM_DATA_YAW_UPDATE_RATE_TARGET }, \
    { atOffsetX, CAM_DATA_AT_OFFSET_X }, \
    { atOffsetY, CAM_DATA_AT_OFFSET_Y }, \
    { atOffsetZ, CAM_DATA_AT_OFFSET_Z }, \
    { fov, CAM_DATA_FOV }, \
    { flags, CAM_DATA_FLAGS }

typedef struct {
    /* 0x00 */ Linef unk_00;
    /* 0x18 */ float unk_18;
    /* 0x1C */ float unk_1C;
    /* 0x20 */ float unk_20;
    /* 0x24 */ float unk_24;
    /* 0x28 */ float unk_28;
    /* 0x2C */ int16_t unk_2C;
    /* 0x2E */ int16_t unk_2E;
    /* 0x30 */ int16_t unk_30;
    /* 0x32 */ int16_t unk_32;
} Subj4Anim; // size = 0x34

typedef struct {
    /* 0x0 */ int16_t interfaceFlags;
    /* 0x4 */ Subj4Anim anim;
} Subj4; // size = 0x38

#define CAM_FUNCDATA_SUBJ4(yOffset, eyeDist, eyeDistNext, yawUpdateRateTarget, fov, flags) \
    { yOffset, CAM_DATA_Y_OFFSET }, \
    { eyeDist, CAM_DATA_EYE_DIST }, \
    { eyeDistNext, CAM_DATA_EYE_DIST_NEXT }, \
    { yawUpdateRateTarget, CAM_DATA_YAW_UPDATE_RATE_TARGET }, \
    { fov, CAM_DATA_FOV }, \
    { flags, CAM_DATA_FLAGS }

typedef struct {
    /* 0x00 */ PosRot eyePosRot;
    /* 0x14 */ char unk_14[0x8];
    /* 0x1C */ int16_t fov;
    /* 0x1E */ int16_t jfifId;
} Data4InitParams; // size = 0x20

typedef struct {
    /* 0x0 */ float yOffset;
    /* 0x4 */ float fov;
    /* 0x8 */ int16_t interfaceFlags;
    /* 0xC */ Data4InitParams initParams;
} Data4; // size = 0x2C

#define CAM_FUNCDATA_DATA4(yOffset, fov, flags) \
    { yOffset, CAM_DATA_Y_OFFSET }, \
    { fov, CAM_DATA_FOV }, \
    { flags, CAM_DATA_FLAGS }

typedef struct {
    /* 0x0 */ float unk_00; // unused
    /* 0x4 */ int16_t yawTarget;
    /* 0x6 */ int16_t yawTargetAdj;
    /* 0x8 */ int16_t timer;
} Unique1Anim; // size = 0xC

typedef struct {
    /* 0x00 */ float yOffset;
    /* 0x04 */ float distMin;
    /* 0x08 */ float distMax;
    /* 0x0C */ char unk_0C[4]; // unused
    /* 0x10 */ float fovTarget;
    /* 0x14 */ float atLERPScaleMax;
    /* 0x18 */ int16_t pitchTarget;
    /* 0x1A */ int16_t interfaceFlags;
    /* 0x1C */ Unique1Anim anim;
} Unique1; // size = 0x28

#define CAM_FUNCDATA_UNIQ1(yOffset, eyeDist, eyeDistNext, pitchTarget, fov, atLerpStepScale, flags) \
    { yOffset, CAM_DATA_Y_OFFSET }, \
    { eyeDist, CAM_DATA_EYE_DIST }, \
    { eyeDistNext, CAM_DATA_EYE_DIST_NEXT }, \
    { pitchTarget, CAM_DATA_PITCH_TARGET }, \
    { fov, CAM_DATA_FOV }, \
    { atLerpStepScale, CAM_DATA_AT_LERP_STEP_SCALE }, \
    { flags, CAM_DATA_FLAGS }

typedef struct {
    /* 0x0 */ float unk_00;
    /* 0x4 */ int16_t unk_04;
} Unique2Unk10; // size = 0x8

typedef struct {
    /* 0x00 */ float yOffset;
    /* 0x04 */ float distTarget;
    /* 0x08 */ float fovTarget;
    /* 0x0C */ int16_t interfaceFlags;
    /* 0x10 */ Unique2Unk10 unk_10; // unused, values set but not read.
} Unique2; // size = 0x18

#define CAM_FUNCDATA_UNIQ2(yOffset, eyeDist, fov, flags) \
    { yOffset, CAM_DATA_Y_OFFSET }, \
    { eyeDist, CAM_DATA_EYE_DIST }, \
    { fov, CAM_DATA_FOV }, \
    { flags, CAM_DATA_FLAGS }

typedef struct {
    /* 0x0 */ float initialFov;
    /* 0x4 */ float initialDist;
} Unique3Anim; // size = 0x8

typedef struct {
    /* 0x0 */ float yOffset;
    /* 0x4 */ float fov;
    /* 0x8 */ int16_t interfaceFlags;
} Unique3Params; // size = 0xC

typedef struct {
    /* 0x0 */ struct Actor* doorActor;
    /* 0x4 */ int16_t camDataIdx;
    /* 0x6 */ int16_t timer1;
    /* 0x8 */ int16_t timer2;
    /* 0xA */ int16_t timer3;
} DoorParams; // size = 0xC

typedef struct {
    /* 0x00 */ DoorParams doorParams;
    /* 0x0C */ Unique3Params params;
    /* 0x18 */ Unique3Anim anim;
} Unique3; // size = 0x20

#define CAM_FUNCDATA_UNIQ3(yOffset, fov, flags) \
    { yOffset, CAM_DATA_Y_OFFSET }, \
    { fov, CAM_DATA_FOV }, \
    { flags, CAM_DATA_FLAGS }

typedef struct {
    /* 0x00 */ Vec3f initalPos;
    /* 0x0C */ int16_t animTimer;
    /* 0x10 */ Linef sceneCamPosPlayerLine;
} Unique0Anim; // size = 0x28

typedef struct {
    /* 0x0 */ int16_t interfaceFlags;
    /* 0x4 */ Unique0Anim anim;
} Unique0Params; // size = 0x2C

typedef struct {
    /* 0x0 */ DoorParams doorParams;
    /* 0xC */ Unique0Params uniq0;
} Unique0; // size = 0x38

typedef struct {
    /* 0x0 */ int16_t interfaceFlags;
} Unique6; // size = 0x4

typedef union {
    /* 0x0 */ Vec3s unk_00;
} Unique7Unk8; // size = 0x8

typedef struct {
    /* 0x0 */ float fov;
    /* 0x4 */ int16_t interfaceFlags;
    /* 0x6 */ int16_t align;
    /* 0x8 */ Unique7Unk8 unk_08; // unk_08 goes unused.
} Unique7; // size = 0x10

#define CAM_FUNCDATA_UNIQ7(fov, flags) \
    { fov, CAM_DATA_FOV }, \
    { flags, CAM_DATA_FLAGS }

/** initFlags
 * & 0x00FF = atInitFlags
 * & 0xFF00 = eyeInitFlags
 * 0x1: Direct Copy of atTargetInit
 *      if initFlags & 0x6060: use head for focus point
 * 0x2: Add atTargetInit to view's lookAt
 *      if initFlags & 0x6060: use world for focus point
 * 0x3: Add atTargetInit to camera's at
 * 0x4: Don't update targets?
 * 0x8: flag to use atTagetInit as float pitch, yaw, r
 * 0x10: ? unused
 * 0x20: focus on player
*/
typedef struct {
    /* 0x00 */ uint8_t actionFlags;
    /* 0x01 */ uint8_t unk_01;
    /* 0x02 */ int16_t initFlags;
    /* 0x04 */ int16_t timerInit;
    /* 0x06 */ int16_t rollTargetInit;
    /* 0x08 */ float fovTargetInit;
    /* 0x0C */ float lerpStepScale;
    /* 0x10 */ Vec3f atTargetInit;
    /* 0x1C */ Vec3f eyeTargetInit;
} OnePointCsFull; /* size = 0x28 */

typedef struct {
    /* 0x00 */ OnePointCsFull* curKeyFrame;
    /* 0x04 */ Vec3f atTarget;
    /* 0x10 */ Vec3f eyeTarget;
    /* 0x1C */ Vec3f playerPos;
    /* 0x28 */ float fovTarget;
    /* 0x2C */ VecSph atEyeOffsetTarget;
    /* 0x34 */ int16_t rollTarget;
    /* 0x36 */ int16_t curKeyFrameIdx;
    /* 0x38 */ int16_t unk_38;
    /* 0x3A */ int16_t isNewKeyFrame;
    /* 0x3C */ int16_t keyFrameTimer;
} Unique9Anim; // size = 0x3E

typedef struct {
    /* 0x0 */ int16_t interfaceFlags;
    /* 0x4 */ Unique9Anim anim;
} Unique9; // size = 0x40

typedef struct {
    /* 0x0 */ int32_t keyFrameCnt;
    /* 0x4 */ OnePointCsFull* keyFrames;
    /* 0x8 */ Unique9 uniq9;
} Unique9OnePointCs; // size = 0x48

typedef struct {
    /* 0x0 */ float curFrame;
    /* 0x4 */ int16_t keyframe;
} Demo1Anim; // size = 0x14

typedef struct {
    /* 0x0 */ int16_t interfaceFlags;
    /* 0x4 */ Demo1Anim anim;
} Demo1; // size = 0x18

typedef struct {
    /* 0x00 */ Vec3f initialAt;
    /* 0x0C */ float unk_0C;
    /* 0x10 */ int16_t animFrame;
    /* 0x12 */ int16_t yawDir;
} Demo3Anim; // size = 0x14

typedef struct {
    /* 0x0 */ float fov;
    /* 0x4 */ float unk_04; // unused
    /* 0x8 */ int16_t interfaceFlags;
    /* 0xC */ Demo3Anim anim;
} Demo3; // size = 0x20

#define CAM_FUNCDATA_DEMO3(fov, atLerpStepScale, flags) \
    { fov, CAM_DATA_FOV }, \
    { atLerpStepScale, CAM_DATA_AT_LERP_STEP_SCALE }, \
    { flags, CAM_DATA_FLAGS }

typedef struct {
    /* 0x0 */ int16_t animTimer;
    /* 0x4 */ Vec3f atTarget;
} Demo6Anim; // size = 0x10

typedef struct {
    /* 0x0 */ int16_t interfaceFlags;
    /* 0x2 */ int16_t unk_02;
    /* 0x4 */ Demo6Anim anim;
} Demo6; // size = 0x14

typedef struct {
    /* 0x0 */ float curFrame;
    /* 0x4 */ int16_t keyframe;
    /* 0x6 */ int16_t doLERPAt;
    /* 0x8 */ int16_t finishAction;
    /* 0xA */ int16_t animTimer;
} Demo9Anim; // size = 0xC

typedef struct {
    /* 0x0 */ int16_t interfaceFlags;
    /* 0x4 */ Demo9Anim anim;
} Demo9; // size = 0x10

typedef struct {
    /* 0x0 */ CutsceneCameraPoint* atPoints;
    /* 0x4 */ CutsceneCameraPoint* eyePoints;
    /* 0x8 */ int16_t actionParameters;
    /* 0xA */ int16_t initTimer;
} OnePointCsCamera; // size = 0xC

typedef struct {
    /* 0x0 */ OnePointCsCamera onePointCs;
    /* 0xC */ Demo9 demo9;
} Demo9OnePointCs; // size = 0x1C

typedef struct {
    /* 0x0 */ float lerpAtScale;
    /* 0x4 */ int16_t interfaceFlags;
} Special0; // size = 0x8

#define CAM_FUNCDATA_SPEC0(yawUpdateRateTarget, flags) \
    { yawUpdateRateTarget, CAM_DATA_YAW_UPDATE_RATE_TARGET }, \
    { flags, CAM_DATA_FLAGS }

typedef struct {
    /* 0x0 */ int16_t initalTimer;
} Special4; // size = 0x4

typedef struct {
    /* 0x0 */ int16_t animTimer;
} Special5Anim; // size = 0x4

typedef struct {
    /* 0x00 */ float yOffset;
    /* 0x04 */ float eyeDist;
    /* 0x08 */ float minDistForRot;
    /* 0x0C */ float fovTarget;
    /* 0x10 */ float atMaxLERPScale;
    /* 0x14 */ int16_t timerInit;
    /* 0x16 */ int16_t pitch;
    /* 0x18 */ int16_t interfaceFlags;
    /* 0x1A */ int16_t unk_1A;
    /* 0x1C */ Special5Anim anim;
} Special5; // size = 0x20

#define CAM_FUNCDATA_SPEC5(yOffset, eyeDist, eyeDistNext, unk_22, pitchTarget, fov, atLerpStepScale, flags) \
    { yOffset, CAM_DATA_Y_OFFSET }, \
    { eyeDist, CAM_DATA_EYE_DIST }, \
    { eyeDistNext, CAM_DATA_EYE_DIST_NEXT }, \
    { unk_22, CAM_DATA_UNK_22 }, \
    { pitchTarget, CAM_DATA_PITCH_TARGET }, \
    { fov, CAM_DATA_FOV }, \
    { atLerpStepScale, CAM_DATA_AT_LERP_STEP_SCALE }, \
    { flags, CAM_DATA_FLAGS }

// Uses incorrect CAM_DATA values
#define CAM_FUNCDATA_SPEC5_ALT(yOffset, eyeDist, eyeDistNext, pitchTarget, fov, atLerpStepScale, unk_22, flags) \
    { yOffset, CAM_DATA_Y_OFFSET }, \
    { eyeDist, CAM_DATA_EYE_DIST }, \
    { eyeDistNext, CAM_DATA_EYE_DIST_NEXT }, \
    { pitchTarget, CAM_DATA_PITCH_TARGET }, \
    { fov, CAM_DATA_FOV }, \
    { atLerpStepScale, CAM_DATA_AT_LERP_STEP_SCALE }, \
    { unk_22, CAM_DATA_UNK_22 }, \
    { flags, CAM_DATA_FLAGS }

typedef struct {
    /* 0x0 */ int16_t idx;
} Special7; // size = 0x4

typedef struct {
    /* 0x0 */ float initalPlayerY;
    /* 0x4 */ int16_t animTimer;
} Special6Anim; // size = 0x8

typedef struct {
    /* 0x0 */ int16_t interfaceFlags;
    /* 0x4 */ Special6Anim anim;
} Special6; // size = 0xC

typedef struct {
    /* 0x0 */ int16_t targetYaw;
} Special9Anim; // size = 0x2

typedef struct {
    /* 0x0 */ float yOffset;
    /* 0x4 */ float unk_04;
    /* 0x8 */ int16_t interfaceFlags;
    /* 0xA */ int16_t unk_0A;
    /* 0xC */ Special9Anim anim;
} Special9Params; // size = 0x10

typedef struct {
    /* 0x0 */ DoorParams doorParams;
    /* 0xC */ Special9Params params;
} Special9; // size = 0x1C

#define CAM_FUNCDATA_SPEC9(yOffset, fov, flags) \
    { yOffset, CAM_DATA_Y_OFFSET }, \
    { fov, CAM_DATA_FOV }, \
    { flags, CAM_DATA_FLAGS }

typedef struct {
    /* 0x00 */ Vec3f pos;
    /* 0x0C */ Vec3f norm;
    /* 0x18 */ CollisionPoly* poly;
    /* 0x1C */ VecSph sphNorm;
    /* 0x24 */ int32_t bgId;
} CamColChk; // size = 0x28

typedef struct {
    /* 0x000 */ char paramData[0x14 * sizeof(void*)];
    /* 0x050 */ Vec3f at;
    /* 0x05C */ Vec3f eye;
    /* 0x068 */ Vec3f up;
    /* 0x074 */ Vec3f eyeNext;
    /* 0x080 */ Vec3f skyboxOffset;
    /* 0x08C */ struct PlayState* play;
    /* 0x090 */ struct Player* player;
    /* 0x094 */ PosRot playerPosRot;
    /* 0x0A8 */ struct Actor* target;
    /* 0x0AC */ PosRot targetPosRot;
    /* 0x0C0 */ float rUpdateRateInv;
    /* 0x0C4 */ float pitchUpdateRateInv;
    /* 0x0C8 */ float yawUpdateRateInv;
    /* 0x0CC */ float xzOffsetUpdateRate;
    /* 0x0D0 */ float yOffsetUpdateRate;
    /* 0x0D4 */ float fovUpdateRate;
    /* 0x0D8 */ float xzSpeed;
    /* 0x0DC */ float dist;
    /* 0x0E0 */ float speedRatio;
    /* 0x0E4 */ Vec3f posOffset;
    /* 0x0F0 */ Vec3f playerPosDelta;
    /* 0x0FC */ float fov;
    /* 0x100 */ float atLERPStepScale;
    /* 0x104 */ float playerGroundY;
    /* 0x108 */ Vec3f floorNorm;
    /* 0x114 */ float waterYPos;
    /* 0x118 */ int32_t waterPrevCamIdx;
    /* 0x11C */ int32_t waterPrevCamSetting;
    /* 0x120 */ int32_t waterQuakeId;
    /* 0x124 */ void* data0;
    /* 0x128 */ void* data1;
    /* 0x12C */ int16_t data2;
    /* 0x12E */ int16_t data3;
    /* 0x130 */ int16_t uid;
    /* 0x132 */ char unk_132[2];
    /* 0x134 */ Vec3s inputDir;
    /* 0x13A */ Vec3s camDir;
    /* 0x140 */ int16_t status;
    /* 0x142 */ int16_t setting;
    /* 0x144 */ int16_t mode;
    /* 0x146 */ int16_t bgCheckId;
    /* 0x148 */ int16_t camDataIdx;
    /* 0x14A */ int16_t unk_14A;
    /* 0x14C */ int16_t unk_14C;
    /* 0x14E */ int16_t childCamIdx;
    /* 0x150 */ int16_t waterDistortionTimer;
    /* 0x152 */ int16_t distortionFlags;
    /* 0x154 */ int16_t prevSetting;
    /* 0x156 */ int16_t nextCamDataIdx;
    /* 0x158 */ int16_t nextBGCheckId;
    /* 0x15A */ int16_t roll;
    /* 0x15C */ int16_t paramFlags;
    /* 0x15E */ int16_t animState;
    /* 0x160 */ int16_t timer;
    /* 0x162 */ int16_t parentCamIdx;
    /* 0x164 */ int16_t thisIdx;
    /* 0x166 */ int16_t prevCamDataIdx;
    /* 0x168 */ int16_t csId;
    /* 0x16A */ int16_t unk_16A;
} Camera; // size = 0x16C

#endif
