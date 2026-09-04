#pragma once

// OpenGL water-surface rendering entry points.

#ifdef __cplusplus
extern "C" {
#endif

void gfx_queue_water_drop(float u, float v, float radius, float strength);
void gfx_set_water_mask(int level, const unsigned char* data, int size);

#ifdef __cplusplus
}
#endif
