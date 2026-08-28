#ifndef Z64MATH_H
#define Z64MATH_H

#include <runtime/libultra.h>
#include <float.h>
#include <math.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846f
#endif

#define VEC_SET(V,X,Y,Z) (V).x=(X);(V).y=(Y);(V).z=(Z)

#ifdef __cplusplus
#define Vec2f Vec2f_
#define Vec3f Vec3f_
#define Vec3s Vec3s_
#endif

typedef struct {
    float x, y;
} Vec2f; // size = 0x08

typedef struct {
    float x, y, z;
} Vec3f; // size = 0x0C

typedef struct {
    uint16_t x, y, z;
} Vec3us; // size = 0x06

typedef struct {
    int16_t x, y, z;
} Vec3s; // size = 0x06

typedef struct {
    int32_t x, y, z;
} Vec3i; // size = 0x0C

typedef struct {
    Vec3s center;
    int16_t radius;
} Sphere16; // size = 0x08

typedef struct {
    Vec3f center;
    float radius;
} Spheref; // size = 0x10

typedef struct {
    Vec3f normal;
    float originDist;
} Plane; // size = 0x10

typedef struct {
    Vec3f vtx[3];
    Plane plane;
} TriNorm; // size = 0x34

typedef struct {
    /* 0x0000 */ int16_t radius;
    /* 0x0002 */ int16_t height;
    /* 0x0004 */ int16_t yShift;
    /* 0x0006 */ Vec3s pos;
} Cylinder16; // size = 0x0C

typedef struct {
    /* 0x00 */ float radius;
    /* 0x04 */ float height;
    /* 0x08 */ float yShift;
    /* 0x0C */ Vec3f pos;
} Cylinderf; // size = 0x18

typedef struct {
    /* 0x0000 */ Vec3f point;
    /* 0x000C */ Vec3f dir;
} InfiniteLine; // size = 0x18

typedef struct {
    /* 0x0000 */ Vec3f a;
    /* 0x000C */ Vec3f b;
} Linef; // size = 0x18

// Defines a point in the spherical coordinate system
typedef struct {
    /* 0x00 */ float r;      // radius
    /* 0x04 */ int16_t pitch;  // polar (zenith) angle
    /* 0x06 */ int16_t yaw;    // azimuthal angle
} VecSph; // size = 0x08

#define LERP(x, y, scale) (((y) - (x)) * (scale) + (x))
#define LERP32(x, y, scale) ((int32_t)(((y) - (x)) * (scale)) + (x))
#define LERP16(x, y, scale) ((int16_t)(((y) - (x)) * (scale)) + (x))
#define F32_LERP(v0,v1,t) ((v0) * (1.0f - (t)) + (v1) * (t))
#define F32_LERPIMP(v0, v1, t) (v0 + ((v1 - v0) * t))
#define F32_LERPIMPINV(v0, v1, t) ((v0) + (((v1) - (v0)) / (t)))
#define BINANG_LERPIMP(v0, v1, t) ((v0) + (int16_t)(BINANG_SUB((v1), (v0)) * (t)))
#define BINANG_LERPIMPINV(v0, v1, t) ((v0) + BINANG_SUB((v1), (v0)) / (t))

#define VEC3F_LERPIMPDST(dst, v0, v1, t){ \
    (dst)->x = (v0)->x + (((v1)->x - (v0)->x) * t); \
    (dst)->y = (v0)->y + (((v1)->y - (v0)->y) * t); \
    (dst)->z = (v0)->z + (((v1)->z - (v0)->z) * t); \
}

#define IS_ZERO(f) (fabsf(f) < 0.008f)

// Trig macros
#define DEGF_TO_BINANG(degreesf) (int16_t)(degreesf * 182.04167f + .5f)
#define RADF_TO_BINANG(radf) (int16_t)(radf * (32768.0f / M_PI))
#define RADF_TO_DEGF(radf) (radf * (180.0f / M_PI))
#define DEGF_TO_RADF(degf) (degf * (M_PI / 180.0f))
#define BINANG_ROT180(angle) ((int16_t)(angle - 0x7FFF))
#define BINANG_SUB(a, b) ((int16_t)(a - b))
#define DEG_TO_RAD(degrees) ((degrees) * (M_PI / 180.0f))
#define BINANG_TO_DEGF(binang) ((float)binang * (360.0001525f / 65535.0f))
#define BINANG_TO_RAD(binang) (((float)binang / 32768.0f) * M_PI)

// Vector macros
#define SQXZ(vec) ((vec.x) * (vec.x) + (vec.z) * (vec.z))
#define DOTXZ(vec1, vec2) ((vec1.x) * (vec2.x) + (vec1.z) * (vec2.z))
#define SQXYZ(vec) ((vec.x) * (vec.x) + (vec.y) * (vec.y) + (vec.z) * (vec.z))
#define DOTXYZ(vec1, vec2) ((vec1.x) * (vec2.x) + (vec1.y) * (vec2.y) + (vec1.z) * (vec2.z))

#endif
