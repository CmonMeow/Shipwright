#ifndef VARIABLES_H
#define VARIABLES_H

#include "z64.h"
#include "segment_symbols.h"

#ifdef __cplusplus
extern "C"
{
#endif

	extern uint32_t osTvType;
	extern uint32_t osRomBase;
	extern uint32_t osResetType;
	extern uint32_t osMemSize;
	extern uint8_t osAppNmiBuffer[0x40];

	extern uint8_t D_80009320[];
	extern uint8_t D_800093F0[];
	extern uint32_t D_80009460;
	extern uint32_t gDmaMgrDmaBuffSize;
	extern OSPiHandle* gCartHandle;
	extern uint32_t __osPiAccessQueueEnabled;
	extern OSViMode osViModePalLan1;
	extern int32_t osViClock;
	extern uint32_t __osShutdown;
	extern OSHWIntr __OSGlobalIntMask;
	extern OSThread* __osThreadTail[];
	extern OSThread* __osRunQueue;
	extern OSThread* __osActiveQueue;
	extern OSThread* __osRunningThread;
	extern OSThread* __osFaultedThread;
	extern OSPiHandle* __osPiTable;
	extern OSPiHandle* __osCurrentHandle[];
	extern OSTimer* __osTimerList;
	extern OSViMode osViModeNtscLan1;
	extern OSViMode osViModeMpalLan1;
	extern OSViContext* __osViCurr;
	extern OSViContext* __osViNext;
	extern OSViMode osViModeFpalLan1;
	extern uint32_t __additional_scanline;
	extern const char gBuildVersion[];
	extern uint16_t gBuildVersionMajor;
	extern uint16_t gBuildVersionMinor;
	extern uint16_t gBuildVersionPatch;
	extern const char gGitBranch[];
	extern const char gGitCommitHash[];
	extern uint8_t gGitCommitTag[];
	extern uint8_t gBuildTeam[];
	extern uint8_t gBuildDate[];
	extern uint8_t gBuildMakeOption[];
	extern OSMesgQueue __osPiAccessQueue;
	extern OSPiHandle __Dom1SpeedParam;
	extern OSPiHandle __Dom2SpeedParam;
	extern OSTime __osCurrentTime;
	extern uint32_t __osBaseCounter;
	extern uint32_t __osViIntrCount;
	extern uint32_t __osTimerCounter;
	extern DmaEntry gDmaDataTable[0x60C];
	extern uint64_t D_801120C0[];
	extern uint8_t D_80113070[];
	extern EffectSsOverlay gEffectSsOverlayTable[EFFECT_SS_TYPE_MAX];
	extern Gfx D_80116280[];
	extern GameStateOverlay gGameStateOverlayTable[2];
	extern uint8_t gWeatherMode;
	extern uint8_t D_8011FB34;
	extern uint8_t D_8011FB38;
	extern uint8_t gSkyboxBlendingEnabled;
	extern uint16_t gTimeIncrement;
	extern struct_8011FC1C D_8011FC1C[][9];
	extern SkyboxFile gSkyboxFiles[];
	extern int32_t gZeldaArenaLogSeverity;
	extern int16_t gSpoilingItems[3];
	extern int16_t gSpoilingItemReverts[3];
	extern FlexSkeletonHeader* gPlayerSkelHeaders;
	extern uint8_t gPlayerModelTypes[PLAYER_MODELGROUP_MAX][PLAYER_MODELGROUPENTRY_MAX];
	extern Gfx* gPlayerLeftHandBgsDLs[];
	extern Gfx* gPlayerLeftHandOpenDLs[];
	extern Gfx* gPlayerLeftHandClosedDLs[];
	extern Gfx* gPlayerLeftHandBoomerangDLs[];
	extern Gfx gCullBackDList[];
	extern Gfx gCullFrontDList[];
	extern Gfx gEmptyDL[];
	extern uint32_t gBitFlags[32];
	extern uint16_t gEquipMasks[4];
	extern uint16_t gEquipNegMasks[4];
	extern uint32_t gUpgradeMasks[8];
	extern uint32_t gUpgradeNegMasks[8];
	extern uint8_t gEquipShifts[4];
	extern uint8_t gUpgradeShifts[8];
	extern uint16_t gUpgradeCapacities[8][4];
	extern uint32_t gGsFlagsMasks[4];
	extern uint32_t gGsFlagsShifts[4];
	extern uint8_t gItemSlots[56];
	extern uint32_t gObjectTableSize;
	extern RomFile gObjectTable[OBJECT_ID_MAX];
	extern EntranceInfo gEntranceTable[ENTR_MAX];
	extern SceneTableEntry gSceneTable[SCENE_ID_MAX];
	extern uint16_t gSramSlotOffsets[];
	extern uint8_t gBossMarkState;
	extern int32_t gScreenWidth;
	extern int32_t gScreenHeight;
	extern Mtx gMtxClear;
	extern MtxF gMtxFClear;
	extern uint32_t gIsCtrlr2Valid;
	extern volatile uint32_t gIrqMgrResetStatus;
	extern volatile OSTime gIrqMgrRetraceTime;
	extern int16_t* gWaveSamples[9];
	extern float gBendPitchOneOctaveFrequencies[256];
	extern float gBendPitchTwoSemitonesFrequencies[256];
	extern float gNoteFrequencies[];
	extern uint8_t gDefaultShortNoteVelocityTable[16];
	extern uint8_t gDefaultShortNoteGateTimeTable[16];
	extern AdsrEnvelope gDefaultEnvelope[4];
	extern NoteSubEu gZeroNoteSub;
	extern NoteSubEu gDefaultNoteSub;
	extern uint16_t gHeadsetPanQuantization[64];
	extern int16_t D_8012FBA8[];
	extern float gHeadsetPanVolume[128];
	extern float gStereoPanVolume[128];
	extern float gDefaultPanVolume[128];
	extern int16_t sLowPassFilterData[16 * 8];
	extern int16_t sHighPassFilterData[15 * 8];
	extern int32_t gAudioContextInitalized;
	extern uint8_t gIsLargeSoundBank[7];
	extern uint8_t gChannelsPerBank[4][7];
	extern uint8_t gUsedChannelsPerBank[4][7];
	extern uint8_t gMorphaTransposeTable[16];
	extern uint8_t* gFrogsSongPtr;
	extern OcarinaNote* gScarecrowCustomSongPtr;
	extern uint8_t* gScarecrowSpawnSongPtr;
	extern OcarinaSongInfo gOcarinaSongNotes[];
	extern SoundParams* gSoundParams[7];
	extern char D_80133390[];
	extern char D_80133398[];
	extern SoundBankEntry* gSoundBanks[7];
	extern uint8_t gSfxChannelLayout;
	extern Vec3f gSfxDefaultPos;
	extern float gSfxDefaultFreqAndVolScale;
	extern int8_t gSfxDefaultReverb;
	extern uint8_t D_801333F0;
	extern uint8_t gAudioSfxSwapOff;
	extern uint8_t D_80133408;
	extern uint8_t D_8013340C;
	extern uint8_t gAudioSpecId;
	extern uint8_t D_80133418;
	extern AudioSpec gAudioSpecs[18];
	extern int32_t gOverlayLogSeverity;
	extern int32_t gSystemArenaLogSeverity;
	extern uint8_t __osPfsInodeCacheBank;
	extern int32_t __osPfsLastChannel;
	extern float triforcePieceScale;
	extern float mysteryItemScale;

	extern const int16_t D_8014A6C0[];
#define gTatumsPerBeat (D_8014A6C0[1])
	extern const AudioContextInitSizes D_8014A6C4;
	extern int16_t gOcarinaSongItemMap[];
	extern uint8_t D_80155F50[];
	extern uint8_t D_80157580[];
	extern uint8_t D_801579A0[];

	extern SaveContext gSaveContext;
	extern GameInfo* gGameInfo;
	extern uint8_t gCustomLensFlareOn;
	extern Vec3f gCustomLensFlarePos;
	extern int16_t gLensFlareScale;
	extern float gLensFlareColorIntensity;
	extern int16_t gLensFlareScreenFillAlpha;
	extern LightningStrike gLightningStrike;
	extern float gBossMarkScale;
	extern PreNmiBuff* gAppNmiBufferPtr;
	extern SchedContext gSchedContext;
	extern PadMgr gPadMgr;
	extern uintptr_t gSegments[NUM_SEGMENTS];

	extern ActiveSound gActiveSounds[7][MAX_CHANNELS_PER_BANK]; // total size = 0xA8
	extern uint8_t gSoundBankMuted[];
	extern uint8_t D_801333F0;
	extern uint8_t gAudioSfxSwapOff;
	extern uint16_t gAudioSfxSwapSource[10];
	extern uint16_t gAudioSfxSwapTarget[10];
	extern uint8_t gAudioSfxSwapMode[10];
	extern ActiveSequence gActiveSeqs[4];
	extern AudioContext gAudioContext;
	extern void(*D_801755D0)(void);

	extern uint32_t __osMalloc_FreeBlockTest_Enable;
	extern Arena gSystemArena;
	extern OSPifRam __osPifInternalBuff;
	extern uint8_t __osContLastPoll;
	extern uint8_t __osMaxControllers;
	extern __OSInode __osPfsInodeCache;
	extern OSPifRam gPifMempakBuf;
	extern uint16_t gZBuffer[SCREEN_HEIGHT][SCREEN_WIDTH]; // 0x25800 bytes
	extern uint64_t gGfxSPTaskOutputBuffer[0x3000]; // 0x18000 bytes
	extern uint8_t gGfxSPTaskYieldBuffer[OS_YIELD_DATA_SIZE]; // 0xC00 bytes
	extern uint8_t gGfxSPTaskStack[0x400]; // 0x400 bytes
	extern GfxPool gGfxPools[2]; // 0x24820 bytes
	extern uint8_t* gAudioHeap;
	extern uint8_t* gSystemHeap;
	extern GameState* gGameState;

#ifdef __cplusplus
};
#endif

#endif
