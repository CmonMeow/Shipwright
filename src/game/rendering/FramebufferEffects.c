#include "rendering/FramebufferEffects.h"
#include "global.h"
#include "platform/client/RetainedGameBridge.h"

int gfx_create_framebuffer(uint32_t width, uint32_t height, uint32_t native_width, uint32_t native_height,
                           uint8_t resize);

int32_t gBlurFrameBuffer = -1;
// Scratch framebuffer used by effects that copy and draw within one frame.
int32_t gReusableFrameBuffer = -1;

// N64 resolution sized buffer (320x240), used by picto box and deku bubble
int32_t gN64ResFrameBuffer = -1;

void FB_CreateFramebuffers(void) {
    if (gBlurFrameBuffer == -1) {
        gBlurFrameBuffer = gfx_create_framebuffer(SCREEN_WIDTH, SCREEN_HEIGHT, SCREEN_WIDTH, SCREEN_HEIGHT, true);
    }

    if (gReusableFrameBuffer == -1) {
        gReusableFrameBuffer = gfx_create_framebuffer(SCREEN_WIDTH, SCREEN_HEIGHT, SCREEN_WIDTH, SCREEN_HEIGHT, true);
    }

    if (gN64ResFrameBuffer == -1) {
        gN64ResFrameBuffer = gfx_create_framebuffer(SCREEN_WIDTH, SCREEN_HEIGHT, SCREEN_WIDTH, SCREEN_HEIGHT, false);
    }
}

/**
 * Copies the current texture data from the source frame buffer to the destination frame buffer
 * Setting oncePerFrame ensures that the copy will only happen once every game frame. This
 * is important for effects that could be affected by increased frame interpolation (like motion blur).
 * A pointer to a boolean is passed to the render for the render to set once the copy has been performed.
 * This function uses opcodes from f3dex2 but may be called when s2dex is loaded, such as during shrink window. Make
 * sure f3dex2 is loaded before this function is called.
 */
void FB_CopyToFramebuffer(Gfx** gfxp, int32_t fb_src, int32_t fb_dest, uint8_t oncePerFrame, uint8_t* hasCopied) {
    Gfx* gfx = *gfxp;

    gSPMatrix(gfx++, &gMtxClear, G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);

    gDPSetOtherMode(gfx++,
                    G_AD_DISABLE | G_CD_DISABLE | G_CK_NONE | G_TC_FILT | G_TF_POINT | G_TT_NONE | G_TL_TILE |
                        G_TD_CLAMP | G_TP_NONE | G_CYC_1CYCLE | G_PM_NPRIMITIVE,
                    G_AC_NONE | G_ZS_PRIM | G_RM_OPA_SURF | G_RM_OPA_SURF2);

    gSPClearGeometryMode(gfx++, G_FOG | G_LIGHTING | G_TEXTURE_GEN | G_TEXTURE_GEN_LINEAR);
    gSPSetGeometryMode(gfx++, G_ZBUFFER | G_SHADE | G_SHADING_SMOOTH);

    gDPSetBlendColor(gfx++, 255, 255, 255, 8);
    gDPSetPrimDepth(gfx++, 0xFFFF, 0xFFFF);

    gDPSetEnvColor(gfx++, 255, 255, 255, 255);
    gDPSetCombineLERP(gfx++, TEXEL0, 0, ENVIRONMENT, 0, 0, 0, 0, ENVIRONMENT, TEXEL0, 0, ENVIRONMENT, 0, 0, 0, 0,
                      ENVIRONMENT);

    gDPSetScissor(gfx++, G_SC_NON_INTERLACE, 0, 0, SCREEN_WIDTH, SCREEN_HEIGHT);

    gDPCopyFB(gfx++, fb_dest, fb_src, oncePerFrame, hasCopied);

    *gfxp = gfx;
}

/**
 * Copies a 4:3 slice of the current framebuffer scaled down to 320x240 to a CPU buffer address.
 * The buffer output will be in RGBA16 format.
 * Specify the byteswap flag to force the buffer data to be written as BigEndian, which is
 * required if the buffer is being used as a texture in F3D.
 */
void FB_WriteFramebufferSliceToCPU(Gfx** gfxp, void* buffer, uint8_t byteSwap) {
    Gfx* gfx = *gfxp;

    FB_CopyToFramebuffer(&gfx, 0, gReusableFrameBuffer, false, NULL);

    // Set the N64 resolution framebuffer as the draw target (320x240)
    gsSPSetFB(gfx++, gN64ResFrameBuffer);
    // Reset scissor for new framebuffer
    gDPSetScissor(gfx++, G_SC_NON_INTERLACE, 0, 0, SCREEN_WIDTH, SCREEN_HEIGHT);

    int16_t s0 = 0, t0 = 0;
    int16_t s1 = RetainedGame_GetRenderWidth();
    int16_t t1 = RetainedGame_GetRenderHeight();

    float aspectRatio = RetainedGame_GetAspectRatio();
    float fourByThree = 4.0f / 3.0f;

    // Adjust the texture coordinates so that only a 4:3 region from the center is drawn
    // to the N64 resolution buffer. Currently ratios smaller than 4:3 will just stretch to fill.
    if (aspectRatio > fourByThree) {
        int16_t adjustedWidth = RetainedGame_GetRenderWidth() / (aspectRatio / fourByThree);
        s0 = (RetainedGame_GetRenderWidth() - adjustedWidth) / 2;
        s1 -= s0;
    }

    gDPSetTextureImageFB(gfx++, 0, 0, 0, gReusableFrameBuffer);
    gDPImageRectangle(gfx++, 0 << 2, 0 << 2, s0, t0, SCREEN_WIDTH << 2, SCREEN_HEIGHT << 2, s1, t1, G_TX_RENDERTILE,
                      RetainedGame_GetRenderWidth(), RetainedGame_GetRenderHeight());

    // Read the final N64 framebuffer back as rgba16 into the CPU-side buffer
    gDPReadFB(gfx++, gN64ResFrameBuffer, buffer, 0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, byteSwap);

    gsSPResetFB(gfx++);
    // Reset scissor for original framebuffer
    gDPSetScissor(gfx++, G_SC_NON_INTERLACE, 0, 0, SCREEN_WIDTH, SCREEN_HEIGHT);

    *gfxp = gfx;
}

/**
 * Draws the texture data from the specified frame buffer as a full screen image
 */
void FB_DrawFromFramebuffer(Gfx** gfxp, int32_t fb, uint8_t alpha) {
    Gfx* gfx = *gfxp;

    gSPMatrix(gfx++, &gMtxClear, G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);

    gDPSetEnvColor(gfx++, 255, 255, 255, alpha);

    gDPSetOtherMode(gfx++,
                    G_AD_NOISE | G_CD_NOISE | G_CK_NONE | G_TC_FILT | G_TF_POINT | G_TT_NONE | G_TL_TILE | G_TD_CLAMP |
                        G_TP_NONE | G_CYC_1CYCLE | G_PM_NPRIMITIVE,
                    G_AC_NONE | G_ZS_PRIM | G_RM_CLD_SURF | G_RM_CLD_SURF2);

    gSPClearGeometryMode(gfx++, G_CULL_BOTH | G_FOG | G_LIGHTING | G_TEXTURE_GEN | G_TEXTURE_GEN_LINEAR);
    gSPSetGeometryMode(gfx++, G_ZBUFFER | G_SHADE | G_SHADING_SMOOTH);

    gDPSetCombineLERP(gfx++, TEXEL0, 0, ENVIRONMENT, 0, 0, 0, 0, ENVIRONMENT, TEXEL0, 0, ENVIRONMENT, 0, 0, 0, 0,
                      ENVIRONMENT);

    gDPSetScissor(gfx++, G_SC_NON_INTERLACE, 0, 0, SCREEN_WIDTH, SCREEN_HEIGHT);

    gDPSetTextureImageFB(gfx++, 0, 0, 0, fb);
    gDPImageRectangle(gfx++, RetainedGame_GetRectDimensionFromLeftEdge(0) << 2, 0 << 2, 0, 0,
                      RetainedGame_GetRectDimensionFromRightEdge(SCREEN_WIDTH) << 2, SCREEN_HEIGHT << 2,
                      RetainedGame_GetRenderWidth(), RetainedGame_GetRenderHeight(), G_TX_RENDERTILE,
                      RetainedGame_GetRenderWidth(), RetainedGame_GetRenderHeight());

    *gfxp = gfx;
}

/**
 * Similar to FB_DrawFromFramebuffer, but scales the image relative to the center of the screen.
 * This function uses opcodes from f3dex2 but may be called when s2dex is loaded, such as during shrink window. Make
 * sure f3dex2 is loaded before this function is called.
 */
void FB_DrawFromFramebufferScaled(Gfx** gfxp, int32_t fb, uint8_t alpha, float scaleX, float scaleY) {
    Gfx* gfx = *gfxp;

    gDPSetEnvColor(gfx++, 255, 255, 255, alpha);

    gDPSetOtherMode(gfx++,
                    G_AD_NOISE | G_CD_NOISE | G_CK_NONE | G_TC_FILT | G_TF_POINT | G_TT_NONE | G_TL_TILE | G_TD_CLAMP |
                        G_TP_NONE | G_CYC_1CYCLE | G_PM_NPRIMITIVE,
                    G_AC_NONE | G_ZS_PRIM | G_RM_CLD_SURF | G_RM_CLD_SURF2);

    gDPSetCombineLERP(gfx++, TEXEL0, 0, ENVIRONMENT, 0, 0, 0, 0, ENVIRONMENT, TEXEL0, 0, ENVIRONMENT, 0, 0, 0, 0,
                      ENVIRONMENT);

    gDPSetScissor(gfx++, G_SC_NON_INTERLACE, 0, 0, SCREEN_WIDTH, SCREEN_HEIGHT);

    gDPSetTextureImageFB(gfx++, 0, 0, 0, fb);

    float x0 = gScreenWidth * 0.5f * scaleX;
    float y0 = gScreenHeight * 0.5f * scaleY;

    gDPImageRectangle(gfx++, RetainedGame_GetRectDimensionFromLeftEdge(x0) << 2, (int)(y0) << 2, 0, 0,
                      RetainedGame_GetRectDimensionFromRightEdge((float)(gScreenWidth - x0)) << 2,
                      (int)((float)(gScreenHeight - y0)) << 2, RetainedGame_GetRenderWidth(),
                      RetainedGame_GetRenderHeight(), G_TX_RENDERTILE, RetainedGame_GetRenderWidth(),
                      RetainedGame_GetRenderHeight());

    *gfxp = gfx;
}
