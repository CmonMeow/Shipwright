#pragma once

#include <cstdint>

#include <maff.h>

struct Input;

struct Controls {
    vec2i move;
    uint8_t weapon = 1;

    void Update(const Input& source);
};

extern Controls controls;
