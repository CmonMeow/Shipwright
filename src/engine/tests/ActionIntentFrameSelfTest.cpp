#include "engine/input/ActionIntentFrame.h"

#include <cstddef>

namespace {

enum TestAction : size_t {
    ActionWeapon,
    ActionEvade,
    ActionBow,
    ActionCount,
};

bool TestOneShotConsumption() {
    Engine::ActionIntentFrame<ActionCount> intents;
    if (intents.Pending(ActionWeapon) || intents.Requested(ActionWeapon)) return false;
    intents.Request(ActionWeapon);
    return intents.Pending(ActionWeapon) && intents.Requested(ActionWeapon) &&
           intents.Consume(ActionWeapon) && !intents.Pending(ActionWeapon) &&
           intents.Requested(ActionWeapon) && !intents.Consume(ActionWeapon);
}

bool TestSampleBoundaryDiscardsRejectedAction() {
    Engine::ActionIntentFrame<ActionCount> intents;
    intents.Request(ActionBow);
    if (!intents.Pending(ActionBow)) return false;

    // The native state rejected this edge. Starting the next controller
    // sample must discard it rather than firing after recovery finishes.
    intents.BeginSample();
    return !intents.Pending(ActionBow) && !intents.Requested(ActionBow) &&
           !intents.Consume(ActionBow);
}

bool TestIndependentActions() {
    Engine::ActionIntentFrame<ActionCount> intents;
    intents.Request(ActionWeapon);
    intents.Request(ActionEvade);
    if (!intents.Consume(ActionWeapon) || !intents.Pending(ActionEvade)) return false;
    intents.Cancel(ActionEvade);
    return intents.Requested(ActionWeapon) && !intents.Pending(ActionWeapon) &&
           !intents.Pending(ActionEvade) && !intents.Requested(ActionEvade);
}

bool TestExplicitStateTransitionDiscard() {
    Engine::ActionIntentFrame<ActionCount> intents;
    intents.Request(ActionWeapon);
    intents.Request(ActionEvade);
    intents.Clear();
    return !intents.Consume(ActionWeapon) && !intents.Requested(ActionWeapon) &&
           !intents.Consume(ActionEvade) && !intents.Requested(ActionEvade);
}

bool TestInvalidAction() {
    Engine::ActionIntentFrame<ActionCount> intents;
    intents.Request(ActionCount);
    return !intents.Pending(ActionCount) && !intents.Requested(ActionCount) &&
           !intents.Consume(ActionCount);
}

} // namespace

int main() {
    if (!TestOneShotConsumption()) return 1;
    if (!TestSampleBoundaryDiscardsRejectedAction()) return 2;
    if (!TestIndependentActions()) return 3;
    if (!TestExplicitStateTransitionDiscard()) return 4;
    if (!TestInvalidAction()) return 5;
    return 0;
}
