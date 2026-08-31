#include "runtime/runtime.h"
#include "engine/input/Win32Input.h"
#include "engine/input/ActionIntentFrame.h"
#include "engine/input/PCInput.h"

#include <cstring>
#include <ratio>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

// Establish a chrono duration for the N64 46.875MHz clock rate
typedef std::ratio<3000, 64> n64ClockRatio;
typedef std::ratio_divide<std::micro, n64ClockRatio> n64CycleRate;
typedef std::chrono::duration<long long, n64CycleRate> n64CycleRateDuration;

extern "C" {
uint8_t __osMaxControllers = MAXCONTROLLERS;
uint64_t __osCurrentTime = 0;

static uint16_t sSelectedWeaponButton = BTN_CLEFT;
static int32_t sWeaponSelectionRequested = 0;

enum PcActionIntent : size_t {
    PC_ACTION_WEAPON_SELECTION,
    PC_ACTION_TOGGLE_WEAPON,
    PC_ACTION_EVADE,
    PC_ACTION_BOW_USE,
    PC_ACTION_COUNT,
};

static Engine::ActionIntentFrame<PC_ACTION_COUNT> sActionIntents;

int32_t osContInit(OSMesgQueue* mq, uint8_t* controllerBits, OSContStatus* status) {
    std::memset(status, 0, sizeof(OSContStatus) * __osMaxControllers);
    *controllerBits = 1;
    status[0].type = CONT_TYPE_NORMAL;
    status[0].status = CONT_CARD_ON;

    return 0;
}

int32_t osContStartReadData(OSMesgQueue* mesg) {
    return 0;
}

int32_t PCInput_ConsumeMouseAimDelta(int32_t* deltaX, int32_t* deltaY) {
#ifdef _WIN32
    Engine::Win32Input& input = Engine::GetWin32Input();

    *deltaX = 0;
    *deltaY = 0;
    if (input.IsGameInputBlocked() || input.IsTextInputCaptured()) {
        input.ConsumeMouseDelta();
        return false;
    }

    const Engine::MousePosition delta = input.ConsumeMouseDelta();
    *deltaX = delta.x;
    *deltaY = delta.y;
    return true;
#else
    *deltaX = 0;
    *deltaY = 0;
    return false;
#endif
}

int32_t PCInput_ConsumeToggleWeapon(void) {
    return sActionIntents.Consume(PC_ACTION_TOGGLE_WEAPON);
}

int32_t PCInput_ConsumeEvade(void) {
    return sActionIntents.Consume(PC_ACTION_EVADE);
}

int32_t PCInput_EvadeRequestedThisSample(void) {
    return sActionIntents.Requested(PC_ACTION_EVADE);
}

void PCInput_DiscardActionIntents(void) {
    sWeaponSelectionRequested = 0;
    sActionIntents.Clear();
}

int32_t PCInput_IsFishingReelHeld(void) {
#ifdef _WIN32
    Engine::Win32Input& input = Engine::GetWin32Input();
    return input.Pressed(VK_RBUTTON) && !input.IsGameInputBlocked() && !input.IsTextInputCaptured();
#else
    return false;
#endif
}

int32_t PCInput_IsShieldHeld(void) {
#ifdef _WIN32
    Engine::Win32Input& input = Engine::GetWin32Input();
    const bool swordSelected = sSelectedWeaponButton == BTN_CLEFT || sSelectedWeaponButton == BTN_CDOWN;
    return swordSelected && input.Pressed(VK_RBUTTON) && !input.IsGameInputBlocked() &&
           !input.IsTextInputCaptured();
#else
    return false;
#endif
}

int32_t PCInput_GetSelectedWeaponSlot(void) {
    switch (sSelectedWeaponButton) {
        case BTN_CLEFT:
            return 1;
        case BTN_CDOWN:
            return 2;
        case BTN_CRIGHT:
            return 3;
        default:
            return 4;
    }
}

int32_t PCInput_ConsumeWeaponSelection(void) {
    if (!sActionIntents.Consume(PC_ACTION_WEAPON_SELECTION)) {
        sWeaponSelectionRequested = 0;
        return 0;
    }
    const int32_t requested = sWeaponSelectionRequested;
    sWeaponSelectionRequested = 0;
    return requested;
}

int32_t PCInput_IsBowAimHeld(void) {
#ifdef _WIN32
    Engine::Win32Input& input = Engine::GetWin32Input();
    return (sSelectedWeaponButton == BTN_CRIGHT) && input.Pressed(VK_RBUTTON) &&
           !input.IsGameInputBlocked() && !input.IsTextInputCaptured();
#else
    return false;
#endif
}

void PCInput_ConsumeBowUseIntent(void) {
    sActionIntents.Cancel(PC_ACTION_BOW_USE);
}

int32_t PCInput_HasBowUseIntent(void) {
    return sActionIntents.Pending(PC_ACTION_BOW_USE);
}

void osContGetReadData(OSContPad* pad) {
    memset(pad, 0, sizeof(OSContPad) * __osMaxControllers);
    sActionIntents.BeginSample();

#ifdef _WIN32
    Engine::Win32Input& input = Engine::GetWin32Input();

    if (input.IsGameInputBlocked() || input.IsTextInputCaptured()) {
        PCInput_DiscardActionIntents();
        return;
    }

    // A number-key edge changes the selected weapon and asks the native item
    // system to show that selection immediately.
    if (input.ConsumePress('1')) {
        sSelectedWeaponButton = BTN_CLEFT;
        sWeaponSelectionRequested = 1;
        sActionIntents.Request(PC_ACTION_WEAPON_SELECTION);
        sActionIntents.Cancel(PC_ACTION_BOW_USE);
    }
    if (input.ConsumePress('2')) {
        sSelectedWeaponButton = BTN_CDOWN;
        sWeaponSelectionRequested = 2;
        sActionIntents.Request(PC_ACTION_WEAPON_SELECTION);
        sActionIntents.Cancel(PC_ACTION_BOW_USE);
    }
    if (input.ConsumePress('3')) {
        sSelectedWeaponButton = BTN_CRIGHT;
        sWeaponSelectionRequested = 3;
        sActionIntents.Request(PC_ACTION_WEAPON_SELECTION);
    }
    if (input.ConsumePress('4')) {
        sSelectedWeaponButton = BTN_CUP;
        sWeaponSelectionRequested = 4;
        sActionIntents.Request(PC_ACTION_WEAPON_SELECTION);
        sActionIntents.Cancel(PC_ACTION_BOW_USE);
    }
    if (input.ConsumePress('X')) {
        sActionIntents.Request(PC_ACTION_TOGGLE_WEAPON);
    }
    if (input.ConsumePress(VK_SPACE)) {
        // Evade is an edge for this simulation opportunity only. If the
        // current native action cannot accept it, the next controller sample
        // expires it instead of producing a delayed backflip.
        sActionIntents.Request(PC_ACTION_EVADE);
    }

    const bool swordSelected = sSelectedWeaponButton == BTN_CLEFT || sSelectedWeaponButton == BTN_CDOWN;
    const bool bowSelected = sSelectedWeaponButton == BTN_CRIGHT;
    const bool fishingSelected = sSelectedWeaponButton == BTN_CUP;

    // A click is valid for this controller sample only. If bow recovery still
    // owns the action state, the click is rejected instead of replayed later.
    if (bowSelected && input.ConsumePress(VK_LBUTTON)) {
        sActionIntents.Request(PC_ACTION_BOW_USE);
    }

    if (fishingSelected && input.Pressed(VK_LBUTTON)) {
        // Left-click begins a cast through the pole's native item button.
        pad[0].button |= BTN_CUP;
    } else if (input.Pressed(VK_LBUTTON)) {
        // Bow input remains held until LMB is released, preserving the native
        // draw/hold/release sequence while RMB controls only the camera. A
        // rapid click is consumed directly by the player once recovery ends;
        // it must not become a permanently held virtual C-right button.
        pad[0].button |= sSelectedWeaponButton;
    }
    if (fishingSelected && input.Pressed(VK_RBUTTON)) {
        // Native fishing uses A for reeling, hooking, and fighting fish.
        pad[0].button |= BTN_A;
    } else if (swordSelected && input.Pressed(VK_RBUTTON)) {
        pad[0].button |= BTN_R;
    }

    const int32_t stickX = (input.Pressed('D') ? 85 : 0) - (input.Pressed('A') ? 85 : 0);
    int32_t stickY = (input.Pressed('W') ? 85 : 0) - (input.Pressed('S') ? 85 : 0);

    // A/D owns lateral locomotion whenever a horizontal key is held. Mixing
    // equal digital axes sits on the engine's forward/side boundary and picks
    // the forward run with its cornering torso/head lean.
    if ((stickX != 0) && (stickY != 0)) {
        stickY = 0;
    }
    pad[0].stick_x = static_cast<int8_t>(stickX);
    pad[0].stick_y = static_cast<int8_t>(stickY);
#endif
}

void osSetTime(OSTime time) {
    __osCurrentTime =
        std::chrono::duration_cast<n64CycleRateDuration>(std::chrono::steady_clock::now().time_since_epoch()).count() +
        time;
}

// Returns the OS time matching the N64 46.875MHz cycle rate
uint64_t osGetTime() {
    return std::chrono::duration_cast<n64CycleRateDuration>(std::chrono::steady_clock::now().time_since_epoch())
               .count() -
           __osCurrentTime;
}

// Returns the CPU clock count matching the N64 46.875Mhz cycle rate
uint32_t osGetCount() {
    return std::chrono::duration_cast<n64CycleRateDuration>(std::chrono::steady_clock::now().time_since_epoch())
        .count();
}

OSPiHandle* osCartRomInit() {
    return NULL;
}

int osSetTimer(OSTimer* t, OSTime countdown, OSTime interval, OSMesgQueue* mq, OSMesg msg) {
    return 0;
}

int32_t osEPiStartDma(OSPiHandle* pihandle, OSIoMesg* mb, int32_t direction) {
    return 0;
}

uint32_t osAiGetLength() {
    // TODO: Implement
    return 0;
}

int32_t osAiSetNextBuffer(void* buff, size_t len) {
    // TODO: Implement
    return 0;
}

int32_t __osMotorAccess(OSPfs* pfs, uint32_t vibrate) {
    return 0;
}

int32_t osMotorInit(OSMesgQueue* ctrlrqueue, OSPfs* pfs, int32_t channel) {
    pfs->channel = channel;
    return 0;
}
}
