#ifndef FRAMEBUFFER_EFFECTS_H
#define FRAMEBUFFER_EFFECTS_H

#include <runtime/libultra.h>

extern int32_t gBlurFrameBuffer;
extern int32_t gReusableFrameBuffer;
extern int32_t gN64ResFrameBuffer;

void FB_CreateFramebuffers(void);
void FB_CopyToFramebuffer(Gfx** gfxp, int32_t fb_src, int32_t fb_dest, uint8_t oncePerFrame, uint8_t* hasCopied);
void FB_WriteFramebufferSliceToCPU(Gfx** gfxp, void* buffer, uint8_t byteSwap);
void FB_DrawFromFramebuffer(Gfx** gfxp, int32_t fb, uint8_t alpha);
void FB_DrawFromFramebufferScaled(Gfx** gfxp, int32_t fb, uint8_t alpha, float scaleX, float scaleY);

#endif // FRAMEBUFFER_EFFECTS_H
