#ifndef GS2DEX_H
#define GS2DEX_H

#ifdef _LANGUAGE_C_PLUS_PLUS
extern "C" {
#endif

/*===========================================================================*
 *	Macro
 *===========================================================================*/
#define GS_CALC_DXT(line) (((1 << G_TX_DXT_FRAC) - 1) / (line) + 1)
#define GS_PIX2TMEM(pix, siz) ((pix) >> (4 - (siz)))
#define GS_PIX2DXT(pix, siz) GS_CALC_DXT(GS_PIX2TMEM((pix), (siz)))

/*===========================================================================*
 *	Data structures for S2DEX microcode
 *===========================================================================*/

/*---------------------------------------------------------------------------*
 *	Background
 *---------------------------------------------------------------------------*/
#define G_BGLT_LOADBLOCK 0x0033
#define G_BGLT_LOADTILE 0xfff4

#define G_BG_FLAG_FLIPS 0x01
#define G_BG_FLAG_FLIPT 0x10

/* Non scalable background plane */
typedef struct {
    uint16_t imageX; /* x-coordinate of upper-left position of texture (u10.5) */
    uint16_t imageW; /* width of the texture (u10.2) */
    int16_t frameX; /* upper-left position of transferred frame (s10.2) */
    uint16_t frameW; /* width of transferred frame (u10.2) */

    uint16_t imageY; /* y-coordinate of upper-left position of texture (u10.5) */
    uint16_t imageH; /* height of the texture (u10.2) */
    int16_t frameY; /* upper-left position of transferred frame (s10.2) */
    uint16_t frameH; /* height of transferred frame (u10.2) */

    uint64_t* imagePtr; /* texture source address on DRAM */
    uint16_t imageLoad; /* which to use, LoadBlock or  LoadTile */
    uint8_t imageFmt;   /* format of texel - G_IM_FMT_*  */
    uint8_t imageSiz;   /* size of texel - G_IM_SIZ_*   */
    uint16_t imagePal;  /* pallet number  */
    uint16_t imageFlip; /* right & left image inversion (Inverted by G_BG_FLAG_FLIPS) */

    /* The following is set in the initialization routine guS2DInitBg(). There is no need for the user to set it. */
    uint16_t tmemW;      /* TMEM width and Word size of frame 1 line.
                       At LoadBlock, GS_PIX2TMEM(imageW/4,imageSiz)
                       At LoadTile  GS_PIX2TMEM(frameW/4,imageSiz)+1 */
    uint16_t tmemH;      /* height of TMEM loadable at a time (s13.2) 4 times value
                       When the normal texture, 512/tmemW*4
                       When the CI texture, 256/tmemW*4 */
    uint16_t tmemLoadSH; /* SH value
                       At LoadBlock, tmemSize/2-1
                       At LoadTile, tmemW*16-1 */
    uint16_t tmemLoadTH; /* TH value or Stride value
                       At LoadBlock, GS_CALC_DXT(tmemW)
                       At LoadTile, tmemH-1 */
    uint16_t tmemSizeW;  /* skip value of imagePtr for image 1-line
                       At LoadBlock, tmemW*2
                       At LoadTile, GS_PIX2TMEM(imageW/4,imageSiz)*2 */
    uint16_t tmemSize;   /* skip value of imagePtr for 1-loading
                       = tmemSizeW*tmemH                          */
} uObjBg_t;         /* 40 bytes */

/* Scalable background plane */
typedef struct {
    uint16_t imageX; /* x-coordinate of upper-left position of texture (u10.5) */
    uint16_t imageW; /* width of texture (u10.2) */
    int16_t frameX; /* upper-left position of transferred frame (s10.2) */
    uint16_t frameW; /* width of transferred frame (u10.2) */

    uint16_t imageY; /* y-coordinate of upper-left position of texture (u10.5) */
    uint16_t imageH; /* height of texture (u10.2) */
    int16_t frameY; /* upper-left position of transferred frame (s10.2) */
    uint16_t frameH; /* height of transferred frame (u10.2) */

    uint64_t* imagePtr; /* texture source address on DRAM */
    uint16_t imageLoad; /* Which to use, LoadBlock or LoadTile? */
    uint8_t imageFmt;   /* format of texel - G_IM_FMT_*  */
    uint8_t imageSiz;   /* size of texel - G_IM_SIZ_*  */
    uint16_t imagePal;  /* pallet number */
    uint16_t imageFlip; /* right & left image inversion (Inverted by G_BG_FLAG_FLIPS) */

    uint16_t scaleW;     /* scale value of X-direction (u5.10) */
    uint16_t scaleH;     /* scale value of Y-direction (u5.10) */
    int32_t imageYorig; /* start point of drawing on image (s20.5) */

    uint8_t padding[4];

} uObjScaleBg_t; /* 40 bytes */

typedef union {
    uObjBg_t b;
    uObjScaleBg_t s;
    long long int force_structure_alignment;
} uObjBg;

/*---------------------------------------------------------------------------*
 *	2D Objects
 *---------------------------------------------------------------------------*/
#define G_OBJ_FLAG_FLIPS 1 << 0 /* inversion to S-direction */
#define G_OBJ_FLAG_FLIPT 1 << 4 /* nversion to T-direction */

typedef struct {
    int16_t objX;        /* s10.2 OBJ x-coordinate of upper-left end */
    uint16_t scaleW;      /* u5.10 Scaling of u5.10 width direction   */
    uint16_t imageW;      /* u10.5 width of u10.5 texture (length of S-direction) */
    uint16_t paddingX;    /* Unused - Always 0 */
    int16_t objY;        /* s10.2 OBJ y-coordinate of s10.2 OBJ upper-left end */
    uint16_t scaleH;      /* u5.10 Scaling of u5.10 height direction */
    uint16_t imageH;      /* u10.5 height of u10.5 texture (length of T-direction) */
    uint16_t paddingY;    /* Unused - Always 0 */
    uint16_t imageStride; /* folding width of texel (In units of 64bit word) */
    uint16_t imageAdrs;   /* texture header position in TMEM (In units of 64bit word) */
    uint8_t imageFmt;     /* format of texel - G_IM_FMT_* */
    uint8_t imageSiz;     /* size of texel - G_IM_SIZ_* */
    uint8_t imagePal;     /* pallet number (0-7) */
    uint8_t imageFlags;   /* The display flag - G_OBJ_FLAG_FLIP* */
} uObjSprite_t;      /* 24 bytes */

typedef union {
    uObjSprite_t s;
    long long int force_structure_alignment;
} uObjSprite;

/*---------------------------------------------------------------------------*
 *	2D Matrix
 *---------------------------------------------------------------------------*/
typedef struct {
    int32_t A, B, C, D; /* s15.16 */
    int16_t X, Y;       /* s10.2  */
    uint16_t BaseScaleX; /* u5.10  */
    uint16_t BaseScaleY; /* u5.10  */
} uObjMtx_t;        /* 24 bytes */

typedef union {
    uObjMtx_t m;
    long long int force_structure_alignment;
} uObjMtx;

typedef struct {
    int16_t X, Y;       /* s10.2  */
    uint16_t BaseScaleX; /* u5.10  */
    uint16_t BaseScaleY; /* u5.10  */
} uObjSubMtx_t;     /* 8 bytes */

typedef union {
    uObjSubMtx_t m;
    long long int force_structure_alignment;
} uObjSubMtx;

/*---------------------------------------------------------------------------*
 *	Loading into TMEM
 *---------------------------------------------------------------------------*/
#define G_OBJLT_TXTRBLOCK 0x00001033
#define G_OBJLT_TXTRTILE 0x00fc1034
#define G_OBJLT_TLUT 0x00000030

#define GS_TB_TSIZE(pix, siz) (GS_PIX2TMEM((pix), (siz)) - 1)
#define GS_TB_TLINE(pix, siz) (GS_CALC_DXT(GS_PIX2TMEM((pix), (siz))))

typedef struct {
    uint32_t type;      /* G_OBJLT_TXTRBLOCK divided into types */
    uint64_t* image;    /* texture source address on DRAM */
    uint16_t tmem;      /* loaded TMEM word address (8byteWORD) */
    uint16_t tsize;     /* Texture size, Specified by macro GS_TB_TSIZE() */
    uint16_t tline;     /* width of Texture 1-line, Specified by macro GS_TB_TLINE() */
    uint16_t sid;       /* STATE ID Multipled by 4 (Either one of  0, 4, 8 and 12) */
    uint32_t flag;      /* STATE flag  */
    uint32_t mask;      /* STATE mask  */
} uObjTxtrBlock_t; /* 24 bytes */

#define GS_TT_TWIDTH(pix, siz) ((GS_PIX2TMEM((pix), (siz)) << 2) - 1)
#define GS_TT_THEIGHT(pix, siz) (((pix) << 2) - 1)

typedef struct {
    uint32_t type;     /* G_OBJLT_TXTRTILE divided into types */
    uint64_t* image;   /* texture source address on DRAM */
    uint16_t tmem;     /* loaded TMEM word address (8byteWORD)*/
    uint16_t twidth;   /* width of Texture (Specified by macro GS_TT_TWIDTH()) */
    uint16_t theight;  /* height of Texture (Specified by macro GS_TT_THEIGHT()) */
    uint16_t sid;      /* STATE ID Multipled by 4 (Either one of  0, 4, 8 and 12) */
    uint32_t flag;     /* STATE flag  */
    uint32_t mask;     /* STATE mask  */
} uObjTxtrTile_t; /* 24 bytes */

#define GS_PAL_HEAD(head) ((head) + 256)
#define GS_PAL_NUM(num) ((num)-1)

typedef struct {
    uint32_t type;     /* G_OBJLT_TLUT divided into types */
    uint64_t* image;   /* texture source address on DRAM */
    uint16_t phead;    /* pallet number of load header (Between 256 and 511) */
    uint16_t pnum;     /* loading pallet number -1 */
    uint16_t zero;     /* Assign 0 all the time */
    uint16_t sid;      /* STATE ID Multipled by 4 (Either one of  0, 4, 8 and 12)*/
    uint32_t flag;     /* STATE flag  */
    uint32_t mask;     /* STATE mask  */
} uObjTxtrTLUT_t; /* 24 bytes */

typedef union {
    uObjTxtrBlock_t block;
    uObjTxtrTile_t tile;
    uObjTxtrTLUT_t tlut;
    long long int force_structure_alignment;
} uObjTxtr;

/*---------------------------------------------------------------------------*
 *	Loading into TMEM & 2D Objects
 *---------------------------------------------------------------------------*/
typedef struct {
    uObjTxtr txtr;
    uObjSprite sprite;
} uObjTxSprite; /* 48 bytes */

/*===========================================================================*
 *	GBI Commands for S2DEX microcode
 *===========================================================================*/
/* GBI Header */
#ifdef F3DEX_GBI_2
#define G_OBJ_RECTANGLE_R 0xda
#define G_OBJ_MOVEMEM 0xdc
#define G_RDPHALF_0 0xe4
#define G_OBJ_RECTANGLE 0x01
#define G_OBJ_SPRITE 0x02
#define G_SELECT_DL 0x04
#define G_OBJ_LOADTXTR 0x05
#define G_OBJ_LDTX_SPRITE 0x06
#define G_OBJ_LDTX_RECT 0x07
#define G_OBJ_LDTX_RECT_R 0x08
#define G_BG_1CYC 0x09
#define G_BG_COPY 0x0a
#define G_OBJ_RENDERMODE 0x0b
#else
#define G_BG_1CYC 0x01
#define G_BG_COPY 0x02
#define G_OBJ_RECTANGLE 0x03
#define G_OBJ_SPRITE 0x04
#define G_OBJ_MOVEMEM 0x05
#define G_SELECT_DL 0xb0
#define G_OBJ_RENDERMODE 0xb1
#define G_OBJ_RECTANGLE_R 0xb2
#define G_OBJ_LOADTXTR 0xc1
#define G_OBJ_LDTX_SPRITE 0xc2
#define G_OBJ_LDTX_RECT 0xc3
#define G_OBJ_LDTX_RECT_R 0xc4
#define G_RDPHALF_0 0xe4
#endif

/*---------------------------------------------------------------------------*
 *	Background wrapped screen
 *---------------------------------------------------------------------------*/
#define gSPBgRectangle(pkt, m, mptr) gDma0p((pkt), (m), (mptr), 0)
#define gsSPBgRectangle(m, mptr) gsDma0p((m), (mptr), 0)
#define gSPBgRectCopy(pkt, mptr) gSPBgRectangle((pkt), G_BG_COPY, (mptr))
#define gsSPBgRectCopy(mptr) gsSPBgRectangle(G_BG_COPY, (mptr))
#define gSPBgRect1Cyc(pkt, mptr) gSPBgRectangle((pkt), G_BG_1CYC, (mptr))
#define gsSPBgRect1Cyc(mptr) gsSPBgRectangle(G_BG_1CYC, (mptr))

/*---------------------------------------------------------------------------*
 *	2D Objects
 *---------------------------------------------------------------------------*/
#define gSPObjSprite(pkt, mptr) gDma0p((pkt), G_OBJ_SPRITE, (mptr), 0)
#define gsSPObjSprite(mptr) gsDma0p(G_OBJ_SPRITE, (mptr), 0)
#define gSPObjRectangle(pkt, mptr) gDma0p((pkt), G_OBJ_RECTANGLE, (mptr), 0)
#define gsSPObjRectangle(mptr) gsDma0p(G_OBJ_RECTANGLE, (mptr), 0)
#define gSPObjRectangleR(pkt, mptr) gDma0p((pkt), G_OBJ_RECTANGLE_R, (mptr), 0)
#define gsSPObjRectangleR(mptr) gsDma0p(G_OBJ_RECTANGLE_R, (mptr), 0)

/*---------------------------------------------------------------------------*
 *	2D Matrix
 *---------------------------------------------------------------------------*/
#define gSPObjMatrix(pkt, mptr) gDma1p((pkt), G_OBJ_MOVEMEM, (mptr), 0, 23)
#define gsSPObjMatrix(mptr) gsDma1p(G_OBJ_MOVEMEM, (mptr), 0, 23)
#define gSPObjSubMatrix(pkt, mptr) gDma1p((pkt), G_OBJ_MOVEMEM, (mptr), 2, 7)
#define gsSPObjSubMatrix(mptr) gsDma1p(G_OBJ_MOVEMEM, (mptr), 2, 7)

/*---------------------------------------------------------------------------*
 *	Loading into TMEM
 *---------------------------------------------------------------------------*/
#define gSPObjLoadTxtr(pkt, tptr) gDma0p((pkt), G_OBJ_LOADTXTR, (tptr), 23)
#define gsSPObjLoadTxtr(tptr) gsDma0p(G_OBJ_LOADTXTR, (tptr), 23)
#define gSPObjLoadTxSprite(pkt, tptr) gDma0p((pkt), G_OBJ_LDTX_SPRITE, (tptr), 47)
#define gsSPObjLoadTxSprite(tptr) gsDma0p(G_OBJ_LDTX_SPRITE, (tptr), 47)
#define gSPObjLoadTxRect(pkt, tptr) gDma0p((pkt), G_OBJ_LDTX_RECT, (tptr), 47)
#define gsSPObjLoadTxRect(tptr) gsDma0p(G_OBJ_LDTX_RECT, (tptr), 47)
#define gSPObjLoadTxRectR(pkt, tptr) gDma0p((pkt), G_OBJ_LDTX_RECT_R, (tptr), 47)
#define gsSPObjLoadTxRectR(tptr) gsDma0p(G_OBJ_LDTX_RECT_R, (tptr), 47)

/*---------------------------------------------------------------------------*
 *	Select Display List
 *---------------------------------------------------------------------------*/
#define gSPSelectDL(pkt, mptr, sid, flag, mask)                           \
    {                                                                     \
        gDma1p((pkt), G_RDPHALF_0, (flag), (uint32_t)(mptr)&0xffff, (sid));    \
        gDma1p((pkt), G_SELECT_DL, (mask), (uint32_t)(mptr) >> 16, G_DL_PUSH); \
    }
#define gsSPSelectDL(mptr, sid, flag, mask)                         \
    {                                                               \
        gsDma1p(G_RDPHALF_0, (flag), (uint32_t)(mptr)&0xffff, (sid));    \
        gsDma1p(G_SELECT_DL, (mask), (uint32_t)(mptr) >> 16, G_DL_PUSH); \
    }
#define gSPSelectBranchDL(pkt, mptr, sid, flag, mask)                       \
    {                                                                       \
        gDma1p((pkt), G_RDPHALF_0, (flag), (uint32_t)(mptr)&0xffff, (sid));      \
        gDma1p((pkt), G_SELECT_DL, (mask), (uint32_t)(mptr) >> 16, G_DL_NOPUSH); \
    }
#define gsSPSelectBranchDL(mptr, sid, flag, mask)                     \
    {                                                                 \
        gsDma1p(G_RDPHALF_0, (flag), (uint32_t)(mptr)&0xffff, (sid));      \
        gsDma1p(G_SELECT_DL, (mask), (uint32_t)(mptr) >> 16, G_DL_NOPUSH); \
    }

/*---------------------------------------------------------------------------*
 *	Set general status
 *---------------------------------------------------------------------------*/
#define G_MW_GENSTAT 0x08 /* Note that it is the same value of G_MW_FOG */

#define gSPSetStatus(pkt, sid, val) gMoveWd((pkt), G_MW_GENSTAT, (sid), (val))
#define gsSPSetStatus(sid, val) gsMoveWd(G_MW_GENSTAT, (sid), (val))

/*---------------------------------------------------------------------------*
 *	Set Object Render Mode
 *---------------------------------------------------------------------------*/
#define G_OBJRM_NOTXCLAMP 0x01
#define G_OBJRM_XLU 0x02       /* Ignored */
#define G_OBJRM_ANTIALIAS 0x04 /* Ignored */
#define G_OBJRM_BILERP 0x08
#define G_OBJRM_SHRINKSIZE_1 0x10
#define G_OBJRM_SHRINKSIZE_2 0x20
#define G_OBJRM_WIDEN 0x40

#define gSPObjRenderMode(pkt, mode) gImmp1((pkt), G_OBJ_RENDERMODE, (mode))
#define gsSPObjRenderMode(mode) gsImmp1(G_OBJ_RENDERMODE, (mode))

/*===========================================================================*
 *	Render Mode Macro
 *===========================================================================*/
#define RM_RA_SPRITE(clk)                                                        \
    AA_EN | CVG_DST_CLAMP | CVG_X_ALPHA | ALPHA_CVG_SEL | ZMODE_OPA | TEX_EDGE | \
        GBL_c##clk(G_BL_CLR_IN, G_BL_A_IN, G_BL_CLR_MEM, G_BL_1MA)

#define G_RM_SPRITE G_RM_OPA_SURF
#define G_RM_SPRITE2 G_RM_OPA_SURF2
#define G_RM_RA_SPRITE RM_RA_SPRITE(1)
#define G_RM_RA_SPRITE2 RM_RA_SPRITE(2)
#define G_RM_AA_SPRITE G_RM_AA_TEX_TERR
#define G_RM_AA_SPRITE2 G_RM_AA_TEX_TERR2
#define G_RM_XLU_SPRITE G_RM_XLU_SURF
#define G_RM_XLU_SPRITE2 G_RM_XLU_SURF2
#define G_RM_AA_XLU_SPRITE G_RM_AA_XLU_SURF
#define G_RM_AA_XLU_SPRITE2 G_RM_AA_XLU_SURF2

/*===========================================================================*
 *	External functions
 *===========================================================================*/
extern uint64_t gspS2DEX_fifoTextStart[], gspS2DEX_fifoTextEnd[];
extern uint64_t gspS2DEX_fifoDataStart[], gspS2DEX_fifoDataEnd[];
extern uint64_t gspS2DEX_fifo_dTextStart[], gspS2DEX_fifo_dTextEnd[];
extern uint64_t gspS2DEX_fifo_dDataStart[], gspS2DEX_fifo_dDataEnd[];
extern uint64_t gspS2DEX2_fifoTextStart[], gspS2DEX2_fifoTextEnd[];
extern uint64_t gspS2DEX2_fifoDataStart[], gspS2DEX2_fifoDataEnd[];
extern uint64_t gspS2DEX2_xbusTextStart[], gspS2DEX2_xbusTextEnd[];
extern uint64_t gspS2DEX2_xbusDataStart[], gspS2DEX2_xbusDataEnd[];
extern void guS2DInitBg(uObjBg*);

#ifdef F3DEX_GBI_2
#define guS2DEmuBgRect1Cyc guS2D2EmuBgRect1Cyc /*Wrapper*/
#define guS2DEmuSetScissor guS2D2EmuSetScissor /*Wrapper*/
extern void guS2D2EmuSetScissor(uint32_t, uint32_t, uint32_t, uint32_t, uint8_t);
extern void guS2D2EmuBgRect1Cyc(Gfx**, uObjBg*);
#else
extern void guS2DEmuSetScissor(uint32_t, uint32_t, uint32_t, uint32_t, uint8_t);
extern void guS2DEmuBgRect1Cyc(Gfx**, uObjBg*);
#endif

#ifdef _LANGUAGE_C_PLUS_PLUS
}
#endif
#endif /* GS2DEX_H */

/*======== End of gs2dex.h ========*/
