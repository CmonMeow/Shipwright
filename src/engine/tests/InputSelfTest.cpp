#include "platform/win32/Input.h"
#include "gameplay/Controls.h"

namespace {

bool TestPressLifetime() {
    Input test;
    test.KeyDown('W');
    if (!test.key['W'] || !test.keyPress['W'] || !test.framePress['W']) return false;
    if (!test.ConsumePress('W') || test.keyPress['W'] || !test.framePress['W']) return false;

    test.EndFrame();
    if (!test.key['W'] || test.keyPress['W'] || test.framePress['W']) return false;

    test.KeyDown('W');
    if (test.keyPress['W'] || test.framePress['W']) return false;
    test.KeyUp('W');
    return !test.key['W'];
}

bool TestClear() {
    Input test;
    test.KeyDown('A');
    test.AddMouseDelta(3, -4);
    test.AddMouseWheel(120);
    test.TextInput('a');
    test.Clear();
    return !test.key['A'] && !test.keyPress['A'] && !test.framePress['A'] &&
           test.mouseDelta == vec2i{} && test.mouseWheel == 0 && test.textCount == 0;
}

bool TestMouseUsesKeyTable() {
    constexpr uint8_t LeftMouse = 1;
    Input test;
    test.KeyDown(LeftMouse);
    if (!test.key[LeftMouse] || !test.ConsumePress(LeftMouse)) return false;
    test.KeyUp(LeftMouse);
    return !test.key[LeftMouse];
}

bool TestMouseDelta() {
    Input test;
    test.AddMouseDelta(4, -2);
    test.AddMouseDelta(-1, 5);
    if (test.ConsumeMouseDelta() != vec2i{ 3, 3 }) return false;
    return test.ConsumeMouseDelta() == vec2i{};
}

bool TestTextOrder() {
    Input test;
    uint8_t character = 0;
    test.TextInput('a');
    test.TextInput('b');
    return test.PopTextInput(character) && character == 'a' &&
           test.PopTextInput(character) && character == 'b' &&
           !test.PopTextInput(character);
}

bool TestWeaponSelection() {
    Input test;
    Controls selected;
    test.KeyDown('3');
    selected.Update(test);
    if (selected.weapon != 3) return false;
    test.EndFrame();
    selected.Update(test);
    if (selected.weapon != 3) return false;
    test.KeyUp('3');
    test.KeyDown('1');
    selected.Update(test);
    return selected.weapon == 1;
}

bool TestMovement() {
    Input test;
    Controls selected;
    test.KeyDown('W');
    selected.Update(test);
    if (selected.move != vec2i{ 0, 1 }) return false;

    test.KeyDown('D');
    selected.Update(test);
    if (selected.move != vec2i{ 1, 0 }) return false;

    test.KeyUp('W');
    test.KeyUp('D');
    selected.Update(test);
    return selected.move == vec2i{};
}

} // namespace

int main() {
    if (!TestPressLifetime()) return 1;
    if (!TestClear()) return 2;
    if (!TestMouseUsesKeyTable()) return 3;
    if (!TestMouseDelta()) return 4;
    if (!TestTextOrder()) return 5;
    if (!TestWeaponSelection()) return 6;
    if (!TestMovement()) return 7;
    return 0;
}
