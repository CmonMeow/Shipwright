#include "Controls.h"

#include "platform/win32/Input.h"

Controls controls;

void Controls::Update(const Input& source) {
    move.x = (source.key['D'] ? 1 : 0) - (source.key['A'] ? 1 : 0);
    move.y = (source.key['W'] ? 1 : 0) - (source.key['S'] ? 1 : 0);
    if (move.x != 0 && move.y != 0) {
        move.y = 0;
    }

    for (uint8_t slot = 1; slot <= 4; ++slot) {
        if (source.framePress['0' + slot]) {
            weapon = slot;
        }
    }
}
