#ifndef _Z64ENVIRONMENT_H_
#define _Z64ENVIRONMENT_H_

#include "z64math.h"
#include "z64light.h"
#include "z64dma.h"

#define FILL_SCREEN_OPA (1 << 0)
#define FILL_SCREEN_XLU (1 << 1)

typedef enum {
    /* 0 */ LIGHTNING_MODE_OFF, // no lightning
    /* 1 */ LIGHTNING_MODE_ON, // request ligtning strikes at random intervals
    /* 2 */ LIGHTNING_MODE_LAST // request one lightning strike before turning off
} LightningMode;

typedef enum {
    /* 0 */ LIGHTNING_STRIKE_WAIT, // wait between lightning strikes. request bolts when timer hits 0
    /* 1 */ LIGHTNING_STRIKE_START, // fade in the flash. note: bolts are requested in the previous state
    /* 2 */ LIGHTNING_STRIKE_END // fade out the flash and go back to wait
} LightningStrikeState;

typedef enum {
    /*  0 */ SKYBOX_DMA_INACTIVE,
    /*  1 */ SKYBOX_DMA_FILE1_START,
    /*  2 */ SKYBOX_DMA_FILE1_DONE,
    /*  3 */ SKYBOX_DMA_PAL1_START,
    /* 11 */ SKYBOX_DMA_FILE2_START = 11,
    /* 12 */ SKYBOX_DMA_FILE2_DONE,
    /* 13 */ SKYBOX_DMA_PAL2_START
} SkyboxDmaState;

typedef enum {
    /* 0 */ SANDSTORM_OFF,
    /* 1 */ SANDSTORM_FILL,
    /* 2 */ SANDSTORM_UNFILL,
    /* 3 */ SANDSTORM_ACTIVE,
    /* 4 */ SANDSTORM_DISSIPATE
} SandstormState;

typedef struct {
    /* 0x00 */ uint8_t state;
    /* 0x01 */ uint8_t flashRed;
    /* 0x02 */ uint8_t flashGreen;
    /* 0x03 */ uint8_t flashBlue;
    /* 0x04 */ uint8_t flashAlphaTarget;
    /* 0x08 */ float delayTimer;
} LightningStrike; // size = 0xC

// describes what skybox files and blending modes to use depending on time of day
typedef struct {
    /* 0x00 */ uint16_t startTime;
    /* 0x02 */ uint16_t endTime;
    /* 0x04 */ uint8_t blend; // if true, blend between.. skyboxes? palettes?
    /* 0x05 */ uint8_t skybox1Index; // whats the difference between _pal and non _pal files?
    /* 0x06 */ uint8_t skybox2Index;
} struct_8011FC1C; // size = 0x8

typedef struct {
    /* 0x00 */ uint8_t ambientColor[3];
    /* 0x03 */ int8_t light1Dir[3];
    /* 0x06 */ uint8_t light1Color[3];
    /* 0x09 */ int8_t light2Dir[3];
    /* 0x0C */ uint8_t light2Color[3];
    /* 0x0F */ uint8_t fogColor[3];
    /* 0x12 */ int16_t fogNear;
    /* 0x14 */ int16_t fogFar;
} EnvLightSettings; // size = 0x16

// 1.0: 801D8EC4
// dbg: 80222A44
typedef struct {
    /* 0x00 */ char unk_00[0x02];
    /* 0x02 */ uint16_t timeIncrement; // how many units of time that pass every update
    /* 0x04 */ Vec3f sunPos; // moon position can be found by negating the sun position
    /* 0x10 */ uint8_t skybox1Index;
    /* 0x11 */ uint8_t skybox2Index;
    /* 0x12 */ char unk_12[0x01];
    /* 0x13 */ uint8_t skyboxBlend;
    /* 0x14 */ char unk_14[0x01];
    /* 0x15 */ uint8_t skyboxDisabled;
    /* 0x16 */ uint8_t sunMoonDisabled;
    /* 0x17 */ uint8_t unk_17; // currentWeatherMode for skybox? (prev called gloomySky)
    /* 0x18 */ uint8_t unk_18; // nextWeatherMode for skybox?
    /* 0x19 */ uint8_t unk_19;
    /* 0x1A */ uint16_t unk_1A;
    /* 0x1C */ char unk_1C[0x02];
    /* 0x1E */ uint8_t indoors; // when set, day time has no effect on lighting
    /* 0x1F */ uint8_t unk_1F; // outdoor light index
    /* 0x20 */ uint8_t unk_20; // prev outdoor light index?
    /* 0x21 */ uint8_t unk_21;
    /* 0x22 */ uint16_t unk_22;
    /* 0x24 */ uint16_t unk_24;
    /* 0x26 */ char unk_26[0x02];
    /* 0x28 */ LightInfo dirLight1; // used for sunlight outdoors
    /* 0x36 */ LightInfo dirLight2; // used for moonlight outdoors
    /* 0x44 */ int8_t skyboxDmaState;
    /* 0x48 */ DmaRequest dmaRequest;
    /* 0x68 */ OSMesgQueue loadQueue;
    /* 0x80 */ OSMesg loadMsg;
    /* 0x84 */ float unk_84;
    /* 0x88 */ float unk_88;
    /* 0x8C */ int16_t adjAmbientColor[3];
    /* 0x92 */ int16_t adjLight1Color[3];
    /* 0x98 */ int16_t adjFogColor[3];
    /* 0x9E */ int16_t adjFogNear;
    /* 0xA0 */ int16_t adjFogFar;
    /* 0xA2 */ char unk_A2[0x06];
    /* 0xA8 */ Vec3s windDirection;
    /* 0xB0 */ float windSpeed;
    /* 0xB4 */ uint8_t numLightSettings;
    /* 0xB8 */ EnvLightSettings* lightSettingsList; // list of light settings from the scene file
    /* 0xBC */ uint8_t blendIndoorLights; // when true, blend between indoor light settings when switching
    /* 0xBD */ uint8_t unk_BD; // indoor light index
    /* 0xBE */ uint8_t unk_BE; // prev indoor light index?
    /* 0xBF */ uint8_t unk_BF;
    /* 0xC0 */ EnvLightSettings lightSettings;
    /* 0xD6 */ uint16_t unk_D6;
    /* 0xD8 */ float unk_D8; // indoor light blend weight?
    /* 0xDC */ uint8_t unk_DC;
    /* 0xDD */ uint8_t gloomySkyMode;
    /* 0xDE */ uint8_t unk_DE; // gloomy sky state
    /* 0xDF */ uint8_t lightningMode;
    /* 0xE0 */ uint8_t unk_E0; // env sounds state
    /* 0xE1 */ uint8_t fillScreen;
    /* 0xE2 */ uint8_t screenFillColor[4];
    /* 0xE6 */ uint8_t sandstormState;
    /* 0xE7 */ uint8_t sandstormPrimA;
    /* 0xE8 */ uint8_t sandstormEnvA;
    /* 0xE9 */ uint8_t customSkyboxFilter;
    /* 0xEA */ uint8_t skyboxFilterColor[4];
    /* 0xEE */ uint8_t unk_EE[4];
    /* 0xF2 */ uint8_t unk_F2[4];
    /* 0xF6 */ char unk_F6[0x06];
} EnvironmentContext; // size = 0xFC

#endif
