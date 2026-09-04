#ifndef Z64ITEM_H
#define Z64ITEM_H

#define MOD_NONE 0

typedef enum {
    /* 0 */ EQUIP_TYPE_SWORD,
    /* 1 */ EQUIP_TYPE_SHIELD,
    /* 2 */ EQUIP_TYPE_MAX
} EquipmentType;

// `EquipInv*` enums are for Inventory.equipment (for example used in the `CHECK_OWNED_EQUIP` macro)

typedef enum {
    EQUIP_INV_SWORD_MASTER,
    EQUIP_INV_SWORD_BIGGORON
} EquipInvSword;

typedef enum {
    EQUIP_INV_SHIELD_MIRROR
} EquipInvShield;

// `EquipValue*` enums are for ItemEquips.equipment (for example used in the `CUR_EQUIP_VALUE` macro)

typedef enum {
    EQUIP_VALUE_SWORD_NONE,
    EQUIP_VALUE_SWORD_MASTER,
    EQUIP_VALUE_SWORD_BIGGORON,
    EQUIP_VALUE_SWORD_MAX
} EquipValueSword;

typedef enum {
    EQUIP_VALUE_SHIELD_NONE,
    EQUIP_VALUE_SHIELD_MIRROR,
    EQUIP_VALUE_SHIELD_MAX
} EquipValueShield;

typedef enum ItemID {
    ITEM_NONE,
    ITEM_FISHING_POLE,
    ITEM_SWORD_MASTER,
    ITEM_SWORD_BGS,
    ITEM_BOW,
    ITEM_SHIELD_MIRROR,
    ITEM_MAX
} ItemID;

typedef enum {
    GI_NONE,
    GI_MAX
} GetItemID;

#endif
