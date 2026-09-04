#pragma once

#include <cstdint>

struct vec2i {
    int32_t x = 0;
    int32_t y = 0;

    vec2i(int32_t x = 0, int32_t y = 0) : x(x), y(y) {
    }

    vec2i& operator+=(const vec2i& other) {
        x += other.x;
        y += other.y;
        return *this;
    }

    vec2i operator+(const vec2i& other) const {
        return { x + other.x, y + other.y };
    }

    vec2i& operator-=(const vec2i& other) {
        x -= other.x;
        y -= other.y;
        return *this;
    }

    vec2i operator-(const vec2i& other) const {
        return { x - other.x, y - other.y };
    }

    bool operator==(const vec2i& other) const {
        return x == other.x && y == other.y;
    }

    bool operator!=(const vec2i& other) const {
        return !(*this == other);
    }
};
