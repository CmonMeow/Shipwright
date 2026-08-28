#ifndef Z64SCENE_H
#define Z64SCENE_H

#include "libultraship/libultra.h"
#include "z64math.h"

typedef struct {
    /* 0x00 */ uintptr_t vromStart;
    /* 0x04 */ uintptr_t vromEnd;
    char* fileName;
} RomFile; // size = 0x8

typedef struct {
    /* 0x00 */ RomFile sceneFile;
    /* 0x08 */ RomFile titleFile;
    /* 0x10 */ uint8_t  unk_10;
    /* 0x11 */ uint8_t  config;
    /* 0x12 */ uint8_t  unk_12;
    /* 0x13 */ uint8_t  unk_13;
} SceneTableEntry; // size = 0x14
typedef struct {
    uint8_t headerType;
} MeshHeaderBase;

typedef struct {
    MeshHeaderBase base;
    uint8_t numEntries;
    Gfx* dListStart;
    Gfx* dListEnd;
} MeshHeader0;

typedef struct {
    Gfx* opaqueDList;
    Gfx* translucentDList;
} MeshEntry0;

typedef struct {
    MeshHeaderBase base;
    uint8_t format;
    uint32_t entryRecord;
} MeshHeader1Base;

typedef struct {
    MeshHeader1Base base;
    void* imagePtr; // 0x08
    uint32_t unknown; // 0x0C
    uint32_t unknown2; // 0x10
    uint16_t bgWidth; // 0x14
    uint16_t bgHeight; // 0x16
    uint8_t imageFormat; // 0x18
    uint8_t imageSize; // 0x19
    uint16_t imagePal; // 0x1A
    uint16_t imageFlip; // 0x1C
} MeshHeader1Single;

typedef struct {
    MeshHeader1Base base;
    uint8_t bgCnt;
    void* bgRecordPtr;
} MeshHeader1Multi;

typedef struct {
    uint16_t unknown; // 0x00
    int8_t bgID; // 0x02
    void* imagePtr; // 0x04
    uint32_t unknown2; // 0x08
    uint32_t unknown3; // 0x0C
    uint16_t bgWidth; // 0x10
    uint16_t bgHeight; // 0x12
    uint8_t imageFmt; // 0x14
    uint8_t imageSize; // 0x15
    uint16_t imagePal; // 0x16
    uint16_t imageFlip; // 0x18
} BackgroundRecord;

typedef struct {
    int16_t playerXMax, playerZMax;
    int16_t playerXMin, playerZMin;
    Gfx* opaqueDList;
    Gfx* translucentDList;
} MeshEntry2;

typedef struct {
    MeshHeaderBase base;
    uint8_t numEntries;
    Gfx* dListStart;
    Gfx* dListEnd;
} MeshHeader2;

typedef struct {
    /* 0x00 */ uint8_t ambientColor[3];
    /* 0x03 */ int8_t diffuseDir1[3];
    /* 0x06 */ uint8_t diffuseColor1[3];
    /* 0x09 */ int8_t diffuseDir2[3];
    /* 0x0C */ uint8_t diffuseColor2[3];
    /* 0x0F */ uint8_t fogColor[3];
    /* 0x12 */ uint16_t fogNear;
    /* 0x14 */ uint16_t fogFar;
} LightSettings; // size = 0x16

typedef struct {
    /* 0x00 */ uint8_t count; // number of points in the path
    /* 0x04 */ Vec3s* points; // Segment Address to the array of points
} Path; // size = 0x8


#define DEFINE_SCENE(_0, _1, enum, _3, _4, _5) enum,

#ifdef __cplusplus
enum SceneID : int {
#else
enum SceneID {
#endif
    #include "tables/scene_table.h"
    /* 0x6E */ SCENE_ID_MAX
};

// this define exists to preserve shiftability for an unused scene that is
// listed in the entrance table
#define SCENE_UNUSED_6E SCENE_ID_MAX

#undef DEFINE_SCENE

// Entrance Index Enum
#define DEFINE_ENTRANCE(enum, _1, _2, _3, _4, _5, _6) enum,

typedef enum {
    #include "tables/entrance_table.h"
    /* 0x614 */ ENTR_MAX
} EntranceIndex;

#define ENTR_LOAD_OPENING -1

typedef enum {
    /* 0x7FF9 */ ENTR_RETURN_YOUSEI_IZUMI_YOKO = 0x7FF9, // Great Fairy Fountain (spells)
    /* 0x7FFA */ ENTR_RETURN_SYATEKIJYOU, // Shooting gallery
    /* 0x7FFB */ ENTR_RETURN_2, // unused
    /* 0x7FFC */ ENTR_RETURN_SHOP1, // Bazaar
    /* 0x7FFD */ ENTR_RETURN_4, // unused
    /* 0x7FFE */ ENTR_RETURN_DAIYOUSEI_IZUMI, // Great Fairy Fountain (magic, double magic, double defense)
    /* 0x7FFF */ ENTR_RETURN_GROTTO // Grottos and normal Fairy Fountain
} ReturnEntranceIndex;

#undef DEFINE_ENTRANCE

typedef enum {
    /*  0 */ SDC_DEFAULT,
    /*  1 */ SDC_HYRULE_FIELD,
    /*  2 */ SDC_KAKARIKO_VILLAGE,
    /*  3 */ SDC_ZORAS_RIVER,
    /*  4 */ SDC_KOKIRI_FOREST,
    /*  5 */ SDC_LAKE_HYLIA,
    /*  6 */ SDC_ZORAS_DOMAIN,
    /*  7 */ SDC_ZORAS_FOUNTAIN,
    /*  8 */ SDC_GERUDO_VALLEY,
    /*  9 */ SDC_LOST_WOODS,
    /* 10 */ SDC_DESERT_COLOSSUS,
    /* 11 */ SDC_GERUDOS_FORTRESS,
    /* 12 */ SDC_HAUNTED_WASTELAND,
    /* 13 */ SDC_HYRULE_CASTLE,
    /* 14 */ SDC_DEATH_MOUNTAIN_TRAIL,
    /* 15 */ SDC_DEATH_MOUNTAIN_CRATER,
    /* 16 */ SDC_GORON_CITY,
    /* 17 */ SDC_LON_LON_RANCH,
    /* 18 */ SDC_FIRE_TEMPLE,
    /* 19 */ SDC_DEKU_TREE,
    /* 20 */ SDC_DODONGOS_CAVERN,
    /* 21 */ SDC_JABU_JABU,
    /* 22 */ SDC_FOREST_TEMPLE,
    /* 23 */ SDC_WATER_TEMPLE,
    /* 24 */ SDC_SHADOW_TEMPLE_AND_WELL,
    /* 25 */ SDC_SPIRIT_TEMPLE,
    /* 26 */ SDC_INSIDE_GANONS_CASTLE,
    /* 27 */ SDC_GERUDO_TRAINING_GROUND,
    /* 28 */ SDC_DEKU_TREE_BOSS,
    /* 29 */ SDC_WATER_TEMPLE_BOSS,
    /* 30 */ SDC_TEMPLE_OF_TIME,
    /* 31 */ SDC_GROTTOS,
    /* 32 */ SDC_CHAMBER_OF_THE_SAGES,
    /* 33 */ SDC_GREAT_FAIRYS_FOUNTAIN,
    /* 34 */ SDC_SHOOTING_GALLERY,
    /* 35 */ SDC_CASTLE_COURTYARD_GUARDS,
    /* 36 */ SDC_OUTSIDE_GANONS_CASTLE,
    /* 37 */ SDC_ICE_CAVERN,
    /* 38 */ SDC_GANONS_TOWER_COLLAPSE_EXTERIOR,
    /* 39 */ SDC_FAIRYS_FOUNTAIN,
    /* 40 */ SDC_THIEVES_HIDEOUT,
    /* 41 */ SDC_BOMBCHU_BOWLING_ALLEY,
    /* 42 */ SDC_ROYAL_FAMILYS_TOMB,
    /* 43 */ SDC_LAKESIDE_LABORATORY,
    /* 44 */ SDC_LON_LON_BUILDINGS,
    /* 45 */ SDC_MARKET_GUARD_HOUSE,
    /* 46 */ SDC_POTION_SHOP_GRANNY,
    /* 47 */ SDC_CALM_WATER,
    /* 48 */ SDC_GRAVE_EXIT_LIGHT_SHINING,
    /* 49 */ SDC_BESITU,
    /* 50 */ SDC_FISHING_POND,
    /* 51 */ SDC_GANONS_TOWER_COLLAPSE_INTERIOR,
    /* 52 */ SDC_INSIDE_GANONS_CASTLE_COLLAPSE,
    /* 53 */ SDC_MAX
} SceneDrawConfig;

#endif
