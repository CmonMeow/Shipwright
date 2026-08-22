#include "soh/Enhancements/game-interactor/GameInteractor.h"
#include "soh/ShipInit.hpp"
#include <cassert>

extern "C" {
#include "z64save.h"
extern PlayState* gPlayState;
extern SaveContext gSaveContext;

}

#define CVAR_NAME CVAR_ENHANCEMENT("TimeSavers.SkipCutscene.OnePoint")
#define CVAR_VALUE CVarGetInteger(CVAR_NAME, 0)

static s16 GetEntranceIndex(s32 owlType) {
    s16 entranceIndex;

    switch (owlType) {
        case 7:
            entranceIndex = ENTR_HYRULE_FIELD_OWL_DROP;
            break;
        case 8:
        case 9:
            entranceIndex = ENTR_KAKARIKO_VILLAGE_OWL_DROP;
            break;
        default:
            assert(0);
            return ENTR_MAX;
    }

    return entranceIndex;
}

static void SkipOwlTravel(s32 owlType) {
    gPlayState->nextEntranceIndex = GetEntranceIndex(owlType);
    gPlayState->transitionTrigger = TRANS_TRIGGER_START;
    gPlayState->transitionType = TRANS_TYPE_FADE_BLACK;
}

static void RegisterSkipOwlTravel() {
    COND_VB_SHOULD(VB_PLAY_OWL_TRAVEL_CS, CVAR_VALUE, {
        s32 owlType = va_arg(args, s32);
        SkipOwlTravel(owlType);
        *should = false;
    });
}

static RegisterShipInitFunc initFunc(RegisterSkipOwlTravel, { CVAR_NAME });
