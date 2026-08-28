#pragma once

#ifndef GLOBAL_H
#define GLOBAL_H

#include "math.h"

#include "functions.h"
#include "variables.h"
#include "macros.h"
#include "port/cvar_prefixes.h"
#include <runtime/bridge.h>

#define _AudioseqSegmentRomStart "Audioseq"
#define _AudiobankSegmentRomStart "Audiobank"
#define _AudiotableSegmentRomStart "Audiotable"

#define _icon_item_staticSegmentRomStart 0
#define _icon_item_staticSegmentRomEnd 0
#define _map_i_staticSegmentRomStart 0
#define _map_i_staticSegmentRomEnd 0
#define _nintendo_rogo_staticSegmentRomStart 0
#define _nintendo_rogo_staticSegmentRomEnd 0
#define _dmadataSegmentStart 0
#define _dmadataSegmentEnd 0
#define _parameter_staticSegmentRomStart 0
#define _parameter_staticSegmentRomEnd 0
#define _map_name_staticSegmentRomStart 0
#define _map_name_staticSegmentRomEnd 0
#define _title_staticSegmentRomStart 0
#define _title_staticSegmentRomEnd 0
#define _z_select_staticSegmentRomStart 0
#define _z_select_staticSegmentRomEnd 0

// TODO: POSIX/BSD Bug, this is a hack to fix the build compilation on any BSD system (Switch, Wii-U, Vita, etc)
// <sys/types.h> defines quad as a macro, which conflicts with the quad parameter on z_collision_check.c
#undef quad

#ifdef __cplusplus
extern "C" {
#endif
extern PlayState* gPlayState;
#ifdef __cplusplus
}
#endif

#endif
