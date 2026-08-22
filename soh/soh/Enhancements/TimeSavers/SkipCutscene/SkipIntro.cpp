#include "soh/Enhancements/game-interactor/GameInteractor.h"
#include "soh/ShipInit.hpp"

extern "C" {
#include "z64save.h"
#include "functions.h"
extern PlayState* gPlayState;
extern SaveContext gSaveContext;
}

void RegisterSkipIntro() {
    bool shouldRegister = CVarGetInteger(CVAR_ENHANCEMENT("TimeSavers.SkipCutscene.Intro"), 0);
    COND_VB_SHOULD(VB_PLAY_TRANSITION_CS, shouldRegister, {
        if (CVarGetInteger(CVAR_ENHANCEMENT("TimeSavers.SkipCutscene.Intro"), 0) &&
            gSaveContext.cutsceneIndex == 0xFFF1) {
            int32_t spawnEntrance = ENTR_LINKS_HOUSE_CHILD_SPAWN;
            // Skip the intro cutscene for whatever the spawnEntrance is calculated to be.
            if (gSaveContext.entranceIndex == spawnEntrance) {
                gSaveContext.cutsceneIndex = 0;
                *should = false;
            }
        }
    });
}

static RegisterShipInitFunc initFunc(RegisterSkipIntro, { CVAR_ENHANCEMENT("TimeSavers.SkipCutscene.Intro") });
