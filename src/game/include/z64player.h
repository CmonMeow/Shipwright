#ifndef Z64PLAYER_H
#define Z64PLAYER_H

#include "z64actor.h"
#include "alignment.h"
#include "runtime/items/GetItemTypes.h"

struct Player;

// Determines behavior when spawning. See `PlayerStartMode`.
#define PLAYER_GET_START_MODE(thisx) (thisx->params & 0xF00) >> 8

typedef enum PlayerStartMode {
    /*  0 */ PLAYER_START_MODE_NOTHING, // Update is empty and draw function is NULL, nothing occurs. Useful in cutscenes, for example.
    /*  1 */ PLAYER_START_MODE_TIME_TRAVEL, // Arriving from time travel. Automatically adjusts by age.
    /*  2 */ PLAYER_START_MODE_BLUE_WARP, // Arriving from a blue warp.
    /*  3 */ PLAYER_START_MODE_DOOR, // Unused. Use a door immediately if one is nearby. If no door is in usable range, a softlock occurs.
    /*  4 */ PLAYER_START_MODE_GROTTO, // Arriving from a grotto, launched upward from the ground.
    /*  5 */ PLAYER_START_MODE_WARP_SONG, // Arriving from a warp song.
    /*  6 */ PLAYER_START_MODE_UNUSED_6,
    /*  7 */ PLAYER_START_MODE_KNOCKED_OVER, // Knocked over on the ground and flashing red.
    /*  8 */ PLAYER_START_MODE_UNUSED_8,  // Unused, behaves the same as PLAYER_START_MODE_MOVE_FORWARD_SLOW.
    /*  9 */ PLAYER_START_MODE_UNUSED_9,  // Unused, behaves the same as PLAYER_START_MODE_MOVE_FORWARD_SLOW.
    /* 10 */ PLAYER_START_MODE_UNUSED_10, // Unused, behaves the same as PLAYER_START_MODE_MOVE_FORWARD_SLOW.
    /* 11 */ PLAYER_START_MODE_UNUSED_11, // Unused, behaves the same as PLAYER_START_MODE_MOVE_FORWARD_SLOW.
    /* 12 */ PLAYER_START_MODE_UNUSED_12, // Unused, behaves the same as PLAYER_START_MODE_MOVE_FORWARD_SLOW.
    /* 13 */ PLAYER_START_MODE_IDLE, // Idle standing still, or swim if in water.
    /* 14 */ PLAYER_START_MODE_MOVE_FORWARD_SLOW, // Take a few steps forward at a slow speed (2.0f), or swim if in water.
    /* 15 */ PLAYER_START_MODE_MOVE_FORWARD, // Take a few steps forward, using the speed from the last exit (gSaveContext.entranceSpeed), or swim if in water.
    /* 16 */ PLAYER_START_MODE_MAX // Note: By default, this param has 4 bits allocated. The max value is 16.
} PlayerStartMode;

typedef enum PlayerShield {
    PLAYER_SHIELD_NONE,
    PLAYER_SHIELD_MIRROR,
    PLAYER_SHIELD_MAX
} PlayerShield;

typedef enum PlayerEnvHazard {
    /* 0x0 */ PLAYER_ENV_HAZARD_NONE,
    /* 0x1 */ PLAYER_ENV_HAZARD_HOTROOM,
    /* 0x2 */ PLAYER_ENV_HAZARD_UNDERWATER_FLOOR,
    /* 0x3 */ PLAYER_ENV_HAZARD_SWIMMING,
    /* 0x4 */ PLAYER_ENV_HAZARD_UNDERWATER_FREE
} PlayerEnvHazard;

typedef enum PlayerIdleType {
    /* -0x1 */ PLAYER_IDLE_CRIT_HEALTH = -1,
    /*  0x0 */ PLAYER_IDLE_DEFAULT,
    /*  0x1 */ PLAYER_IDLE_FIDGET
} PlayerIdleType;

typedef enum PlayerItemAction {
    PLAYER_IA_NONE,
    PLAYER_IA_FISHING_POLE,
    PLAYER_IA_SWORD_MASTER,
    PLAYER_IA_SWORD_BIGGORON,
    PLAYER_IA_BOW,
    PLAYER_IA_MAX
} PlayerItemAction;

typedef enum PlayerLimb {
    /* 0x00 */ PLAYER_LIMB_NONE,
    /* 0x01 */ PLAYER_LIMB_ROOT,
    /* 0x02 */ PLAYER_LIMB_WAIST,
    /* 0x03 */ PLAYER_LIMB_LOWER,
    /* 0x04 */ PLAYER_LIMB_R_THIGH,
    /* 0x05 */ PLAYER_LIMB_R_SHIN,
    /* 0x06 */ PLAYER_LIMB_R_FOOT,
    /* 0x07 */ PLAYER_LIMB_L_THIGH,
    /* 0x08 */ PLAYER_LIMB_L_SHIN,
    /* 0x09 */ PLAYER_LIMB_L_FOOT,
    /* 0x0A */ PLAYER_LIMB_UPPER,
    /* 0x0B */ PLAYER_LIMB_HEAD,
    /* 0x0C */ PLAYER_LIMB_HAT,
    /* 0x0D */ PLAYER_LIMB_COLLAR,
    /* 0x0E */ PLAYER_LIMB_L_SHOULDER,
    /* 0x0F */ PLAYER_LIMB_L_FOREARM,
    /* 0x10 */ PLAYER_LIMB_L_HAND,
    /* 0x11 */ PLAYER_LIMB_R_SHOULDER,
    /* 0x12 */ PLAYER_LIMB_R_FOREARM,
    /* 0x13 */ PLAYER_LIMB_R_HAND,
    /* 0x14 */ PLAYER_LIMB_SHEATH,
    /* 0x15 */ PLAYER_LIMB_TORSO,
    /* 0x16 */ PLAYER_LIMB_MAX
} PlayerLimb;

typedef enum PlayerBodyPart {
    /* 0x00 */ PLAYER_BODYPART_WAIST,      // PLAYER_LIMB_WAIST
    /* 0x01 */ PLAYER_BODYPART_R_THIGH,    // PLAYER_LIMB_R_THIGH
    /* 0x02 */ PLAYER_BODYPART_R_SHIN,     // PLAYER_LIMB_R_SHIN
    /* 0x03 */ PLAYER_BODYPART_R_FOOT,     // PLAYER_LIMB_R_FOOT
    /* 0x04 */ PLAYER_BODYPART_L_THIGH,    // PLAYER_LIMB_L_THIGH
    /* 0x05 */ PLAYER_BODYPART_L_SHIN,     // PLAYER_LIMB_L_SHIN
    /* 0x06 */ PLAYER_BODYPART_L_FOOT,     // PLAYER_LIMB_L_FOOT
    /* 0x07 */ PLAYER_BODYPART_HEAD,       // PLAYER_LIMB_HEAD
    /* 0x08 */ PLAYER_BODYPART_HAT,        // PLAYER_LIMB_HAT
    /* 0x09 */ PLAYER_BODYPART_COLLAR,     // PLAYER_LIMB_COLLAR
    /* 0x0A */ PLAYER_BODYPART_L_SHOULDER, // PLAYER_LIMB_L_SHOULDER
    /* 0x0B */ PLAYER_BODYPART_L_FOREARM,  // PLAYER_LIMB_L_FOREARM
    /* 0x0C */ PLAYER_BODYPART_L_HAND,     // PLAYER_LIMB_L_HAND
    /* 0x0D */ PLAYER_BODYPART_R_SHOULDER, // PLAYER_LIMB_R_SHOULDER
    /* 0x0E */ PLAYER_BODYPART_R_FOREARM,  // PLAYER_LIMB_R_FOREARM
    /* 0x0F */ PLAYER_BODYPART_R_HAND,     // PLAYER_LIMB_R_HAND
    /* 0x10 */ PLAYER_BODYPART_SHEATH,     // PLAYER_LIMB_SHEATH
    /* 0x11 */ PLAYER_BODYPART_TORSO,      // PLAYER_LIMB_TORSO
    /* 0x12 */ PLAYER_BODYPART_MAX
} PlayerBodyPart;

typedef enum PlayerMeleeWeaponAnimation {
    /*  0 */ PLAYER_MWA_FORWARD_SLASH_1H,
    /*  1 */ PLAYER_MWA_FORWARD_SLASH_2H,
    /*  2 */ PLAYER_MWA_FORWARD_COMBO_1H,
    /*  3 */ PLAYER_MWA_FORWARD_COMBO_2H,
    /*  4 */ PLAYER_MWA_RIGHT_SLASH_1H,
    /*  5 */ PLAYER_MWA_RIGHT_SLASH_2H,
    /*  6 */ PLAYER_MWA_RIGHT_COMBO_1H,
    /*  7 */ PLAYER_MWA_RIGHT_COMBO_2H,
    /*  8 */ PLAYER_MWA_LEFT_SLASH_1H,
    /*  9 */ PLAYER_MWA_LEFT_SLASH_2H,
    /* 10 */ PLAYER_MWA_LEFT_COMBO_1H,
    /* 11 */ PLAYER_MWA_LEFT_COMBO_2H,
    /* 12 */ PLAYER_MWA_STAB_1H,
    /* 13 */ PLAYER_MWA_STAB_2H,
    /* 14 */ PLAYER_MWA_STAB_COMBO_1H,
    /* 15 */ PLAYER_MWA_STAB_COMBO_2H,
    /* 16 */ PLAYER_MWA_FLIPSLASH_START,
    /* 17 */ PLAYER_MWA_JUMPSLASH_START,
    /* 18 */ PLAYER_MWA_FLIPSLASH_FINISH,
    /* 19 */ PLAYER_MWA_JUMPSLASH_FINISH,
    /* 20 */ PLAYER_MWA_BACKSLASH_RIGHT,
    /* 21 */ PLAYER_MWA_BACKSLASH_LEFT,
    /* 22 */ PLAYER_MWA_SPIN_ATTACK_1H,
    /* 23 */ PLAYER_MWA_SPIN_ATTACK_2H,
    /* 24 */ PLAYER_MWA_BIG_SPIN_1H,
    /* 25 */ PLAYER_MWA_BIG_SPIN_2H,
    /* 26 */ PLAYER_MWA_MAX
} PlayerMeleeWeaponAnimation;

typedef enum PlayerDoorType {
    /* -1 */ PLAYER_DOORTYPE_AJAR = -1,
    /*  0 */ PLAYER_DOORTYPE_NONE,
    /*  1 */ PLAYER_DOORTYPE_HANDLE,
    /*  2 */ PLAYER_DOORTYPE_SLIDING,
    /*  3 */ PLAYER_DOORTYPE_FAKE
} PlayerDoorType;

typedef enum PlayerModelGroup {
    PLAYER_MODELGROUP_SWORD_AND_SHIELD,
    PLAYER_MODELGROUP_DEFAULT,
    PLAYER_MODELGROUP_BGS,
    PLAYER_MODELGROUP_BOW,
    PLAYER_MODELGROUP_FISHING,
    PLAYER_MODELGROUP_MAX
} PlayerModelGroup;

typedef enum PlayerModelGroupEntry {
    /* 0x00 */ PLAYER_MODELGROUPENTRY_ANIM,
    /* 0x01 */ PLAYER_MODELGROUPENTRY_LEFT_HAND,
    /* 0x02 */ PLAYER_MODELGROUPENTRY_RIGHT_HAND,
    /* 0x03 */ PLAYER_MODELGROUPENTRY_SHEATH,
    /* 0x04 */ PLAYER_MODELGROUPENTRY_WAIST,
    /* 0x05 */ PLAYER_MODELGROUPENTRY_MAX
} PlayerModelGroupEntry;

typedef enum PlayerModelType {
    PLAYER_MODELTYPE_LH_OPEN,
    PLAYER_MODELTYPE_LH_CLOSED,
    PLAYER_MODELTYPE_LH_SWORD,
    PLAYER_MODELTYPE_LH_BGS,
    PLAYER_MODELTYPE_RH_OPEN,
    PLAYER_MODELTYPE_RH_CLOSED,
    PLAYER_MODELTYPE_RH_SHIELD,
    PLAYER_MODELTYPE_RH_BOW,
    PLAYER_MODELTYPE_SHEATH_SWORD,
    PLAYER_MODELTYPE_SHEATH_EMPTY,
    PLAYER_MODELTYPE_SHEATH_SWORD_AND_SHIELD,
    PLAYER_MODELTYPE_SHEATH_EMPTY_AND_SHIELD,
    PLAYER_MODELTYPE_WAIST,
    PLAYER_MODELTYPE_MAX
} PlayerModelType;

typedef enum PlayerAnimType {
    /* 0x00 */ PLAYER_ANIMTYPE_0,
    /* 0x01 */ PLAYER_ANIMTYPE_1,
    /* 0x02 */ PLAYER_ANIMTYPE_2,
    /* 0x03 */ PLAYER_ANIMTYPE_3,
    /* 0x04 */ PLAYER_ANIMTYPE_4,
    /* 0x05 */ PLAYER_ANIMTYPE_5,
    /* 0x06 */ PLAYER_ANIMTYPE_MAX
} PlayerAnimType;

/**
 * Temporary names, derived from original animation names in `D_80853914`
 */
typedef enum PlayerAnimGroup {
    /* 0x00 */ PLAYER_ANIMGROUP_wait,
    /* 0x01 */ PLAYER_ANIMGROUP_walk,
    /* 0x02 */ PLAYER_ANIMGROUP_run,
    /* 0x03 */ PLAYER_ANIMGROUP_damage_run,
    /* 0x04 */ PLAYER_ANIMGROUP_heavy_run,
    /* 0x05 */ PLAYER_ANIMGROUP_waitL,
    /* 0x06 */ PLAYER_ANIMGROUP_waitR,
    /* 0x07 */ PLAYER_ANIMGROUP_wait2waitR,
    /* 0x08 */ PLAYER_ANIMGROUP_normal2fighter,
    /* 0x09 */ PLAYER_ANIMGROUP_doorA_free,
    /* 0x0A */ PLAYER_ANIMGROUP_doorA,
    /* 0x0B */ PLAYER_ANIMGROUP_doorB_free,
    /* 0x0C */ PLAYER_ANIMGROUP_doorB,
    /* 0x0D */ PLAYER_ANIMGROUP_carryB,
    /* 0x0E */ PLAYER_ANIMGROUP_landing,
    /* 0x0F */ PLAYER_ANIMGROUP_short_landing,
    /* 0x10 */ PLAYER_ANIMGROUP_landing_roll,
    /* 0x11 */ PLAYER_ANIMGROUP_hip_down,
    /* 0x12 */ PLAYER_ANIMGROUP_walk_endL,
    /* 0x13 */ PLAYER_ANIMGROUP_walk_endR,
    /* 0x14 */ PLAYER_ANIMGROUP_defense,
    /* 0x15 */ PLAYER_ANIMGROUP_defense_wait,
    /* 0x16 */ PLAYER_ANIMGROUP_defense_end,
    /* 0x17 */ PLAYER_ANIMGROUP_side_walk,
    /* 0x18 */ PLAYER_ANIMGROUP_side_walkL,
    /* 0x19 */ PLAYER_ANIMGROUP_side_walkR,
    /* 0x1A */ PLAYER_ANIMGROUP_45_turn,
    /* 0x1B */ PLAYER_ANIMGROUP_waitL2wait,
    /* 0x1C */ PLAYER_ANIMGROUP_waitR2wait,
    /* 0x1D */ PLAYER_ANIMGROUP_throw,
    /* 0x1E */ PLAYER_ANIMGROUP_put,
    /* 0x1F */ PLAYER_ANIMGROUP_back_walk,
    /* 0x20 */ PLAYER_ANIMGROUP_check,
    /* 0x21 */ PLAYER_ANIMGROUP_check_wait,
    /* 0x22 */ PLAYER_ANIMGROUP_check_end,
    /* 0x23 */ PLAYER_ANIMGROUP_pull_start,
    /* 0x24 */ PLAYER_ANIMGROUP_pulling,
    /* 0x25 */ PLAYER_ANIMGROUP_pull_end,
    /* 0x26 */ PLAYER_ANIMGROUP_fall_up,
    /* 0x27 */ PLAYER_ANIMGROUP_jump_climb_hold,
    /* 0x28 */ PLAYER_ANIMGROUP_jump_climb_wait,
    /* 0x29 */ PLAYER_ANIMGROUP_jump_climb_up,
    /* 0x2A */ PLAYER_ANIMGROUP_down_slope_slip_end,
    /* 0x2B */ PLAYER_ANIMGROUP_up_slope_slip_end,
    /* 0x2C */ PLAYER_ANIMGROUP_nwait,
    /* 0x2D */ PLAYER_ANIMGROUP_MAX
} PlayerAnimGroup;

#define LIMB_BUF_COUNT(limbCount) ((ALIGN16((limbCount) * sizeof(Vec3s)) + sizeof(Vec3s) - 1) / sizeof(Vec3s))
#define PLAYER_LIMB_BUF_COUNT LIMB_BUF_COUNT(PLAYER_LIMB_MAX)

typedef enum PlayerCsAction {
    /* 0x00 */ PLAYER_CSACTION_NONE,
    /* 0x01 */ PLAYER_CSACTION_1,
    /* 0x02 */ PLAYER_CSACTION_2,
    /* 0x03 */ PLAYER_CSACTION_3,
    /* 0x04 */ PLAYER_CSACTION_4,
    /* 0x05 */ PLAYER_CSACTION_5,
    /* 0x06 */ PLAYER_CSACTION_6,
    /* 0x07 */ PLAYER_CSACTION_7,
    /* 0x08 */ PLAYER_CSACTION_8,
    /* 0x09 */ PLAYER_CSACTION_9,
    /* 0x0A */ PLAYER_CSACTION_10,
    /* 0x0B */ PLAYER_CSACTION_11,
    /* 0x0C */ PLAYER_CSACTION_12,
    /* 0x0D */ PLAYER_CSACTION_13,
    /* 0x0E */ PLAYER_CSACTION_14,
    /* 0x0F */ PLAYER_CSACTION_15,
    /* 0x10 */ PLAYER_CSACTION_16,
    /* 0x11 */ PLAYER_CSACTION_17,
    /* 0x12 */ PLAYER_CSACTION_18,
    /* 0x13 */ PLAYER_CSACTION_19,
    /* 0x14 */ PLAYER_CSACTION_20,
    /* 0x15 */ PLAYER_CSACTION_21,
    /* 0x16 */ PLAYER_CSACTION_22,
    /* 0x17 */ PLAYER_CSACTION_23,
    /* 0x18 */ PLAYER_CSACTION_24,
    /* 0x19 */ PLAYER_CSACTION_25,
    /* 0x1A */ PLAYER_CSACTION_26,
    /* 0x1B */ PLAYER_CSACTION_27,
    /* 0x1C */ PLAYER_CSACTION_28,
    /* 0x1D */ PLAYER_CSACTION_29,
    /* 0x1E */ PLAYER_CSACTION_30,
    /* 0x1F */ PLAYER_CSACTION_31,
    /* 0x20 */ PLAYER_CSACTION_32,
    /* 0x21 */ PLAYER_CSACTION_33,
    /* 0x22 */ PLAYER_CSACTION_34,
    /* 0x23 */ PLAYER_CSACTION_35,
    /* 0x24 */ PLAYER_CSACTION_36,
    /* 0x25 */ PLAYER_CSACTION_37,
    /* 0x26 */ PLAYER_CSACTION_38,
    /* 0x27 */ PLAYER_CSACTION_39,
    /* 0x28 */ PLAYER_CSACTION_40,
    /* 0x29 */ PLAYER_CSACTION_41,
    /* 0x2A */ PLAYER_CSACTION_42,
    /* 0x2B */ PLAYER_CSACTION_43,
    /* 0x2C */ PLAYER_CSACTION_44,
    /* 0x2D */ PLAYER_CSACTION_45,
    /* 0x2E */ PLAYER_CSACTION_46,
    /* 0x2F */ PLAYER_CSACTION_47,
    /* 0x30 */ PLAYER_CSACTION_48,
    /* 0x31 */ PLAYER_CSACTION_49,
    /* 0x32 */ PLAYER_CSACTION_50,
    /* 0x33 */ PLAYER_CSACTION_51,
    /* 0x34 */ PLAYER_CSACTION_52,
    /* 0x35 */ PLAYER_CSACTION_53,
    /* 0x36 */ PLAYER_CSACTION_54,
    /* 0x37 */ PLAYER_CSACTION_55,
    /* 0x38 */ PLAYER_CSACTION_56,
    /* 0x39 */ PLAYER_CSACTION_57,
    /* 0x3A */ PLAYER_CSACTION_58,
    /* 0x3B */ PLAYER_CSACTION_59,
    /* 0x3C */ PLAYER_CSACTION_60,
    /* 0x3D */ PLAYER_CSACTION_61,
    /* 0x3E */ PLAYER_CSACTION_62,
    /* 0x3F */ PLAYER_CSACTION_63,
    /* 0x40 */ PLAYER_CSACTION_64,
    /* 0x41 */ PLAYER_CSACTION_65,
    /* 0x42 */ PLAYER_CSACTION_66,
    /* 0x43 */ PLAYER_CSACTION_67,
    /* 0x44 */ PLAYER_CSACTION_68,
    /* 0x45 */ PLAYER_CSACTION_69,
    /* 0x46 */ PLAYER_CSACTION_70,
    /* 0x47 */ PLAYER_CSACTION_71,
    /* 0x48 */ PLAYER_CSACTION_72,
    /* 0x49 */ PLAYER_CSACTION_73,
    /* 0x4A */ PLAYER_CSACTION_74,
    /* 0x4B */ PLAYER_CSACTION_75,
    /* 0x4C */ PLAYER_CSACTION_76,
    /* 0x4D */ PLAYER_CSACTION_77,
    /* 0x4E */ PLAYER_CSACTION_78,
    /* 0x4F */ PLAYER_CSACTION_79,
    /* 0x50 */ PLAYER_CSACTION_80,
    /* 0x51 */ PLAYER_CSACTION_81,
    /* 0x52 */ PLAYER_CSACTION_82,
    /* 0x53 */ PLAYER_CSACTION_83,
    /* 0x54 */ PLAYER_CSACTION_84,
    /* 0x55 */ PLAYER_CSACTION_85,
    /* 0x56 */ PLAYER_CSACTION_86,
    /* 0x57 */ PLAYER_CSACTION_87,
    /* 0x58 */ PLAYER_CSACTION_88,
    /* 0x59 */ PLAYER_CSACTION_89,
    /* 0x5A */ PLAYER_CSACTION_90,
    /* 0x5B */ PLAYER_CSACTION_91,
    /* 0x5C */ PLAYER_CSACTION_92,
    /* 0x5D */ PLAYER_CSACTION_93,
    /* 0x5E */ PLAYER_CSACTION_94,
    /* 0x5F */ PLAYER_CSACTION_95,
    /* 0x60 */ PLAYER_CSACTION_96,
    /* 0x61 */ PLAYER_CSACTION_97,
    /* 0x62 */ PLAYER_CSACTION_98,
    /* 0x63 */ PLAYER_CSACTION_99,
    /* 0x64 */ PLAYER_CSACTION_100,
    /* 0x65 */ PLAYER_CSACTION_101,
    /* 0x66 */ PLAYER_CSACTION_102,
    /* 0x67 */ PLAYER_CSACTION_MAX
} PlayerCsAction;

typedef enum PlayerCueId {
    /* 0x00 */ PLAYER_CUEID_NONE,
    /* 0x01 */ PLAYER_CUEID_1,
    /* 0x02 */ PLAYER_CUEID_2,
    /* 0x03 */ PLAYER_CUEID_3,
    /* 0x04 */ PLAYER_CUEID_4,
    /* 0x05 */ PLAYER_CUEID_5,
    /* 0x06 */ PLAYER_CUEID_6,
    /* 0x07 */ PLAYER_CUEID_7,
    /* 0x08 */ PLAYER_CUEID_8,
    /* 0x09 */ PLAYER_CUEID_9,
    /* 0x0A */ PLAYER_CUEID_10,
    /* 0x0B */ PLAYER_CUEID_11,
    /* 0x0C */ PLAYER_CUEID_12,
    /* 0x0D */ PLAYER_CUEID_13,
    /* 0x0E */ PLAYER_CUEID_14,
    /* 0x0F */ PLAYER_CUEID_15,
    /* 0x10 */ PLAYER_CUEID_16,
    /* 0x11 */ PLAYER_CUEID_17,
    /* 0x12 */ PLAYER_CUEID_18,
    /* 0x13 */ PLAYER_CUEID_19,
    /* 0x14 */ PLAYER_CUEID_20,
    /* 0x15 */ PLAYER_CUEID_21,
    /* 0x16 */ PLAYER_CUEID_22,
    /* 0x17 */ PLAYER_CUEID_23,
    /* 0x18 */ PLAYER_CUEID_24,
    /* 0x19 */ PLAYER_CUEID_25,
    /* 0x1A */ PLAYER_CUEID_26,
    /* 0x1B */ PLAYER_CUEID_27,
    /* 0x1C */ PLAYER_CUEID_28,
    /* 0x1D */ PLAYER_CUEID_29,
    /* 0x1E */ PLAYER_CUEID_30,
    /* 0x1F */ PLAYER_CUEID_31,
    /* 0x20 */ PLAYER_CUEID_32,
    /* 0x21 */ PLAYER_CUEID_33,
    /* 0x22 */ PLAYER_CUEID_34,
    /* 0x23 */ PLAYER_CUEID_35,
    /* 0x24 */ PLAYER_CUEID_36,
    /* 0x25 */ PLAYER_CUEID_37,
    /* 0x26 */ PLAYER_CUEID_38,
    /* 0x27 */ PLAYER_CUEID_39,
    /* 0x28 */ PLAYER_CUEID_40,
    /* 0x29 */ PLAYER_CUEID_41,
    /* 0x2A */ PLAYER_CUEID_42,
    /* 0x2B */ PLAYER_CUEID_43,
    /* 0x2C */ PLAYER_CUEID_44,
    /* 0x2D */ PLAYER_CUEID_45,
    /* 0x2E */ PLAYER_CUEID_46,
    /* 0x2F */ PLAYER_CUEID_47,
    /* 0x30 */ PLAYER_CUEID_48,
    /* 0x31 */ PLAYER_CUEID_49,
    /* 0x32 */ PLAYER_CUEID_50,
    /* 0x33 */ PLAYER_CUEID_51,
    /* 0x34 */ PLAYER_CUEID_52,
    /* 0x35 */ PLAYER_CUEID_53,
    /* 0x36 */ PLAYER_CUEID_54,
    /* 0x37 */ PLAYER_CUEID_55,
    /* 0x38 */ PLAYER_CUEID_56,
    /* 0x39 */ PLAYER_CUEID_57,
    /* 0x3A */ PLAYER_CUEID_58,
    /* 0x3B */ PLAYER_CUEID_59,
    /* 0x3C */ PLAYER_CUEID_60,
    /* 0x3D */ PLAYER_CUEID_61,
    /* 0x3E */ PLAYER_CUEID_62,
    /* 0x3F */ PLAYER_CUEID_63,
    /* 0x40 */ PLAYER_CUEID_64,
    /* 0x41 */ PLAYER_CUEID_65,
    /* 0x42 */ PLAYER_CUEID_66,
    /* 0x43 */ PLAYER_CUEID_67,
    /* 0x44 */ PLAYER_CUEID_68,
    /* 0x45 */ PLAYER_CUEID_69,
    /* 0x46 */ PLAYER_CUEID_70,
    /* 0x47 */ PLAYER_CUEID_71,
    /* 0x48 */ PLAYER_CUEID_72,
    /* 0x49 */ PLAYER_CUEID_73,
    /* 0x4A */ PLAYER_CUEID_74,
    /* 0x4B */ PLAYER_CUEID_75,
    /* 0x4C */ PLAYER_CUEID_76,
    /* 0x4D */ PLAYER_CUEID_77,
    /* 0x4E */ PLAYER_CUEID_MAX
} PlayerCueId;

typedef enum PlayerLedgeClimbType {
    /* 0 */ PLAYER_LEDGE_CLIMB_NONE,
    /* 1 */ PLAYER_LEDGE_CLIMB_1,
    /* 2 */ PLAYER_LEDGE_CLIMB_2,
    /* 3 */ PLAYER_LEDGE_CLIMB_3,
    /* 4 */ PLAYER_LEDGE_CLIMB_4
} PlayerLedgeClimbType;

typedef enum PlayerStickDirection {
    /* -1 */ PLAYER_STICK_DIR_NONE = -1,
    /*  0 */ PLAYER_STICK_DIR_FORWARD,
    /*  1 */ PLAYER_STICK_DIR_LEFT,
    /*  2 */ PLAYER_STICK_DIR_BACKWARD,
    /*  3 */ PLAYER_STICK_DIR_RIGHT
} PlayerStickDirection;

typedef enum {
    /* 0 */ PLAYER_KNOCKBACK_NONE, // No knockback
    /* 1 */ PLAYER_KNOCKBACK_SMALL, // A small hop, remains standing up
    /* 2 */ PLAYER_KNOCKBACK_LARGE, // Sent flying in the air and lands laying down on the floor
    /* 3 */ PLAYER_KNOCKBACK_LARGE_SHOCK // Same as`PLAYER_KNOCKBACK_LARGE` with a shock effect
} PlayerKnockbackType;

typedef enum {
    /* 0 */ PLAYER_HIT_RESPONSE_NONE,
    /* 1 */ PLAYER_HIT_RESPONSE_KNOCKBACK_LARGE,
    /* 2 */ PLAYER_HIT_RESPONSE_KNOCKBACK_SMALL,
    /* 3 */ PLAYER_HIT_RESPONSE_ICE_TRAP,
    /* 4 */ PLAYER_HIT_RESPONSE_ELECTRIC_SHOCK
} PlayerDamageResponseType;

typedef struct PlayerAgeProperties {
    /* 0x00 */ float ceilingCheckHeight;
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
    /* 0x30 */ float unk_30;
    /* 0x34 */ float unk_34;
    /* 0x38 */ float wallCheckRadius;
    /* 0x3C */ float unk_3C;
    /* 0x40 */ float unk_40;
    /* 0x44 */ Vec3s unk_44;
    /* 0x4A */ Vec3s unk_4A[4];
    /* 0x62 */ Vec3s unk_62[4];
    /* 0x7A */ Vec3s unk_7A[2];
    /* 0x86 */ Vec3s unk_86[2];
    /* 0x92 */ uint16_t unk_92;
    /* 0x94 */ uint16_t unk_94;
    /* 0x98 */ LinkAnimationHeader* unk_98;
    /* 0x9C */ LinkAnimationHeader* unk_9C;
    /* 0xA0 */ LinkAnimationHeader* unk_A0;
    /* 0xA4 */ LinkAnimationHeader* unk_A4;
    /* 0xA8 */ LinkAnimationHeader* unk_A8;
    /* 0xAC */ LinkAnimationHeader* unk_AC[4];
    /* 0xBC */ LinkAnimationHeader* unk_BC[2];
    /* 0xC4 */ LinkAnimationHeader* unk_C4[2];
    /* 0xCC */ LinkAnimationHeader* unk_CC[2];
} PlayerAgeProperties; // size = 0xD4

typedef struct WeaponInfo {
    /* 0x00 */ int32_t active;
    /* 0x04 */ Vec3f tip;
    /* 0x10 */ Vec3f base;
} WeaponInfo; // size = 0x1C

// #region SOH [General]
// Supporting pendingFlag
// Upstream TODO: Rename these to be more obviously SoH specific
typedef enum FlagType {
    FLAG_NONE,
    FLAG_SCENE_SWITCH,
    FLAG_SCENE_TREASURE,
    FLAG_SCENE_CLEAR,
    FLAG_SCENE_COLLECTIBLE,
    FLAG_EVENT_CHECK_INF,
    FLAG_ITEM_GET_INF,
    FLAG_INF_TABLE,
    FLAG_EVENT_INF,
    FLAG_GS_TOKEN,
} FlagType;

typedef struct PendingFlag {
    /* 0x00 */ int32_t flagID;     // which flag to set when Player_SetPendingFlag is called
    /* 0x04 */ FlagType flagType;  // type of flag to set when Player_SetPendingFlag is called
} PendingFlag; // size = 0x06
// #endregion

#define PLAYER_STATE1_LOADING (1 << 0) //Transitioning to a new scene
#define PLAYER_STATE1_ITEM_IN_HAND (1 << 3)
#define PLAYER_STATE1_HOSTILE_LOCK_ON (1 << 4) // Currently locked onto a hostile actor. Triggers a "battle" variant of many actions.
#define PLAYER_STATE1_INPUT_DISABLED (1 << 5)
#define PLAYER_STATE1_TALKING (1 << 6) // Currently talking to an actor. This includes item exchanges.
#define PLAYER_STATE1_DEAD (1 << 7) // Player has died. Note that this gets set when the death cutscene has started, after landing from the air.
#define PLAYER_STATE1_START_CHANGING_HELD_ITEM (1 << 8) // Item change process has begun
#define PLAYER_STATE1_READY_TO_FIRE (1 << 9)
#define PLAYER_STATE1_GETTING_ITEM (1 << 10)
#define PLAYER_STATE1_CARRYING_ACTOR (1 << 11) // Currently carrying an actor
#define PLAYER_STATE1_CHARGING_SPIN_ATTACK (1 << 12) // Currently charing a spin attack (by holding down the B button)
#define PLAYER_STATE1_HANGING_OFF_LEDGE (1 << 13)
#define PLAYER_STATE1_CLIMBING_LEDGE (1 << 14)
#define PLAYER_STATE1_Z_TARGETING (1 << 15) // Either lock-on or parallel is active. This flag is never checked for and is practically unused.
#define PLAYER_STATE1_FRIENDLY_ACTOR_FOCUS (1 << 16) // Currently focusing on a friendly actor. Includes friendly lock-on, talking, and more. Usually does not include hostile actor lock-on, see `PLAYER_STATE1_HOSTILE_LOCK_ON`.
#define PLAYER_STATE1_PARALLEL (1 << 17) // "Parallel" mode, Z-Target without an actor lock-on
#define PLAYER_STATE1_JUMPING (1 << 18)
#define PLAYER_STATE1_FREEFALL (1 << 19)
#define PLAYER_STATE1_FIRST_PERSON (1 << 20)
#define PLAYER_STATE1_CLIMBING_LADDER (1 << 21)
#define PLAYER_STATE1_SHIELDING (1 << 22)
#define PLAYER_STATE1_DAMAGED (1 << 26)
#define PLAYER_STATE1_IN_WATER (1 << 27)
#define PLAYER_STATE1_IN_ITEM_CS (1 << 28)
#define PLAYER_STATE1_IN_CUTSCENE (1 << 29)
#define PLAYER_STATE1_LOCK_ON_FORCED_TO_RELEASE (1 << 30) // Lock-on was released automatically, for example by leaving the lock-on leash range
#define PLAYER_STATE1_FLOOR_DISABLED (1 << 31) //Used for grottos

#define PLAYER_STATE2_DO_ACTION_GRAB (1 << 0)
#define PLAYER_STATE2_CAN_ACCEPT_TALK_OFFER (1 << 1) // Can accept a talk offer. "Speak" or "Check" is shown on the A button.
#define PLAYER_STATE2_DO_ACTION_CLIMB (1 << 2)
#define PLAYER_STATE2_FOOTSTEP (1 << 3)
#define PLAYER_STATE2_MOVING_DYNAPOLY (1 << 4)
#define PLAYER_STATE2_DISABLE_ROTATION_Z_TARGET (1 << 5)
#define PLAYER_STATE2_DISABLE_ROTATION_ALWAYS (1 << 6)
#define PLAYER_STATE2_GRABBED_BY_ENEMY (1 << 7)
#define PLAYER_STATE2_GRABBING_DYNAPOLY (1 << 8)
#define PLAYER_STATE2_FORCE_SAND_FLOOR_SOUND (1 << 9) // Forces sand footstep sounds regardless of current floor type
#define PLAYER_STATE2_UNDERWATER (1 << 10)
#define PLAYER_STATE2_DIVING (1 << 11)
#define PLAYER_STATE2_STATIONARY_LADDER (1 << 12)
#define PLAYER_STATE2_LOCK_ON_WITH_SWITCH (1 << 13) // Actor lock-on is active, specifically with Switch Targeting. Hold Targeting checks the state of the Z button instead of this flag.
#define PLAYER_STATE2_FROZEN (1 << 14)
#define PLAYER_STATE2_DO_ACTION_ENTER (1 << 16) // Sets the "Enter On A" DoAction
#define PLAYER_STATE2_SPIN_ATTACKING (1 << 17) //w/o magic
#define PLAYER_STATE2_CRAWLING (1 << 18) // Crawling through a crawlspace
#define PLAYER_STATE2_HOPPING (1 << 19) //Sidehop/backflip
#define PLAYER_STATE2_NAVI_ACTIVE (1 << 20) // Navi is visible and active. Could be hovering idle near Link or hovering over other actors.
#define PLAYER_STATE2_NAVI_ALERT (1 << 21)
#define PLAYER_STATE2_DO_ACTION_DOWN (1 << 22)
#define PLAYER_STATE2_REFLECTION (1 << 26) //Handles Dark Link's Reflection
#define PLAYER_STATE2_IDLE_FIDGET (1 << 28) // Playing a fidget idle animation (under typical circumstances, see `Player_ChooseNextIdleAnim` for more info)
#define PLAYER_STATE2_DISABLE_DRAW (1 << 29)
#define PLAYER_STATE2_SWORD_LUNGE (1 << 30)
#define PLAYER_STATE2_FORCED_VOID_OUT (1 << 31)

#define PLAYER_STATE3_IGNORE_CEILING_FLOOR_WATER (1 << 0)
#define PLAYER_STATE3_MIDAIR (1 << 1)
#define PLAYER_STATE3_FINISHED_ATTACKING (1 << 3)
#define PLAYER_STATE3_CHECK_FLOOR_WATER_COLLISION (1 << 4)
#define PLAYER_STATE3_UNUSED_6 (1 << 6)

typedef void (*PlayerActionFunc)(struct Player*, struct PlayState*);
typedef int32_t (*UpperActionFunc)(struct Player*, struct PlayState*);
typedef void (*AfterPutAwayFunc)(struct PlayState*, struct Player*);

typedef struct PlayerPresentationDrawData {
    uint8_t modelGroup;
    uint8_t shield;
    uint8_t itemAction;
    uint8_t fishingState;
    uint8_t bowReady;
    uint8_t blocking;
    uint8_t pad[2];
    Vec3s upperLimbRot;
    Vec3s headLimbRot;
    float fishingRodBendY;
    float fishingRodBendX;
    float fishingRodTwist;
    float fishingRodCastX;
    float bowStringScale;
    void* bowArrowSkelAnime;
    Vec3f limbOrigins[PLAYER_LIMB_MAX];
    Vec3f fishingRodTip;
} PlayerPresentationDrawData;

#define UNK6AE_ROT_FOCUS_X (1 << 0)
#define UNK6AE_ROT_FOCUS_Y (1 << 1)
#define UNK6AE_ROT_FOCUS_Z (1 << 2)
#define UNK6AE_ROT_HEAD_X (1 << 3)
#define UNK6AE_ROT_HEAD_Y (1 << 4)
#define UNK6AE_ROT_HEAD_Z (1 << 5)
#define UNK6AE_ROT_UPPER_X (1 << 6)
#define UNK6AE_ROT_UPPER_Y (1 << 7)
#define UNK6AE_ROT_UPPER_Z (1 << 8)

typedef struct Player {
    /* 0x0000 */ Actor actor;
    /* 0x014D */ int8_t currentSwordItemId;
    /* 0x014E */ int8_t currentShield; // current shield from `PlayerShield`
    /* 0x0150 */ int8_t heldItemButton; // Button index for the item currently used
    /* 0x0151 */ int8_t heldItemAction; // Item action for the item currently used
    /* 0x0152 */ uint8_t heldItemId; // Item id for the item currently used
    /* 0x0154 */ int8_t itemAction; // the difference between this and heldItemAction is unclear
    /* 0x0155 */ char unk_155[0x003];
    /* 0x0158 */ uint8_t modelGroup;
    /* 0x0159 */ uint8_t nextModelGroup;
    /* 0x015A */ int8_t itemChangeType;
    /* 0x015B */ uint8_t modelAnimType;
    /* 0x015C */ uint8_t leftHandType;
    /* 0x015D */ uint8_t rightHandType;
    /* 0x015E */ uint8_t sheathType;
    /* 0x0160 */ Gfx** rightHandDLists;
    /* 0x0164 */ Gfx** leftHandDLists;
    /* 0x0168 */ Gfx** sheathDLists;
    /* 0x016C */ Gfx** waistDLists;
    /* 0x0170 */ uint8_t giObjectLoading;
    /* 0x0174 */ DmaRequest giObjectDmaRequest;
    /* 0x0194 */ OSMesgQueue giObjectLoadQueue;
    /* 0x01AC */ OSMesg giObjectLoadMsg;
    /* 0x01B0 */ void* giObjectSegment; // also used for title card textures
    /* 0x01B4 */ SkelAnime skelAnime;
    /* 0x01F8 */ Vec3s jointTable[PLAYER_LIMB_BUF_COUNT];
    /* 0x0288 */ Vec3s morphTable[PLAYER_LIMB_BUF_COUNT];
    /* 0x0318 */ Vec3s blendTable[PLAYER_LIMB_BUF_COUNT];
    /* 0x03A8 */ int16_t unk_3A8[2];
    /* 0x03AC */ Actor* heldActor;
    /* 0x03B0 */ Vec3f leftHandPos;
    /* 0x03BC */ Vec3s unk_3BC;
    /* 0x03C4 */ Actor* unk_3C4;
    /* 0x03C8 */ Vec3f unk_3C8;
    /* 0x03D4 */ char unk_3D4[0x058];
    /* 0x042C */ int8_t doorType;
    /* 0x042D */ int8_t doorDirection;
    /* 0x042E */ int16_t doorTimer;
    /* 0x0430 */ Actor* doorActor;
    /* 0x0434 */ int16_t getItemId; // Upstream TODO: Document why this is int16_t while it's int8_t upstream
    /* 0x0436 */ uint16_t getItemDirection;
    /* 0x0438 */ Actor* interactRangeActor;
    /* 0x043D */ char unk_43D[0x003];
    /* 0x0444 */ uint8_t csAction;
    /* 0x0445 */ uint8_t prevCsAction;
    /* 0x0446 */ uint8_t cueId;
    /* 0x0447 */ uint8_t unk_447;
    /* 0x0448 */ Actor* csActor; // Actor involved in a `csAction`. Typically the actor that invoked the cutscene.
    /* 0x044C */ char unk_44C[0x004];
    /* 0x0450 */ Vec3f unk_450;
    /* 0x045C */ Vec3f unk_45C;
    /* 0x0468 */ char unk_468[0x002];
    /* 0x046A */ union {
        int16_t haltActorsDuringCsAction; // If true, halt actors belonging to certain categories during a `csAction`
        int16_t slidingDoorBgCamIndex; // `BgCamIndex` used during a sliding door cutscene
    } cv; // "Cutscene Variable": context dependent variable that has different meanings depending on what function is called
    /* 0x046C */ int16_t subCamId;
    /* 0x046E */ char unk_46E[0x02A];
    /* 0x0498 */ ColliderCylinder cylinder;
    /* 0x04E4 */ ColliderQuad meleeWeaponQuads[2];
    /* 0x05E4 */ ColliderTris shieldCollider;
    /* 0x0664 */ Actor* focusActor; // Actor that Player and the camera are looking at; Used for lock-on, talking, and more
    /* 0x0668 */ char unk_668[0x004];
    /* 0x066C */ int32_t zTargetActiveTimer; // Non-zero values indicate Z-Targeting should update; Values under 5 indicate lock-on is releasing
    /* 0x0674 */ PlayerActionFunc actionFunc;
    /* 0x0678 */ PlayerAgeProperties* ageProperties;
    /* 0x067C */ uint32_t stateFlags1;
    /* 0x0680 */ uint32_t stateFlags2;
    /* 0x0684 */ Actor* autoLockOnActor; // Actor that is locked onto automatically without player input; see `Player_SetAutoLockOnActor`
    /* 0x068C */ Actor* naviActor;
    /* 0x0690 */ int16_t naviTextId;
    /* 0x0692 */ uint8_t stateFlags3;
    /* 0x0694 */ Actor* talkActor; // Actor offering to talk, or currently talking to, depending on context
    /* 0x0698 */ float talkActorDistance; // xz distance away from `talkActor`
    /* 0x069C */ char unk_69C[0x004];
    /* 0x06A8 */ Actor* unk_6A8;
    /* 0x06AC */ int8_t idleType;
    /* 0x06AD */ uint8_t unk_6AD;
    /* 0x06AE */ uint16_t unk_6AE_rotFlags; // See `UNK6AE_ROT_` macros. If its flag isn't set, a rot steps to 0.
    /* 0x06B0 */ int16_t upperLimbYawSecondary;
    /* 0x06B2 */ char unk_6B4[0x004];
    /* 0x06B6 */ Vec3s headLimbRot;
    /* 0x06BC */ Vec3s upperLimbRot;
    /* 0x06C2 */ int16_t unk_6C2;
    /* 0x06C4 */ float unk_6C4;
    /* 0x06C8 */ SkelAnime upperSkelAnime;
    /* 0x070C */ Vec3s upperJointTable[PLAYER_LIMB_BUF_COUNT];
    /* 0x079C */ Vec3s upperMorphTable[PLAYER_LIMB_BUF_COUNT];
    /* 0x082C */ UpperActionFunc upperActionFunc;
    /* 0x0830 */ float upperAnimInterpWeight;
    /* 0x0834 */ int16_t unk_834;
    /* 0x0836 */ int8_t unk_836;
    /* 0x0837 */ uint8_t putAwayCooldownTimer;
    /* 0x0838 */ float linearVelocity; // Controls horizontal speed, used for `actor.speed`. Current or target value depending on context.
    /* 0x083C */ int16_t yaw; // General yaw value, used both for world and shape rotation. Current or target value depending on context.
    /* 0x083E */ int16_t parallelYaw; // yaw in "parallel" mode, Z-Target without an actor lock-on
    /* 0x0840 */ uint16_t underwaterTimer;
    /* 0x0842 */ int8_t meleeWeaponAnimation;
    /* 0x0843 */ int8_t meleeWeaponState;
    /* 0x0844 */ int8_t unk_844;
    /* 0x0845 */ uint8_t unk_845;
    /* 0x0846 */ uint8_t controlStickDataIndex; // cycles between 0 - 3. Used to index `controlStickSpinAngles` and `controlStickDirections`
    /* 0x0847 */ int8_t controlStickSpinAngles[4]; // Stores a modified version of the control stick angle for the last 4 frames. Used for checking spins.
    /* 0x084B */ int8_t controlStickDirections[4]; // Stores the control stick direction (relative to shape yaw) for the last 4 frames. See `PlayerStickDirection`.

    /* 0x084F */ union {
        int8_t actionVar1;
        int8_t facingUpSlope; // Player_Action_SlideOnSlope: facing uphill when sliding on a slope
    } av1; // "Action Variable 1": context dependent variable that has different meanings depending on what action is currently running

    /* 0x0850 */ union {
        int16_t actionVar2;
        int16_t fallDamageStunTimer; // Player_Action_Idle: Prevents any movement and shakes model up and down quickly to indicate fall damage stun
        int16_t bonked; // Player_Action_Roll: set to true after bonking into a wall or an actor
    } av2; // "Action Variable 2": context dependent variable that has different meanings depending on what action is currently running

    /* 0x0854 */ float unk_854;
    /* 0x0858 */ float unk_858;
    /* 0x085C */ float unk_85C; // stick length among other things
    /* 0x0860 */ int16_t unk_860; // stick flame timer among other things
    /* 0x0862 */ int16_t unk_862; // get item draw ID + 1
    /* 0x0864 */ float unk_864;
    /* 0x0868 */ float unk_868;
    /* 0x086C */ float unk_86C;
    /* 0x0870 */ float unk_870;
    /* 0x0874 */ float unk_874;
    /* 0x0878 */ float unk_878;
    /* 0x087C */ int16_t unk_87C;
    /* 0x087E */ int16_t turnRate; // Amount angle is changed every frame when turning in place
    /* 0x0880 */ float unk_880;
    /* 0x0884 */ float yDistToLedge; // y distance to ground above an interact wall. LEDGE_DIST_MAX if no ground is found
    /* 0x0888 */ float distToInteractWall; // xyz distance to the interact wall
    /* 0x088C */ uint8_t ledgeClimbType;
    /* 0x088D */ uint8_t ledgeClimbDelayTimer;
    /* 0x088E */ uint8_t textboxBtnCooldownTimer; // Prevents usage of A/B/C-up when counting down
    /* 0x088F */ uint8_t damageFlickerAnimCounter; // Used to flicker Link after taking damage
    /* 0x0890 */ uint8_t unk_890;
    /* 0x0891 */ uint8_t bodyShockTimer;
    /* 0x0892 */ uint8_t unk_892;
    /* 0x0893 */ uint8_t hoverBootsTimer;
    /* 0x0894 */ int16_t fallStartHeight; // last truncated Y position before falling
    /* 0x0896 */ int16_t fallDistance; // truncated Y distance the player has fallen so far (positive is down)
    /* 0x0898 */ int16_t floorPitch; // angle of the floor slope in the direction of current world yaw (positive for ascending slope)
    /* 0x089A */ int16_t floorPitchAlt; // the calculation for this value is bugged and doesn't represent anything meaningful
    /* 0x089C */ int16_t unk_89C;
    /* 0x089E */ uint16_t floorSfxOffset;
    /* 0x08A0 */ uint8_t knockbackDamage;
    /* 0x08A1 */ uint8_t knockbackType;
    /* 0x08A2 */ int16_t knockbackRot;
    /* 0x08A4 */ float knockbackSpeed;
    /* 0x08A8 */ float knockbackYVelocity;
    /* 0x08AC */ float pushedSpeed; // Pushing player, examples include water currents, floor conveyors, climbing sloped surfaces
    /* 0x08B0 */ int16_t pushedYaw; // Yaw direction of player being pushed
    /* 0x08B4 */ WeaponInfo meleeWeaponInfo[3];
    /* 0x0908 */ Vec3f bodyPartsPos[PLAYER_BODYPART_MAX];
    /* 0x09E0 */ MtxF mf_9E0;
    /* 0x0A20 */ MtxF shieldMf;
    /* 0x0A60 */ uint8_t bodyIsBurning;
    /* 0x0A61 */ uint8_t bodyFlameTimers[PLAYER_BODYPART_MAX]; // one flame per body part
    /* 0x0A73 */ uint8_t unk_A73;
    /* 0x0A74 */ AfterPutAwayFunc afterPutAwayFunc; // See `Player_SetupWaitForPutAway` and `Player_Action_WaitForPutAway`
    /* 0x0A78 */ int8_t invincibilityTimer; // prevents damage when nonzero. Positive values are intangibility, negative are invulnerability
    /* 0x0A79 */ uint8_t floorTypeTimer; // counts up every frame the current floor type is the same as the last frame
    /* 0x0A7A */ uint8_t floorProperty;
    /* 0x0A7B */ uint8_t prevFloorType;
    /* 0x0A7C */ float prevControlStickMagnitude;
    /* 0x0A80 */ int16_t prevControlStickAngle;
    /* 0x0A82 */ uint16_t prevFloorSfxOffset;
    /* 0x0A84 */ int16_t unk_A84;
    /* 0x0A86 */ int8_t unk_A86;
    /* 0x0A87 */ uint8_t unk_A87;
    /* 0x0A88 */ Vec3f unk_A88; // previous body part 0 position
    // #region SOH [General]
    // Upstream TODO: Rename these to be more obviously SoH specific
    /*        */ PendingFlag pendingFlag;
    /*        */ GetItemEntry getItemEntry;
    // True only while an authoritative retained corpse owns this exact local
    // player incarnation's body presentation.
    /*        */ uint8_t authoritativeBodyHidden;
    // #endregion
} Player;

#endif
