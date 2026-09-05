#include <runtime/log/Log.hpp>
#include "colViewer.h"
#include "rendering/FrameInterpolation.h"
#include "platform/simulation/AuthoritativePlayerHitRig.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <vector>
#include <map>
#include <cmath>

extern "C" {
#include <z64.h>
#include "variables.h"
#include "functions.h"
#include "macros.h"
extern PlayState* gPlayState;
}

static bool sColViewerEnabled = false;

static std::vector<Gfx> opaDl;
static std::vector<Gfx> xluDl;
static std::vector<Vtx> vtxDl;
static std::vector<Mtx> mtxDl;
struct PlayerCollisionRecord {
    Game::Simulation::PlayerSnapshot snapshot{};
    std::chrono::steady_clock::time_point receivedAt{};
    std::array<Vec3f, PLAYER_LIMB_MAX> renderedLimbOrigins{};
    std::array<bool, PLAYER_LIMB_MAX> renderedLimbs{};
    Game::Simulation::ArticulatedPlayerHitRig renderedRig{};
    std::chrono::steady_clock::time_point renderedAt{};
    bool hasRenderedRig = false;
};
static std::map<int32_t, PlayerCollisionRecord> playerHitRigs;
static PlayerCollisionRecord localPlayerHitRig;
static int32_t localCollisionPlayerId = -1;

Game::Simulation::Vec3 ToSimulationVector(const Vec3f& value) {
    return { value.x, value.y, value.z };
}

bool BuildRenderedPlayerHitRig(PlayerCollisionRecord& record) {
    using namespace Game::Simulation;
    constexpr std::array<int32_t, 12> requiredLimbs = {
        PLAYER_LIMB_R_THIGH,
        PLAYER_LIMB_R_SHIN,      PLAYER_LIMB_R_FOOT,
        PLAYER_LIMB_L_THIGH,     PLAYER_LIMB_L_SHIN,
        PLAYER_LIMB_L_FOOT,      PLAYER_LIMB_HEAD,
        PLAYER_LIMB_COLLAR,
        PLAYER_LIMB_L_SHOULDER,  PLAYER_LIMB_L_FOREARM,
        PLAYER_LIMB_R_SHOULDER,  PLAYER_LIMB_R_FOREARM,
    };
    for (const int32_t limb : requiredLimbs) {
        if (!record.renderedLimbs[limb]) return false;
    }
    if (!record.renderedLimbs[PLAYER_LIMB_L_HAND] ||
        !record.renderedLimbs[PLAYER_LIMB_R_HAND]) {
        return false;
    }

    AuthoritativePlayerSkeletonPose pose{};
    const auto origin = [&](int32_t limb) {
        return ToSimulationVector(record.renderedLimbOrigins[limb]);
    };
    pose[PlayerHitJoint::HeadBase] = origin(PLAYER_LIMB_HEAD);
    const Vec3 collar = origin(PLAYER_LIMB_COLLAR);
    Vec3 headDirection = HitRigDetail::Subtract(
        pose[PlayerHitJoint::HeadBase], collar);
    const float headLengthSquared = HitRigDetail::Dot(headDirection, headDirection);
    if (headLengthSquared <= 0.000001f) return false;
    headDirection = HitRigDetail::Scale(
        headDirection, 13.0f / std::sqrt(headLengthSquared));
    pose[PlayerHitJoint::HeadTop] = HitRigDetail::Add(
        pose[PlayerHitJoint::HeadBase], headDirection);
    pose[PlayerHitJoint::LeftShoulder] = origin(PLAYER_LIMB_L_SHOULDER);
    pose[PlayerHitJoint::LeftElbow] = origin(PLAYER_LIMB_L_FOREARM);
    pose[PlayerHitJoint::LeftWrist] = origin(PLAYER_LIMB_L_HAND);
    pose[PlayerHitJoint::RightShoulder] = origin(PLAYER_LIMB_R_SHOULDER);
    pose[PlayerHitJoint::RightElbow] = origin(PLAYER_LIMB_R_FOREARM);
    pose[PlayerHitJoint::RightWrist] = origin(PLAYER_LIMB_R_HAND);
    pose[PlayerHitJoint::LeftHip] = origin(PLAYER_LIMB_L_THIGH);
    pose[PlayerHitJoint::LeftKnee] = origin(PLAYER_LIMB_L_SHIN);
    pose[PlayerHitJoint::LeftAnkle] = origin(PLAYER_LIMB_L_FOOT);
    pose[PlayerHitJoint::RightHip] = origin(PLAYER_LIMB_R_THIGH);
    pose[PlayerHitJoint::RightKnee] = origin(PLAYER_LIMB_R_SHIN);
    pose[PlayerHitJoint::RightAnkle] = origin(PLAYER_LIMB_R_FOOT);

    // UPPER and WAIST are display-list pivots. Depending on the current Link
    // animation their world origins can coincide, which collapses both body
    // prisms into flat hexagons. Build the body axis from separated skeletal
    // landmarks instead: the centers of the hips and shoulders. Splitting
    // that axis near the hips preserves the proportions of the authoritative
    // rig while following the rendered character's bend and rotation.
    const auto midpoint = [](const Vec3& left, const Vec3& right) {
        return HitRigDetail::Scale(HitRigDetail::Add(left, right), 0.5f);
    };
    const Vec3 hipCenter = midpoint(pose[PlayerHitJoint::LeftHip],
                                    pose[PlayerHitJoint::RightHip]);
    const Vec3 shoulderCenter = midpoint(pose[PlayerHitJoint::LeftShoulder],
                                         pose[PlayerHitJoint::RightShoulder]);
    const Vec3 bodyAxis = HitRigDetail::Subtract(shoulderCenter, hipCenter);
    pose[PlayerHitJoint::WaistBottom] = hipCenter;
    pose[PlayerHitJoint::TorsoBottom] = HitRigDetail::Add(
        hipCenter, HitRigDetail::Scale(bodyAxis, 0.3f));
    pose[PlayerHitJoint::TorsoTop] = shoulderCenter;

    record.renderedRig = BuildArticulatedPlayerHitRig(
        pose, kAdultLinkHitRigDimensions);
    record.renderedAt = std::chrono::steady_clock::now();
    record.hasRenderedRig = true;
    return true;
}

// These DLs contain a cylinder/sphere model scaled to 128x (to have less error)
// The idea is to push a model view matrix, then draw the DL, to draw the shape somewhere with a certain size
static std::vector<Gfx> cylinderGfx;
static std::vector<Vtx> cylinderVtx;
static std::vector<Gfx> sphereGfx;
static std::vector<Vtx> sphereVtx;
static std::vector<Gfx> hitPrismGfx;
static std::vector<Vtx> hitPrismVtx;

// Calculates the normal for a triangle at the 3 specified points
void CalcTriNorm(const Vec3f& v1, const Vec3f& v2, const Vec3f& v3, Vec3f& norm) {
    norm.x = (v2.y - v1.y) * (v3.z - v1.z) - (v2.z - v1.z) * (v3.y - v1.y);
    norm.y = (v2.z - v1.z) * (v3.x - v1.x) - (v2.x - v1.x) * (v3.z - v1.z);
    norm.z = (v2.x - v1.x) * (v3.y - v1.y) - (v2.y - v1.y) * (v3.x - v1.x);
    float norm_d = sqrtf(norm.x * norm.x + norm.y * norm.y + norm.z * norm.z);
    if (norm_d != 0.f) {
        norm.x *= 127.f / norm_d;
        norm.y *= 127.f / norm_d;
        norm.z *= 127.f / norm_d;
    }
}

// Various macros used for creating verticies and rendering that aren't in gbi.h
#define G_CC_MODULATERGB_PRIM_ENVA PRIMITIVE, 0, SHADE, 0, 0, 0, 0, ENVIRONMENT
#define G_CC_PRIMITIVE_ENVA 0, 0, 0, PRIMITIVE, 0, 0, 0, ENVIRONMENT
#define qs105(n) ((int16_t)((n)*0x0020))
#define gdSPDefVtxN(x, y, z, s, t, nx, ny, nz, ca)                                            \
    {                                                                                         \
        .n = {.ob = { x, y, z }, .tc = { qs105(s), qs105(t) }, .n = { nx, ny, nz }, .a = ca } \
    }

void CreateCylinderData() {
    constexpr int32_t CYL_DIVS = 12;
    cylinderGfx.reserve(5 + CYL_DIVS * 2);
    cylinderVtx.reserve(2 + CYL_DIVS * 2);

    cylinderVtx.push_back(gdSPDefVtxN(0, 0, 0, 0, 0, 0, -127, 0, 0xFF));  // Bottom center vertex
    cylinderVtx.push_back(gdSPDefVtxN(0, 128, 0, 0, 0, 0, 127, 0, 0xFF)); // Top center vertex
    // Create two rings of vertices
    for (int i = 0; i < CYL_DIVS; ++i) {
        short vtx_x = floorf(0.5f + cosf(2.f * M_PI * i / CYL_DIVS) * 128.f);
        short vtx_z = floorf(0.5f - sinf(2.f * M_PI * i / CYL_DIVS) * 128.f);
        signed char norm_x = cosf(2.f * M_PI * i / CYL_DIVS) * 127.f;
        signed char norm_z = -sinf(2.f * M_PI * i / CYL_DIVS) * 127.f;
        cylinderVtx.push_back(gdSPDefVtxN(vtx_x, 0, vtx_z, 0, 0, norm_x, 0, norm_z, 0xFF));
        cylinderVtx.push_back(gdSPDefVtxN(vtx_x, 128, vtx_z, 0, 0, norm_x, 0, norm_z, 0xFF));
    }

    // Draw edges
    cylinderGfx.push_back(gsSPSetGeometryMode(G_CULL_BACK | G_SHADING_SMOOTH));
    cylinderGfx.push_back(gsSPVertex((uintptr_t)cylinderVtx.data(), 2 + CYL_DIVS * 2, 0));
    for (int i = 0; i < CYL_DIVS; ++i) {
        int p = (i + CYL_DIVS - 1) % CYL_DIVS;
        int v[4] = {
            2 + p * 2 + 0,
            2 + i * 2 + 0,
            2 + i * 2 + 1,
            2 + p * 2 + 1,
        };
        cylinderGfx.push_back(gsSP2Triangles(v[0], v[1], v[2], 0, v[0], v[2], v[3], 0));
    }

    // Draw top & bottom
    cylinderGfx.push_back(gsSPClearGeometryMode(G_SHADING_SMOOTH));
    for (int i = 0; i < CYL_DIVS; ++i) {
        int p = (i + CYL_DIVS - 1) % CYL_DIVS;
        int v[4] = {
            2 + p * 2 + 0,
            2 + i * 2 + 0,
            2 + i * 2 + 1,
            2 + p * 2 + 1,
        };
        cylinderGfx.push_back(gsSP2Triangles(0, v[1], v[0], 0, 1, v[3], v[2], 0));
    }

    cylinderGfx.push_back(gsSPClearGeometryMode(G_CULL_BACK));
    cylinderGfx.push_back(gsSPEndDisplayList());
}

// The articulated player rig changes every animation frame. Keep its geometry
// immutable and move each prism with a model matrix, just like the existing
// collider cylinders. Baking world-space endpoints into a reused Vtx buffer
// lets the PC renderer retain the first uploaded geometry by address.
void CreateHitPrismData() {
    constexpr int32_t kSides = 6;
    hitPrismGfx.reserve(5 + kSides * 2);
    hitPrismVtx.reserve(2 + kSides * 2);

    hitPrismVtx.push_back(gdSPDefVtxN(0, 0, 0, 0, 0, 0, -127, 0, 0xFF));
    hitPrismVtx.push_back(gdSPDefVtxN(0, 128, 0, 0, 0, 0, 127, 0, 0xFF));
    for (int32_t side = 0; side < kSides; ++side) {
        const float angle = 2.0f * M_PI * side / kSides;
        const int16_t x = (int16_t)floorf(0.5f + cosf(angle) * 128.0f);
        const int16_t z = (int16_t)floorf(0.5f - sinf(angle) * 128.0f);
        const int8_t nx = (int8_t)(cosf(angle) * 127.0f);
        const int8_t nz = (int8_t)(-sinf(angle) * 127.0f);
        hitPrismVtx.push_back(gdSPDefVtxN(x, 0, z, 0, 0, nx, 0, nz, 0xFF));
        hitPrismVtx.push_back(gdSPDefVtxN(x, 128, z, 0, 0, nx, 0, nz, 0xFF));
    }

    hitPrismGfx.push_back(gsSPSetGeometryMode(G_CULL_BACK | G_SHADING_SMOOTH));
    hitPrismGfx.push_back(gsSPVertex((uintptr_t)hitPrismVtx.data(),
                                     2 + kSides * 2, 0));
    for (int32_t side = 0; side < kSides; ++side) {
        const int32_t previous = (side + kSides - 1) % kSides;
        const int32_t bottomPrevious = 2 + previous * 2;
        const int32_t bottom = 2 + side * 2;
        const int32_t top = bottom + 1;
        const int32_t topPrevious = bottomPrevious + 1;
        hitPrismGfx.push_back(gsSP2Triangles(bottomPrevious, bottom, top, 0,
                                             bottomPrevious, top, topPrevious, 0));
    }
    hitPrismGfx.push_back(gsSPClearGeometryMode(G_SHADING_SMOOTH));
    for (int32_t side = 0; side < kSides; ++side) {
        const int32_t previous = (side + kSides - 1) % kSides;
        const int32_t bottomPrevious = 2 + previous * 2;
        const int32_t bottom = 2 + side * 2;
        const int32_t top = bottom + 1;
        const int32_t topPrevious = bottomPrevious + 1;
        hitPrismGfx.push_back(gsSP2Triangles(0, bottom, bottomPrevious, 0,
                                             1, topPrevious, top, 0));
    }
    hitPrismGfx.push_back(gsSPClearGeometryMode(G_CULL_BACK));
    hitPrismGfx.push_back(gsSPEndDisplayList());
}

// This subdivides a face into four tris by placing new verticies at the midpoints of the sides (Like a triforce!), then
// blowing up the verticies so they are on the unit sphere
void CreateSphereFace(std::vector<std::tuple<size_t, size_t, size_t>>& faces, int32_t v0Index, int32_t v1Index,
                      int32_t v2Index) {
    size_t nextIndex = sphereVtx.size();

    size_t v01Index = nextIndex;
    size_t v12Index = nextIndex + 1;
    size_t v20Index = nextIndex + 2;

    faces.emplace_back(v0Index, v01Index, v20Index);
    faces.emplace_back(v1Index, v12Index, v01Index);
    faces.emplace_back(v2Index, v20Index, v12Index);
    faces.emplace_back(v01Index, v12Index, v20Index);

    const Vtx& v0 = sphereVtx[v0Index];
    const Vtx& v1 = sphereVtx[v1Index];
    const Vtx& v2 = sphereVtx[v2Index];

    // Create 3 new verticies at the midpoints
    Vec3f vs[3] = {
        Vec3f{ (v0.n.ob[0] + v1.n.ob[0]) / 2.0f, (v0.n.ob[1] + v1.n.ob[1]) / 2.0f, (v0.n.ob[2] + v1.n.ob[2]) / 2.0f },
        Vec3f{ (v1.n.ob[0] + v2.n.ob[0]) / 2.0f, (v1.n.ob[1] + v2.n.ob[1]) / 2.0f, (v1.n.ob[2] + v2.n.ob[2]) / 2.0f },
        Vec3f{ (v2.n.ob[0] + v0.n.ob[0]) / 2.0f, (v2.n.ob[1] + v0.n.ob[1]) / 2.0f, (v2.n.ob[2] + v0.n.ob[2]) / 2.0f }
    };

    // Normalize vertex positions so they are on the sphere
    for (int32_t vAddIndex = 0; vAddIndex < 3; vAddIndex++) {
        Vec3f& v = vs[vAddIndex];
        float mag = sqrtf(v.x * v.x + v.y * v.y + v.z * v.z);
        v.x /= mag;
        v.y /= mag;
        v.z /= mag;
        sphereVtx.push_back(gdSPDefVtxN((short)(v.x * 127), (short)(v.y * 127), (short)(v.z * 127), 0, 0,
                                        (signed char)(v.x * 127), (signed char)(v.y * 127), (signed char)(v.z * 127),
                                        0xFF));
    }
}

// Creates a sphere following the idea in here:
// http://blog.andreaskahler.com/2009/06/creating-icosphere-mesh-in-code.html Spcifically, create a icosahedron by
// realizing that the points can be placed on 3 rectangles that are on each unit plane. Then, subdividing each face.
void CreateSphereData() {
    std::vector<Vec3f> base;

    float d = (1.0f + sqrtf(5.0f)) / 2.0f;

    // Create the 12 starting verticies, 4 on each rectangle
    base.emplace_back(Vec3f({ -1, d, 0 }));
    base.emplace_back(Vec3f({ 1, d, 0 }));
    base.emplace_back(Vec3f({ -1, -d, 0 }));
    base.emplace_back(Vec3f({ 1, -d, 0 }));

    base.emplace_back(Vec3f({ 0, -1, d }));
    base.emplace_back(Vec3f({ 0, 1, d }));
    base.emplace_back(Vec3f({ 0, -1, -d }));
    base.emplace_back(Vec3f({ 0, 1, -d }));

    base.emplace_back(Vec3f({ d, 0, -1 }));
    base.emplace_back(Vec3f({ d, 0, 1 }));
    base.emplace_back(Vec3f({ -d, 0, -1 }));
    base.emplace_back(Vec3f({ -d, 0, 1 }));

    // Normalize verticies so they are on the unit sphere
    for (Vec3f& v : base) {
        float mag = sqrtf(v.x * v.x + v.y * v.y + v.z * v.z);
        v.x /= mag;
        v.y /= mag;
        v.z /= mag;
        sphereVtx.push_back(gdSPDefVtxN((short)(v.x * 128), (short)(v.y * 128), (short)(v.z * 128), 0, 0,
                                        (signed char)(v.x * 127), (signed char)(v.y * 127), (signed char)(v.z * 127),
                                        0xFF));
    }

    std::vector<std::tuple<size_t, size_t, size_t>> faces;

    // Subdivide faces
    CreateSphereFace(faces, 0, 11, 5);
    CreateSphereFace(faces, 0, 5, 1);
    CreateSphereFace(faces, 0, 1, 7);
    CreateSphereFace(faces, 0, 7, 10);
    CreateSphereFace(faces, 0, 10, 11);

    CreateSphereFace(faces, 1, 5, 9);
    CreateSphereFace(faces, 5, 11, 4);
    CreateSphereFace(faces, 11, 10, 2);
    CreateSphereFace(faces, 10, 7, 6);
    CreateSphereFace(faces, 7, 1, 8);

    CreateSphereFace(faces, 3, 9, 4);
    CreateSphereFace(faces, 3, 4, 2);
    CreateSphereFace(faces, 3, 2, 6);
    CreateSphereFace(faces, 3, 6, 8);
    CreateSphereFace(faces, 3, 8, 9);

    CreateSphereFace(faces, 4, 9, 5);
    CreateSphereFace(faces, 2, 4, 11);
    CreateSphereFace(faces, 6, 2, 10);
    CreateSphereFace(faces, 8, 6, 7);
    CreateSphereFace(faces, 9, 8, 1);

    size_t vtxStartIndex = sphereVtx.size();
    sphereVtx.reserve(sphereVtx.size() + faces.size() * 3);
    for (size_t faceIndex = 0; faceIndex < faces.size(); faceIndex++) {
        sphereVtx.push_back(sphereVtx[std::get<0>(faces[faceIndex])]);
        sphereVtx.push_back(sphereVtx[std::get<1>(faces[faceIndex])]);
        sphereVtx.push_back(sphereVtx[std::get<2>(faces[faceIndex])]);
        sphereGfx.push_back(gsSPVertex((uintptr_t)(sphereVtx.data() + vtxStartIndex + faceIndex * 3), 3, 0));
        sphereGfx.push_back(gsSP1Triangle(0, 1, 2, 0));
    }

    sphereGfx.push_back(gsSPEndDisplayList());
}

// Initializes a translucent decal display list for collision geometry.
void InitGfx(std::vector<Gfx>& gfx) {
    uint32_t rm = Z_CMP | IM_RD | CVG_DST_FULL | FORCE_BL | ZMODE_DEC;
    uint32_t blc1 = GBL_c1(G_BL_CLR_IN, G_BL_A_IN, G_BL_CLR_MEM, G_BL_1MA);
    uint32_t blc2 = GBL_c2(G_BL_CLR_IN, G_BL_A_IN, G_BL_CLR_MEM, G_BL_1MA);

    gfx.push_back(gsSPTexture(0, 0, 0, G_TX_RENDERTILE, G_OFF));
    gfx.push_back(gsDPSetCycleType(G_CYC_1CYCLE));
    gfx.push_back(gsDPSetRenderMode(rm | blc1, rm | blc2));

    gfx.push_back(gsDPSetCombineMode(G_CC_PRIMITIVE_ENVA, G_CC_PRIMITIVE_ENVA));
    gfx.push_back(gsSPLoadGeometryMode(G_ZBUFFER));
    gfx.push_back(gsDPSetEnvColor(0xFF, 0xFF, 0xFF, 0x80));
}

// Draws a dynapoly structure (scenes or Bg Actors)
void DrawDynapoly(std::vector<Gfx>& dl, CollisionHeader* col, int32_t bgId) {
    Color_RGBA8 color = { 255, 255, 255, 255 };

    uint32_t lastColorR = color.r;
    uint32_t lastColorG = color.g;
    uint32_t lastColorB = color.b;

    dl.push_back(gsDPSetPrimColor(0, 0, color.r, color.g, color.b, 255));

    // This keeps track of if we have processed a poly, but not drawn it yet so we can batch them.
    // This saves several hundred commands in larger scenes
    bool previousPoly = false;

    for (int i = 0; i < col->numPolygons; i++) {
        CollisionPoly* poly = &col->polyList[i];

        if (SurfaceType_IsHookshotSurface(&gPlayState->colCtx, poly, bgId)) {
            color = { 128, 128, 255, 255 };
        } else if (func_80041D94(&gPlayState->colCtx, poly, bgId) > 0x01) {
            color = { 192, 0, 192, 255 };
        } else if (func_80041E80(&gPlayState->colCtx, poly, bgId) == 0x0C) {
            color = { 255, 0, 0, 255 };
        } else if (SurfaceType_GetSceneExitIndex(&gPlayState->colCtx, poly, bgId) ||
                   func_80041E80(&gPlayState->colCtx, poly, bgId) == 0x05) {
            color = { 0, 255, 0, 255 };
        } else if (func_80041D4C(&gPlayState->colCtx, poly, bgId) != 0 ||
                   SurfaceType_IsWallDamage(&gPlayState->colCtx, poly, bgId)) {
            color = { 192, 255, 192, 255 };
        } else if (SurfaceType_GetSlope(&gPlayState->colCtx, poly, bgId) == 0x01) {
            color = { 255, 255, 128, 255 };
        } else {
            color = { 255, 255, 255, 255 };
        }

        if (color.r != lastColorR || color.g != lastColorG || color.b != lastColorB) {
            // Color changed, flush previous poly
            if (previousPoly) {
                dl.push_back(gsSPVertex((uintptr_t)&vtxDl.at(vtxDl.size() - 3), 3, 0));
                dl.push_back(gsSP1Triangle(0, 1, 2, 0));
                previousPoly = false;
            }
            dl.push_back(gsDPSetPrimColor(0, 0, color.r, color.g, color.b, 255));
        }
        lastColorR = color.r;
        lastColorG = color.g;
        lastColorB = color.b;

        Vec3s* va = &col->vtxList[COLPOLY_VTX_INDEX(poly->flags_vIA)];
        Vec3s* vb = &col->vtxList[COLPOLY_VTX_INDEX(poly->flags_vIB)];
        Vec3s* vc = &col->vtxList[COLPOLY_VTX_INDEX(poly->vIC)];
        vtxDl.push_back(gdSPDefVtxN(va->x, va->y, va->z, 0, 0, (signed char)(poly->normal.x / 0x100),
                                    (signed char)(poly->normal.y / 0x100), (signed char)(poly->normal.z / 0x100),
                                    0xFF));
        vtxDl.push_back(gdSPDefVtxN(vb->x, vb->y, vb->z, 0, 0, (signed char)(poly->normal.x / 0x100),
                                    (signed char)(poly->normal.y / 0x100), (signed char)(poly->normal.z / 0x100),
                                    0xFF));
        vtxDl.push_back(gdSPDefVtxN(vc->x, vc->y, vc->z, 0, 0, (signed char)(poly->normal.x / 0x100),
                                    (signed char)(poly->normal.y / 0x100), (signed char)(poly->normal.z / 0x100),
                                    0xFF));

        if (previousPoly) {
            dl.push_back(gsSPVertex((uintptr_t)&vtxDl.at(vtxDl.size() - 6), 6, 0));
            dl.push_back(gsSP2Triangles(0, 1, 2, 0, 3, 4, 5, 0));
            previousPoly = false;
        } else {
            previousPoly = true;
        }
    }

    // Flush previous poly if this is the end and there's no more coming
    if (previousPoly) {
        dl.push_back(gsSPVertex((uintptr_t)&vtxDl.at(vtxDl.size() - 3), 3, 0));
        dl.push_back(gsSP1Triangle(0, 1, 2, 0));
        previousPoly = false;
    }
}

// Draws the scene
void DrawSceneCollision() {
    std::vector<Gfx>& dl = xluDl;
    InitGfx(dl);
    dl.push_back(gsSPMatrix(&gMtxClear, G_MTX_MODELVIEW | G_MTX_LOAD | G_MTX_NOPUSH));

    DrawDynapoly(dl, gPlayState->colCtx.colHeader, BGCHECK_SCENE);
}

// Draws all Bg Actors
void DrawBgActorCollision() {
    std::vector<Gfx>& dl = xluDl;
    InitGfx(dl);
    dl.push_back(gsSPMatrix(&gMtxClear, G_MTX_MODELVIEW | G_MTX_LOAD | G_MTX_NOPUSH));

    for (int32_t bgIndex = 0; bgIndex < BG_ACTOR_MAX; bgIndex++) {
        if (gPlayState->colCtx.dyna.bgActorFlags[bgIndex] & 1) {
            BgActor& bg = gPlayState->colCtx.dyna.bgActors[bgIndex];
            Mtx m;
            MtxF mf;
            SkinMatrix_SetTranslateRotateYXZScale(&mf, bg.curTransform.scale.x, bg.curTransform.scale.y,
                                                  bg.curTransform.scale.z, bg.curTransform.rot.x, bg.curTransform.rot.y,
                                                  bg.curTransform.rot.z, bg.curTransform.pos.x, bg.curTransform.pos.y,
                                                  bg.curTransform.pos.z);
            guMtxF2L(mf.mf, &m);
            mtxDl.push_back(m);
            dl.push_back(gsSPMatrix(&mtxDl.back(), G_MTX_MODELVIEW | G_MTX_LOAD | G_MTX_PUSH));

            DrawDynapoly(dl, bg.colHeader, bgIndex);

            dl.push_back(gsSPPopMatrix(G_MTX_MODELVIEW));
        }
    }
}

// Draws a quad
void DrawQuad(std::vector<Gfx>& dl, Vec3f& v0, Vec3f& v1, Vec3f& v2, Vec3f& v3) {
    Vec3f norm;
    CalcTriNorm(v0, v1, v2, norm);

    vtxDl.push_back(gdSPDefVtxN((short)v0.x, (short)v0.y, (short)v0.z, 0, 0, (signed char)norm.x, (signed char)norm.y,
                                (signed char)norm.z, 0xFF));
    vtxDl.push_back(gdSPDefVtxN((short)v1.x, (short)v1.y, (short)v1.z, 0, 0, (signed char)norm.x, (signed char)norm.y,
                                (signed char)norm.z, 0xFF));
    vtxDl.push_back(gdSPDefVtxN((short)v2.x, (short)v2.y, (short)v2.z, 0, 0, (signed char)norm.x, (signed char)norm.y,
                                (signed char)norm.z, 0xFF));
    vtxDl.push_back(gdSPDefVtxN((short)v3.x, (short)v3.y, (short)v3.z, 0, 0, (signed char)norm.x, (signed char)norm.y,
                                (signed char)norm.z, 0xFF));
    dl.push_back(gsSPVertex((uintptr_t)&vtxDl.at(vtxDl.size() - 4), 4, 0));
    dl.push_back(gsSP2Triangles(0, 1, 2, 0, 0, 2, 3, 0));
}

void DrawHitPrism(std::vector<Gfx>& dl,
                  const Game::Simulation::PlayerHitPrism& prism,
                  const void* rigIdentity, int32_t prismIndex) {
    using namespace Game::Simulation;
    const Vec3 axisVector = HitRigDetail::Subtract(prism.end, prism.start);
    const float lengthSquared = HitRigDetail::Dot(axisVector, axisVector);
    if (lengthSquared <= 0.000001f || prism.outerRadius <= 0.0f) return;
    const float length = std::sqrt(lengthSquared);
    const Vec3 axis = HitRigDetail::Scale(axisVector, 1.0f / length);
    const Vec3 reference = std::abs(axis.y) < 0.9f ? Vec3{ 0.0f, 1.0f, 0.0f }
                                                   : Vec3{ 1.0f, 0.0f, 0.0f };
    const Vec3 cross = HitRigDetail::Cross(axis, reference);
    const Vec3 radialX = HitRigDetail::Scale(
        cross, 1.0f / std::sqrt(HitRigDetail::Dot(cross, cross)));
    const Vec3 radialZ = HitRigDetail::Cross(axis, radialX);
    const float radialScale = prism.outerRadius / 128.0f;
    const float axisScale = length / 128.0f;

    MtxF transform{};
    // Columns map the fixed mesh's local X/Y/Z axes into world space.
    transform.xx = radialX.x * radialScale;
    transform.yx = radialX.y * radialScale;
    transform.zx = radialX.z * radialScale;
    transform.xy = axis.x * axisScale;
    transform.yy = axis.y * axisScale;
    transform.zy = axis.z * axisScale;
    transform.xz = radialZ.x * radialScale;
    transform.yz = radialZ.y * radialScale;
    transform.zz = radialZ.z * radialScale;
    transform.xw = prism.start.x;
    transform.yw = prism.start.y;
    transform.zw = prism.start.z;
    transform.ww = 1.0f;

    mtxDl.emplace_back();
    // Keep each articulated limb on a stable interpolation path. Link's mesh
    // matrices are interpolated between the 20 Hz game updates; converting
    // these transforms directly with guMtxF2L bypassed that system and left
    // the F1 rig frozen on the last simulation pose during interpolated
    // frames. The normal matrix conversion records both poses and replaces
    // this destination matrix with the current interpolated transform.
    FrameInterpolation_RecordOpenChild(rigIdentity, prismIndex);
    Matrix_MtxFToMtx(&transform, &mtxDl.back());
    dl.push_back(gsSPMatrix(&mtxDl.back(),
                            G_MTX_MODELVIEW | G_MTX_LOAD | G_MTX_PUSH));
    dl.push_back(gsSPDisplayList(hitPrismGfx.data()));
    dl.push_back(gsSPPopMatrix(G_MTX_MODELVIEW));
    FrameInterpolation_RecordCloseChild();
}

void DrawAuthoritativePlayerCollision() {
    using namespace Game::Simulation;
    std::vector<Gfx>& dl = xluDl;
    InitGfx(dl);
    // The articulated prisms deliberately fit inside the visible Link mesh.
    // Draw this one added F1 layer without depth rejection so the real combat
    // surfaces can be inspected; all existing collision-viewer layers retain
    // their original depth-tested material.
    dl.push_back(gsSPClearGeometryMode(G_ZBUFFER));
    dl.push_back(gsDPSetRenderMode(G_RM_XLU_SURF, G_RM_XLU_SURF2));
    dl.push_back(gsDPSetEnvColor(255, 255, 255, 208));
    dl.push_back(gsSPMatrix(&gMtxClear,
                            G_MTX_MODELVIEW | G_MTX_LOAD | G_MTX_NOPUSH));
    dl.push_back(gsDPSetPrimColor(0, 0, 255, 96, 0, 255));

    const bool authoritativeLocalRig =
        localCollisionPlayerId >= 0 &&
        playerHitRigs.contains(localCollisionPlayerId);
    if (!authoritativeLocalRig && localPlayerHitRig.hasRenderedRig &&
        localPlayerHitRig.snapshot.sceneId == gPlayState->sceneNum) {
        for (std::size_t index = 0;
             index < localPlayerHitRig.renderedRig.prisms.size(); ++index) {
            DrawHitPrism(dl, localPlayerHitRig.renderedRig.prisms[index],
                         &localPlayerHitRig, static_cast<int32_t>(index));
        }
    }

    for (const auto& [playerId, record] : playerHitRigs) {
        if (record.snapshot.sceneId != gPlayState->sceneNum) continue;
        // Combat remains fixed-tick and server authoritative. The diagnostic
        // advances that same semantic pose only across the short interval
        // since its newest snapshot, matching the continuously sampled Link
        // animation instead of displaying a frozen snapshot mesh.
        constexpr float kMaximumPoseAdvanceSeconds = 0.1f;
        const float elapsedSeconds = std::clamp(
            std::chrono::duration<float>(std::chrono::steady_clock::now() -
                                         record.receivedAt).count(),
            0.0f, kMaximumPoseAdvanceSeconds);
        // When this player was rendered in the current frame, inspect the
        // exact limb pivots used by Link's skeleton. The deterministic
        // authoritative pose remains the fallback for culled/offscreen
        // players and remains the server's combat representation.
        constexpr float kRenderedPoseLifetimeSeconds = 0.25f;
        const bool renderedPoseCurrent = record.hasRenderedRig &&
            std::chrono::duration<float>(std::chrono::steady_clock::now() -
                                         record.renderedAt).count() <=
                kRenderedPoseLifetimeSeconds;
        const ArticulatedPlayerHitRig rig = renderedPoseCurrent
            ? record.renderedRig
            : BuildAuthoritativePlayerHitRig(record.snapshot, elapsedSeconds);
        for (std::size_t index = 0; index < rig.prisms.size(); ++index) {
            DrawHitPrism(dl, rig.prisms[index], &record,
                         static_cast<int32_t>(index));
        }
    }
}

// Draws a list of Col Check objects
void DrawColCheckList(std::vector<Gfx>& dl, Collider** objects, int32_t count) {
    for (int32_t colIndex = 0; colIndex < count; colIndex++) {
        Collider* col = objects[colIndex];
        const Player* localPlayer = gPlayState ? GET_PLAYER(gPlayState) : nullptr;
        if (localPlayer && localPlayer->authoritativeBodyHidden &&
            col->actor == &localPlayer->actor) {
            continue;
        }
        switch (col->shape) {
            case COLSHAPE_JNTSPH: {
                ColliderJntSph* jntSph = (ColliderJntSph*)col;

                for (int32_t sphereIndex = 0; sphereIndex < jntSph->count; sphereIndex++) {
                    ColliderJntSphElement* sph = &jntSph->elements[sphereIndex];

                    Mtx m;
                    MtxF mf;
                    SkinMatrix_SetTranslate(&mf, sph->dim.worldSphere.center.x, sph->dim.worldSphere.center.y,
                                            sph->dim.worldSphere.center.z);
                    MtxF ms;
                    int32_t radius = sph->dim.worldSphere.radius == 0 ? 1 : sph->dim.worldSphere.radius;
                    SkinMatrix_SetScale(&ms, radius / 128.0f, radius / 128.0f, radius / 128.0f);
                    MtxF dest;
                    SkinMatrix_MtxFMtxFMult(&mf, &ms, &dest);
                    guMtxF2L(dest.mf, &m);
                    mtxDl.push_back(m);

                    dl.push_back(gsSPMatrix(&mtxDl.back(), G_MTX_MODELVIEW | G_MTX_LOAD | G_MTX_PUSH));
                    dl.push_back(gsSPDisplayList(sphereGfx.data()));
                    dl.push_back(gsSPPopMatrix(G_MTX_MODELVIEW));
                }
            } break;
            case COLSHAPE_CYLINDER: {
                ColliderCylinder* cyl = (ColliderCylinder*)col;

                Mtx m;
                MtxF mt;
                SkinMatrix_SetTranslate(&mt, cyl->dim.pos.x, cyl->dim.pos.y + cyl->dim.yShift, cyl->dim.pos.z);
                MtxF ms;
                int32_t radius = cyl->dim.radius == 0 ? 1 : cyl->dim.radius;
                SkinMatrix_SetScale(&ms, radius / 128.0f, cyl->dim.height / 128.0f, radius / 128.0f);
                MtxF dest;
                SkinMatrix_MtxFMtxFMult(&mt, &ms, &dest);
                guMtxF2L(dest.mf, &m);
                mtxDl.push_back(m);

                dl.push_back(gsSPMatrix(&mtxDl.back(), G_MTX_MODELVIEW | G_MTX_LOAD | G_MTX_PUSH));
                dl.push_back(gsSPDisplayList(cylinderGfx.data()));
                dl.push_back(gsSPPopMatrix(G_MTX_MODELVIEW));
            } break;
            case COLSHAPE_TRIS: {
                ColliderTris* tris = (ColliderTris*)col;
                for (int32_t triIndex = 0; triIndex < tris->count; triIndex++) {
                    ColliderTrisElement* tri = &tris->elements[triIndex];

                    vtxDl.push_back(gdSPDefVtxN((short)tri->dim.vtx[0].x, (short)tri->dim.vtx[0].y,
                                                (short)tri->dim.vtx[0].z, 0, 0, (signed char)tri->dim.plane.normal.x,
                                                (signed char)tri->dim.plane.normal.y,
                                                (signed char)tri->dim.plane.normal.z, 0xFF));
                    vtxDl.push_back(gdSPDefVtxN((short)tri->dim.vtx[1].x, (short)tri->dim.vtx[1].y,
                                                (short)tri->dim.vtx[1].z, 0, 0, (signed char)tri->dim.plane.normal.x,
                                                (signed char)tri->dim.plane.normal.y,
                                                (signed char)tri->dim.plane.normal.z, 0xFF));
                    vtxDl.push_back(gdSPDefVtxN((short)tri->dim.vtx[2].x, (short)tri->dim.vtx[2].y,
                                                (short)tri->dim.vtx[2].z, 0, 0, (signed char)tri->dim.plane.normal.x,
                                                (signed char)tri->dim.plane.normal.y,
                                                (signed char)tri->dim.plane.normal.z, 0xFF));
                    dl.push_back(gsSPVertex((uintptr_t)&vtxDl.at(vtxDl.size() - 3), 3, 0));
                    dl.push_back(gsSP1Triangle(0, 1, 2, 0));
                }
            } break;
            case COLSHAPE_QUAD: {
                ColliderQuad* quad = (ColliderQuad*)col;
                DrawQuad(dl, quad->dim.quad[0], quad->dim.quad[2], quad->dim.quad[3], quad->dim.quad[1]);
            } break;
            default:
                break;
        }
    }
}

// Draws all Col Check objects
void DrawColCheckCollision() {
    std::vector<Gfx>& dl = xluDl;
    InitGfx(dl);
    dl.push_back(gsSPMatrix(&gMtxClear, G_MTX_MODELVIEW | G_MTX_LOAD | G_MTX_NOPUSH));

    CollisionCheckContext& col = gPlayState->colChkCtx;
    Color_RGBA8 color = { 255, 255, 255, 255 };
    dl.push_back(gsDPSetPrimColor(0, 0, color.r, color.g, color.b, 255));
    DrawColCheckList(dl, col.colOC, col.colOCCount);
    color = { 0, 0, 255, 255 };
    dl.push_back(gsDPSetPrimColor(0, 0, color.r, color.g, color.b, 255));
    DrawColCheckList(dl, col.colAC, col.colACCount);
    color = { 255, 0, 0, 255 };
    dl.push_back(gsDPSetPrimColor(0, 0, color.r, color.g, color.b, 255));

    DrawColCheckList(dl, col.colAT, col.colATCount);
}

// Draws a waterbox
void DrawWaterbox(std::vector<Gfx>& dl, WaterBox* water, float water_max_depth = -4000.0f) {
    // Skip waterboxes that would be disabled in current room
    int32_t room = ((water->properties >> 13) & 0x3F);
    if ((room != gPlayState->roomCtx.curRoom.num) && (room != 0x3F)) {
        return;
    }

    Vec3f vtx[] = {
        { water->xMin, water->ySurface, water->zMin + water->zLength },
        { water->xMin + water->xLength, water->ySurface, water->zMin + water->zLength },
        { water->xMin + water->xLength, water->ySurface, water->zMin },
        { water->xMin, water->ySurface, water->zMin },
        { water->xMin, water_max_depth, water->zMin + water->zLength },
        { water->xMin + water->xLength, water_max_depth, water->zMin + water->zLength },
        { water->xMin + water->xLength, water_max_depth, water->zMin },
        { water->xMin, water_max_depth, water->zMin },
    };
    DrawQuad(dl, vtx[0], vtx[1], vtx[2], vtx[3]);
    DrawQuad(dl, vtx[0], vtx[3], vtx[7], vtx[4]);
    DrawQuad(dl, vtx[1], vtx[0], vtx[4], vtx[5]);
    DrawQuad(dl, vtx[2], vtx[1], vtx[5], vtx[6]);
    DrawQuad(dl, vtx[3], vtx[2], vtx[6], vtx[7]);
}

extern "C" WaterBox zdWaterBox;
extern "C" float zdWaterBoxMinY;

// Draws all waterboxes
void DrawWaterboxList() {
    std::vector<Gfx>& dl = xluDl;
    InitGfx(dl);
    dl.push_back(gsSPMatrix(&gMtxClear, G_MTX_MODELVIEW | G_MTX_LOAD | G_MTX_NOPUSH));

    Color_RGBA8 color = { 0, 0, 255, 255 };

    dl.push_back(gsDPSetPrimColor(0, 0, color.r, color.g, color.b, 255));

    CollisionHeader* col = gPlayState->colCtx.colHeader;
    for (int32_t waterboxIndex = 0; waterboxIndex < col->numWaterBoxes; waterboxIndex++) {
        WaterBox* water = &col->waterBoxes[waterboxIndex];
        DrawWaterbox(dl, water);
    }

    // Zora's Domain has a special, hard-coded waterbox with a bottom so you can go under the waterfall
    if (gPlayState->sceneNum == SCENE_ZORAS_DOMAIN) {
        DrawWaterbox(dl, &zdWaterBox, zdWaterBoxMinY);
    }
}

// Resets a vector for the next frame and returns the capacity
template <typename T> size_t ResetVector(T& vec) {
    size_t oldSize = vec.size();
    vec.clear();
    // Reserve slightly more space than last frame to account for variance (such as different amounts of bg actors)
    vec.reserve(oldSize * 1.2);
    return vec.capacity();
}

extern "C" void DrawColViewer() {
    if (!sColViewerEnabled || gPlayState == nullptr) {
        return;
    }

    ResetVector(opaDl);
    ResetVector(xluDl);
    size_t vtxDlCapacity = ResetVector(vtxDl);
    size_t mtxDlCapacity = ResetVector(mtxDl);

    DrawSceneCollision();
    DrawBgActorCollision();
    DrawColCheckCollision();
    DrawAuthoritativePlayerCollision();
    DrawWaterboxList();

    // Check if we used up more space than we reserved. If so, redo the drawing with our new sizes.
    // This is because we resized the vectors while drawing, invalidating pointers to them.
    // This only matters for the Vtx and Mtx vectors.
    if ((vtxDl.size() > vtxDlCapacity) || (mtxDl.size() > mtxDlCapacity)) {
        ResetVector(opaDl);
        ResetVector(xluDl);
        vtxDlCapacity = ResetVector(vtxDl);
        mtxDlCapacity = ResetVector(mtxDl);

        DrawSceneCollision();
        DrawBgActorCollision();
        DrawColCheckCollision();
        DrawAuthoritativePlayerCollision();
        DrawWaterboxList();
    }

    if ((vtxDl.size() > vtxDlCapacity) || (mtxDl.size() > mtxDlCapacity)) {
        // If the sizes somehow changed between the two draws, we can't continue because we may be using invalid data
        WriteLog("Error drawing collision, vertex/matrix sizes didn't settle.");
        return;
    }

    OPEN_DISPS(gPlayState->state.gfxCtx);

    opaDl.push_back(gsSPEndDisplayList());
    gSPDisplayList(POLY_OPA_DISP++, opaDl.data());

    xluDl.push_back(gsSPEndDisplayList());
    gSPDisplayList(POLY_XLU_DISP++, xluDl.data());

    CLOSE_DISPS(gPlayState->state.gfxCtx);
}

void RecordAuthoritativePlayerCollision(
    int32_t playerId, const Game::Simulation::PlayerSnapshot& snapshot) {
    if (playerId >= 0 && snapshot.sceneId >= 0) {
        // Network snapshots can arrive more often than the native skeleton is
        // evaluated. Updating the whole record here used to erase the limb
        // origins captured by Player_PostLimbDraw on every packet, so F1 kept
        // falling back to the semantic server pose instead of following the
        // character's current animation. Only authoritative state is replaced;
        // the most recent rendered rig remains valid until the next draw
        // refreshes it or its short lifetime expires.
        PlayerCollisionRecord& record = playerId == localCollisionPlayerId
                                            ? localPlayerHitRig
                                            : playerHitRigs[playerId];
        record.snapshot = snapshot;
        record.receivedAt = std::chrono::steady_clock::now();
    }
}

extern "C" void SetLocalCollisionPlayerId(int32_t playerId) {
    if (localCollisionPlayerId != playerId) localPlayerHitRig = {};
    localCollisionPlayerId = playerId;
}

PlayerCollisionRecord* RenderedPlayerCollisionRecord(int32_t playerId) {
    if (playerId == localCollisionPlayerId) return &localPlayerHitRig;
    const auto found = playerHitRigs.find(playerId);
    return found == playerHitRigs.end() ? nullptr : &found->second;
}

extern "C" void BeginRenderedPlayerCollision(int32_t playerId) {
    PlayerCollisionRecord* record = RenderedPlayerCollisionRecord(playerId);
    if (record) record->renderedLimbs.fill(false);
}

extern "C" void RecordRenderedPlayerCollisionLimb(
    int32_t playerId, int32_t limbIndex, float x, float y, float z) {
    PlayerCollisionRecord* record = RenderedPlayerCollisionRecord(playerId);
    if (!record || limbIndex <= PLAYER_LIMB_NONE ||
        limbIndex >= PLAYER_LIMB_MAX) {
        return;
    }
    record->renderedLimbOrigins[limbIndex] = { x, y, z };
    record->renderedLimbs[limbIndex] = true;
}

extern "C" void EndRenderedPlayerCollision(int32_t playerId) {
    PlayerCollisionRecord* record = RenderedPlayerCollisionRecord(playerId);
    if (record) BuildRenderedPlayerHitRig(*record);
}

extern "C" void BeginLocalRenderedPlayerCollision(void) {
    localPlayerHitRig.snapshot.sceneId =
        gPlayState != nullptr ? gPlayState->sceneNum : -1;
    localPlayerHitRig.renderedLimbs.fill(false);
}

extern "C" void RecordLocalRenderedPlayerCollisionLimb(
    int32_t limbIndex, float x, float y, float z) {
    if (limbIndex <= PLAYER_LIMB_NONE || limbIndex >= PLAYER_LIMB_MAX) return;
    localPlayerHitRig.renderedLimbOrigins[limbIndex] = { x, y, z };
    localPlayerHitRig.renderedLimbs[limbIndex] = true;
}

extern "C" void EndLocalRenderedPlayerCollision(void) {
    BuildRenderedPlayerHitRig(localPlayerHitRig);
}

void ClearAuthoritativePlayerCollision(void) {
    playerHitRigs.clear();
    localPlayerHitRig = {};
}

bool ResolveRenderedBodyArrow(const Game::Simulation::ArrowBodyAttachment& attachment,
                              Game::Simulation::Vec3& position, Game::Simulation::Vec3& direction) {
    if (!gPlayState) return false;
    const PlayerCollisionRecord* record = nullptr;
    if (attachment.playerId == localCollisionPlayerId) {
        record = &localPlayerHitRig;
    } else {
        const auto found = playerHitRigs.find(attachment.playerId);
        if (found != playerHitRigs.end()) record = &found->second;
    }
    if (!record || record->snapshot.lifeEpoch != attachment.lifeEpoch ||
        record->snapshot.sceneId != gPlayState->sceneNum) return false;

    constexpr float renderedPoseLifetimeSeconds = 0.25f;
    const bool renderedPoseCurrent = record->hasRenderedRig &&
        std::chrono::duration<float>(std::chrono::steady_clock::now() -
                                     record->renderedAt).count() <=
            renderedPoseLifetimeSeconds;
    const auto rig = renderedPoseCurrent
        ? record->renderedRig
        : Game::Simulation::BuildAuthoritativePlayerHitRig(record->snapshot);
    return Game::Simulation::ResolveArrowOnBody(
        attachment, rig, record->snapshot.headingRadians, position, direction);
}

void RemoveAuthoritativePlayerCollision(int32_t playerId) {
    if (playerId == localCollisionPlayerId) localPlayerHitRig = {};
    playerHitRigs.erase(playerId);
}

extern "C" void ToggleColViewer() {
    sColViewerEnabled = !sColViewerEnabled;
}

extern "C" void InitColViewer() {
    CreateCylinderData();
    CreateSphereData();
    CreateHitPrismData();
}
