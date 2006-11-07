/*
 * utils.c: A plugin for the Video Disk Recorder
 *
 * See the README file for copyright information and how to reach the author.
 *
 * $Id: utils.c,v 1.21 2006/11/07 18:13:19 wachm Exp $
 */

// --- plain C MMX functions (i'm too lazy to put this in a class)


/*
 * MMX conversion functions taken from the mpeg2dec project
 * Copyright (C) 2000-2002 Silicon Integrated System Corp.
 * All Rights Reserved.
 *
 * Author: Olie Lho <ollie@sis.com.tw>
*/
#include <unistd.h>
#include <stdlib.h>
#include <fcntl.h>
#include <string.h>

#include "utils.h"
#include "setup-softdevice.h"

#ifdef HAVE_CONFIG
# include "config.h"
#else
# define  HAVE_BROKEN_GCC_CPP  0
#endif

/* ---------------------------------------------------------------------------
 * chroma: field based
 * lang: C
 */
void yv12_to_yuy2_il_c(const uint8_t *py,
                       const uint8_t *pu, const uint8_t *pv,
                       uint8_t *dst, int width, int height,
                       int lumStride, int chromStride, int dstStride)
{
    const unsigned  chromWidth = width >> 1;
    uint32_t        *idst1, *idst2;
    const uint8_t   *yc1, *yc2, *uc, *vc;

  for(int y=0; y<height/4; y++)
  {
    /* -----------------------------------------------------------------------
     * take chroma line x (it's from field A) for packing with
     * luma lines x * 2 and x * 2 + 2
     */
    idst1 = (uint32_t *) (dst + 0 * dstStride);
    idst2 = (uint32_t *) (dst + 2 * dstStride);
    yc1 = py;
    yc2 = py + 2 * lumStride;
    uc = pu;
    vc = pv;

    for(unsigned i = 0; i < chromWidth; i++)
    {
      *idst1++ = (yc1[0] << 0)+ (uc[0] << 8) + (yc1[1] << 16) + (vc[0] << 24);
      *idst2++ = (yc2[0] << 0)+ (uc[0] << 8) + (yc2[1] << 16) + (vc[0] << 24);
      yc1 += 2;
      yc2 += 2;
      uc++;
      vc++;
    }

    /* -----------------------------------------------------------------------
     * take chroma line x+1 (it's from field B) for packing with
     * luma lines x * 2 + 1 and x * 2 + 3
     */
    py += lumStride;
    pu += chromStride;
    pv += chromStride;

    yc1 = py;
    yc2 = py + 2 * lumStride;
    uc = pu;
    vc = pv;
    idst1 = (uint32_t *) (dst + 1 * dstStride);
    idst2 = (uint32_t *) (dst + 3 * dstStride);

    for(unsigned i = 0; i < chromWidth; i++)
    {
      *idst1++ = (yc1[0] << 0)+ (uc[0] << 8) + (yc1[1] << 16) + (vc[0] << 24);
      *idst2++ = (yc2[0] << 0)+ (uc[0] << 8) + (yc2[1] << 16) + (vc[0] << 24);
      yc1 += 2;
      yc2 += 2;
      uc++;
      vc++;
    }

    py += 3*lumStride;
    pu += chromStride;
    pv += chromStride;

    dst  += 4 * dstStride;
  }
}

/* ---------------------------------------------------------------------------
 * convert to lines luma and one line chroma
 * lang: MMX2
 */
void
yv12_to_yuy2_il_mmx2_line (uint8_t *dest1, uint8_t *dest2, 
                           const int chromaWidth,
                           const uint8_t *yc1, const uint8_t *yc2,
                           const uint8_t *uc, const uint8_t *vc)
{
  int i=chromaWidth;
  
#ifdef USE_MMX 
  for(i = chromaWidth/4; i--; )
  {
    movq_m2r(*(yc1), mm1);     // mm1 = y7 y6 y5 y4 y3 y2 y1 y0
    movq_m2r(*(yc2), mm2);     // mm2 = y7 y6 y5 y4 y3 y2 y1 y0
    movq_r2r(mm1,mm5);                      // mm5 = y7 y6 y5 y4 y3 y2 y1 y0
    movd_m2r(*uc, mm3);                     // mm3 = 00 00 00 00 u3 u2 u1 u0
    movq_r2r(mm2,mm6);                      // mm5 = y7 y6 y5 y4 y3 y2 y1 y0
    movd_m2r(*vc, mm4);                     // mm3 = 00 00 00 00 v3 v2 v1 v0
    punpcklbw_r2r(mm4, mm3);                // mm3 = v3 u3 v2 u2 v1 u1 v0 u0
    punpcklbw_r2r(mm3, mm1);                // mm1 = V1 Y3 U1 Y2 V0 Y1 U0 Y0

    movntq(mm1,*(dest1 + 0));
    punpckhbw_r2r(mm3, mm5);                // mm5 = V3 Y7 U3 Y6 V2 Y5 U2 Y4
    punpcklbw_r2r(mm3, mm2);                // mm2 = V1 Y3 U1 Y2 V0 Y1 U0 Y0
    movntq(mm5,*(dest1 + 8));
    punpckhbw_r2r(mm3, mm6);                // mm6 = V3 Y7 U3 Y6 V2 Y5 U2 Y4

    movntq(mm2,*(dest2    ));
    movntq(mm6,*(dest2 + 8));

    yc1 += 8;
    yc2 += 8;
    uc += 4;
    vc += 4;
    dest1 += 16;
    dest2 += 16;
  }
#elif USE_ALTIVEC

/* This altivec optimization is based on vlc's i420_yuy2.c with the copyright notice:
 * 
 * Copyright (C) 2000, 2001 the VideoLAN team
 *
 * Authors: Samuel Hocevar <sam@zoy.org>
 */
  
   vector unsigned char u_vec;
   vector unsigned char v_vec; 
   vector unsigned char uv_vec;
   vector unsigned char y_vec;

   for ( i=chromaWidth; i>=16; i-=16) {
        u_vec = vec_ld(0,uc); uc+=16;
        v_vec = vec_ld(0,vc); vc+=16;
        uv_vec = vec_mergeh( u_vec, v_vec);
        y_vec = vec_ld( 0, yc1); yc1+=16;
        vec_st( vec_mergeh( y_vec, uv_vec), 0, dest1); dest1+=16;
        vec_st( vec_mergel( y_vec, uv_vec), 0, dest1); dest1+=16;
        y_vec = vec_ld( 0, yc2); yc2+=16;
        vec_st( vec_mergeh( y_vec, uv_vec), 0, dest2); dest2+=16;        
        vec_st( vec_mergel( y_vec, uv_vec), 0, dest2); dest2+=16;             
        
        uv_vec = vec_mergel( u_vec, v_vec);
        y_vec = vec_ld( 0, yc1); yc1+=16;
        vec_st( vec_mergeh( y_vec, uv_vec), 0, dest1); dest1+=16;
        vec_st( vec_mergel( y_vec, uv_vec), 0, dest1); dest1+=16;
        y_vec = vec_ld( 0, yc2); yc2+=16;
        vec_st( vec_mergeh( y_vec, uv_vec), 0, dest2); dest2+=16;        
        vec_st( vec_mergel( y_vec, uv_vec), 0, dest2); dest2+=16;        
   }
#endif
   for ( ; i>=1; i-=1 ) {
      *((uint32_t *)dest1) = (yc1[0] << 24)+ (uc[0] << 16) + (yc1[1] << 8) + (vc[0] << 0);
      *((uint32_t *)dest2) = (yc2[0] << 24)+ (uc[0] << 16) + (yc2[1] << 8) + (vc[0] << 0);
      //*idst++ = (yc[0] << 0)+ (uc[0] << 8) + (yc[1] << 16) + (vc[0] << 24);
      dest1+=4;
      dest2+=4;
      yc1 += 2;
      yc2 += 2;
      uc++;
      vc++;
    }
}

/* ---------------------------------------------------------------------------
 * chroma: field based
 * lang: MMX2
 */
void yv12_to_yuy2_il_mmx2(const uint8_t *py,
                          const uint8_t *pu, const uint8_t *pv,
                          uint8_t *dst, const int width, const int height,
                          const int lumStride, const int chromStride, 
                          const int dstStride)
{
  for(int y=0; y<height/4; y++)
  {
    /* -----------------------------------------------------------------------
     * take chroma line x (it's from field A) for packing with
     * luma lines y * 2 and y * 2 + 2
     */
    yv12_to_yuy2_il_mmx2_line (dst,
                               dst + dstStride * 2, width >> 1,
                               py, py + lumStride *2,
                               pu, pv);
    /* -----------------------------------------------------------------------
     * take chroma line x+1 (it's from field B) for packing with
     * luma lines y * 2 + 1 and y * 2 + 3
     */
    yv12_to_yuy2_il_mmx2_line (dst + dstStride,
                               dst + dstStride * 3, width >> 1,
                               py + lumStride, py + lumStride * 3,
                               pu + chromStride, pv + chromStride);
    py  += 4*lumStride;
    pu  += 2*chromStride;
    pv  += 2*chromStride;
    dst += 4*dstStride;
  }
  SFENCE;
  EMMS;
}

/* ---------------------------------------------------------------------------
 * chroma: frame based
 * lang: C
 */
void yv12_to_yuy2_fr_c(const uint8_t *ysrc,
                       const uint8_t *usrc, const uint8_t *vsrc,
                       uint8_t *dst, int width, int height,
                       int lumStride, int chromStride, int dstStride)
{
    const unsigned chromWidth = width >> 1;

  for(int y=0; y<height; y++)
  {
      uint32_t *idst = (uint32_t *) dst;
      const uint8_t *yc = ysrc, *uc = usrc, *vc = vsrc;

    for(unsigned i = 0; i < chromWidth; i++)
    {
      *idst++ = (yc[0] << 0)+ (uc[0] << 8) + (yc[1] << 16) + (vc[0] << 24);
      yc += 2;
      uc++;
      vc++;
    }

    if( (y&1) == 1)
    {
      usrc += chromStride;
      vsrc += chromStride;
    }

    ysrc += lumStride;
    dst  += dstStride;
  }
}

/* ---------------------------------------------------------------------------
 * chroma: frame based
 * lang: MMX2
 */
void yv12_to_yuy2_fr_mmx2(const uint8_t *ysrc,
                          const uint8_t *usrc, const uint8_t *vsrc,
                          uint8_t *dst, int width, int height,
                          int lumStride, int chromStride, int dstStride)
{
#ifdef USE_MMX 
  for (int i=0; i<height; i++)
  {
      const uint8_t *pu, *pv, *py;
      uint8_t  *srfc;

    pu = usrc;
    pv = vsrc;
    py = ysrc;

    srfc = dst;

    for (int j =0; j < width/8; j++)
    {
      movd_m2r(*pu, mm1);       // mm1 = 00 00 00 00 U3 U2 U1 U0
      movd_m2r(*pv, mm2);       // mm2 = 00 00 00 00 V3 V2 V1 V0
      movq_m2r(*py, mm0);       // mm0 = Y7 Y6 Y5 Y4 Y3 Y2 Y1 Y0
      punpcklbw_r2r(mm2, mm1);  // mm1 = V3 U3 V2 U2 V1 U1 V0 U0
      movq_r2r(mm0,mm3);        // mm3 = Y7 Y6 Y5 Y4 Y3 Y2 Y1 Y0
      movq_r2r(mm1,mm4);        // mm4 = V3 U3 V2 U2 V1 U1 V0 U0
      punpcklbw_r2r(mm1, mm0);  // mm0 = V1 Y3 U1 Y2 V0 Y1 U0 Y0
      punpckhbw_r2r(mm4, mm3);  // mm3 = V3 Y7 U3 Y6 V2 Y5 U2 Y4

      movntq(mm0,*srfc);        // Am Meisten brauchen die Speicherzugriffe
      srfc+=8;
      py+=8;
      pu+=4;
      pv+=4;
      movntq(mm3,*srfc);      // wenn movntq nicht geht, dann movq verwenden
      srfc+=8;
    }

    ysrc += lumStride;;

    if (i % 2 == 1)
    {
      usrc += chromStride;
      vsrc += chromStride;
    }

    dst += dstStride;
  }
  SFENCE;
  EMMS;
#endif
}

void yv12_to_yuy2(const uint8_t *ysrc, const uint8_t *usrc, const uint8_t *vsrc,
                  uint8_t *dst, int width, int height,
                  int lumStride, int chromStride, int dstStride)
{
#ifndef USE_MMX
    const unsigned chromWidth = width >> 1;

  for(int y=0; y<height; y++)
  {
      uint32_t *idst = (uint32_t *) dst;
      const uint8_t *yc = ysrc, *uc = usrc, *vc = vsrc;

    for(unsigned i = 0; i < chromWidth; i++)
    {
      *idst++ = (yc[0] << 0)+ (uc[0] << 8) + (yc[1] << 16) + (vc[0] << 24);
      yc += 2;
      uc++;
      vc++;
    }

    if( (y&1) == 1)
    {
      usrc += chromStride;
      vsrc += chromStride;
    }

    ysrc += lumStride;
    dst  += dstStride;
  }
#else
  for (int i=0; i<height; i++)
  {
      const uint8_t *pu, *pv, *py;
      uint8_t  *srfc;

    pu = usrc;
    pv = vsrc;
    py = ysrc;

    srfc = dst;

    for (int j =0; j < width/8; j++)
    {
      movd_m2r(*pu, mm1);       // mm1 = 00 00 00 00 U3 U2 U1 U0
      movd_m2r(*pv, mm2);       // mm2 = 00 00 00 00 V3 V2 V1 V0
      movq_m2r(*py, mm0);       // mm0 = Y7 Y6 Y5 Y4 Y3 Y2 Y1 Y0
      punpcklbw_r2r(mm2, mm1);  // mm1 = V3 U3 V2 U2 V1 U1 V0 U0
      movq_r2r(mm0,mm3);        // mm3 = Y7 Y6 Y5 Y4 Y3 Y2 Y1 Y0
      movq_r2r(mm1,mm4);        // mm4 = V3 U3 V2 U2 V1 U1 V0 U0
      punpcklbw_r2r(mm1, mm0);  // mm0 = V1 Y3 U1 Y2 V0 Y1 U0 Y0
      punpckhbw_r2r(mm4, mm3);  // mm3 = V3 Y7 U3 Y6 V2 Y5 U2 Y4

      movntq(mm0,*srfc);        // Am Meisten brauchen die Speicherzugriffe
      srfc+=8;
      py+=8;
      pu+=4;
      pv+=4;
      movntq(mm3,*srfc);      // wenn movntq nicht geht, dann movq verwenden
      srfc+=8;
    }

    ysrc += lumStride;;

    if (i % 2 == 1)
    {
      usrc += chromStride;
      vsrc += chromStride;
    }

    dst += dstStride;
  }
  SFENCE;
  EMMS;
#endif
}

#ifdef USE_MMX

#define VERT_SCALING
void (*mmx_unpack)(uint8_t * image, int lines, int stride);

static inline void mmx_yuv2rgb (uint8_t * py, uint8_t * pu, uint8_t * pv)
{
    static mmx_t mmx_80w = {0x0080008000800080LL};
    static mmx_t mmx_U_green = {0xf37df37df37df37dLL};
    static mmx_t mmx_U_blue = {0x4093409340934093LL};
    static mmx_t mmx_V_red = {0x3312331233123312LL};
    static mmx_t mmx_V_green = {0xe5fce5fce5fce5fcLL};
    static mmx_t mmx_10w = {0x1010101010101010LL};
    static mmx_t mmx_00ffw = {0x00ff00ff00ff00ffLL};
    static mmx_t mmx_Y_coeff = {0x253f253f253f253fLL};

    movd_m2r (*pu, mm0);		/* mm0 = 00 00 00 00 u3 u2 u1 u0 */
    movd_m2r (*pv, mm1);		/* mm1 = 00 00 00 00 v3 v2 v1 v0 */
    movq_m2r (*py, mm6);		/* mm6 = Y7 Y6 Y5 Y4 Y3 Y2 Y1 Y0 */
    pxor_r2r (mm4, mm4);		/* mm4 = 0 */
    /* XXX might do cache preload for image here */

    /*
     * Do the multiply part of the conversion for even and odd pixels
     * register usage:
     * mm0 -> Cblue, mm1 -> Cred, mm2 -> Cgreen even pixels
     * mm3 -> Cblue, mm4 -> Cred, mm5 -> Cgreen odd  pixels
     * mm6 -> Y even, mm7 -> Y odd
     */

    punpcklbw_r2r (mm4, mm0);		/* mm0 = u3 u2 u1 u0 */
    punpcklbw_r2r (mm4, mm1);		/* mm1 = v3 v2 v1 v0 */
    psubsw_m2r (mmx_80w, mm0);		/* u -= 128 */
    psubsw_m2r (mmx_80w, mm1);		/* v -= 128 */
    psllw_i2r (3, mm0);			/* promote precision */
    psllw_i2r (3, mm1);			/* promote precision */
    movq_r2r (mm0, mm2);		/* mm2 = u3 u2 u1 u0 */
    movq_r2r (mm1, mm3);		/* mm3 = v3 v2 v1 v0 */
    pmulhw_m2r (mmx_U_green, mm2);	/* mm2 = u * u_green */
    pmulhw_m2r (mmx_V_green, mm3);	/* mm3 = v * v_green */
    pmulhw_m2r (mmx_U_blue, mm0);	/* mm0 = chroma_b */
    pmulhw_m2r (mmx_V_red, mm1);	/* mm1 = chroma_r */
    paddsw_r2r (mm3, mm2);		/* mm2 = chroma_g */

    psubusb_m2r (mmx_10w, mm6);		/* Y -= 16 */
    movq_r2r (mm6, mm7);		/* mm7 = Y7 Y6 Y5 Y4 Y3 Y2 Y1 Y0 */
    pand_m2r (mmx_00ffw, mm6);		/* mm6 =    Y6    Y4    Y2    Y0 */
    psrlw_i2r (8, mm7);			/* mm7 =    Y7    Y5    Y3    Y1 */
    psllw_i2r (3, mm6);			/* promote precision */
    psllw_i2r (3, mm7);			/* promote precision */
    pmulhw_m2r (mmx_Y_coeff, mm6);	/* mm6 = luma_rgb even */
    pmulhw_m2r (mmx_Y_coeff, mm7);	/* mm7 = luma_rgb odd */

    /*
     * Do the addition part of the conversion for even and odd pixels
     * register usage:
     * mm0 -> Cblue, mm1 -> Cred, mm2 -> Cgreen even pixels
     * mm3 -> Cblue, mm4 -> Cred, mm5 -> Cgreen odd  pixels
     * mm6 -> Y even, mm7 -> Y odd
     */

    movq_r2r (mm0, mm3);		/* mm3 = chroma_b */
    movq_r2r (mm1, mm4);		/* mm4 = chroma_r */
    movq_r2r (mm2, mm5);		/* mm5 = chroma_g */
    paddsw_r2r (mm6, mm0);		/* mm0 = B6 B4 B2 B0 */
    paddsw_r2r (mm7, mm3);		/* mm3 = B7 B5 B3 B1 */
    paddsw_r2r (mm6, mm1);		/* mm1 = R6 R4 R2 R0 */
    paddsw_r2r (mm7, mm4);		/* mm4 = R7 R5 R3 R1 */
    paddsw_r2r (mm6, mm2);		/* mm2 = G6 G4 G2 G0 */
    paddsw_r2r (mm7, mm5);		/* mm5 = G7 G5 G3 G1 */
    packuswb_r2r (mm0, mm0);		/* saturate to 0-255 */
    packuswb_r2r (mm1, mm1);		/* saturate to 0-255 */
    packuswb_r2r (mm2, mm2);		/* saturate to 0-255 */
    packuswb_r2r (mm3, mm3);		/* saturate to 0-255 */
    packuswb_r2r (mm4, mm4);		/* saturate to 0-255 */
    packuswb_r2r (mm5, mm5);		/* saturate to 0-255 */
    punpcklbw_r2r (mm3, mm0);		/* mm0 = B7 B6 B5 B4 B3 B2 B1 B0 */
    punpcklbw_r2r (mm4, mm1);		/* mm1 = R7 R6 R5 R4 R3 R2 R1 R0 */
    punpcklbw_r2r (mm5, mm2);		/* mm2 = G7 G6 G5 G4 G3 G2 G1 G0 */
}

void mmx_unpack_16rgb (uint8_t * image, int lines, int stride)
{
    static mmx_t mmx_bluemask = {0xf8f8f8f8f8f8f8f8ll};
    static mmx_t mmx_greenmask = {0xfcfcfcfcfcfcfcfcll};
    static mmx_t mmx_redmask = {0xf8f8f8f8f8f8f8f8ll};

    /*
     * convert RGB plane to RGB 16 bits
     * mm0 -> B, mm1 -> R, mm2 -> G
     * mm4 -> GB, mm5 -> AR pixel 4-7
     * mm6 -> GB, mm7 -> AR pixel 0-3
     */

    pand_m2r (mmx_bluemask, mm0);       // mm0 = b7b6b5b4b3______
    pxor_r2r (mm4, mm4);                // mm4 = 0

    pand_m2r (mmx_greenmask, mm2);      // mm2 = g7g6g5g4g3g2____
    psrlq_i2r (3, mm0);                 // mm0 = ______b7b6b5b4b3

    movq_r2r (mm2, mm7);                // mm7 = g7g6g5g4g3g2____
    movq_r2r (mm0, mm5);                // mm5 = ______b7b6b5b4b3

    pand_m2r (mmx_redmask, mm1);        // mm1 = r7r6r5r4r3______
    punpcklbw_r2r (mm4, mm2);

    punpcklbw_r2r (mm1, mm0);

    psllq_i2r (3, mm2);

    punpckhbw_r2r (mm4, mm7);
    por_r2r (mm2, mm0);

    psllq_i2r (3, mm7);


    // jetzt muss ich die an bestimmten Stellen verdoppeln.
    movntq (mm0, *(image));

    punpckhbw_r2r (mm1, mm5);
    por_r2r (mm7, mm5);

    movntq (mm5, *(image+8));
    while(--lines) { // write the same in the line above
      image -= stride;
      movntq (mm0, *(image));
      movntq (mm5, *(image+8));
    }

}

void mmx_unpack_15rgb (uint8_t * image, int lines, int stride)
{
    static mmx_t mmx_bluemask = {0xf8f8f8f8f8f8f8f8ll};
    static mmx_t mmx_greenmask = {0xf8f8f8f8f8f8f8f8ll};
    static mmx_t mmx_redmask = {0xf8f8f8f8f8f8f8f8ll};

    /*
     * convert RGB plane to RGB 16 bits
     * mm0 -> B, mm1 -> R, mm2 -> G
     * mm4 -> GB, mm5 -> AR pixel 4-7
     * mm6 -> GB, mm7 -> AR pixel 0-3
     */

    pand_m2r (mmx_bluemask, mm0);       // mm0 = b7b6b5b4b3______
    pxor_r2r (mm4, mm4);                // mm4 = 0

    pand_m2r (mmx_greenmask, mm2);      // mm2 = g7g6g5g4g3g2____
    psrlq_i2r (2, mm0);                 // mm0 = ______b7b6b5b4b3

    movq_r2r (mm2, mm7);                // mm7 = g7g6g5g4g3g2____
    movq_r2r (mm0, mm5);                // mm5 = ______b7b6b5b4b3

    pand_m2r (mmx_redmask, mm1);        // mm1 = r7r6r5r4r3______
    punpcklbw_r2r (mm4, mm2);

    punpcklbw_r2r (mm1, mm0);

    psllq_i2r (3, mm2);

    punpckhbw_r2r (mm4, mm7);
    por_r2r (mm2, mm0);

    psllq_i2r (3, mm7);

    // jetzt muss ich die an bestimmten Stellen verdoppeln.
    psrlq_i2r(1,mm0);//MW
    movntq (mm0, *(image));

    punpckhbw_r2r (mm1, mm5);
    por_r2r (mm7, mm5);

    psrlq_i2r(1,mm5); //MW
    movntq (mm5, *(image+8));
    while(--lines) { // write the same in the line above
      image -= stride;
      movntq (mm0, *(image));
      movntq (mm5, *(image+8));
    }

}

void yuv_to_rgb (uint8_t * image, uint8_t * py,
                 uint8_t * pu, uint8_t * pv,
                 int width, int height,
                 int rgb_stride, int y_stride, int uv_stride,
                 int dstW, int dstH,
                 int depth, unsigned char * mask, int deintMethod)
{
/*
    Do the YUV->RGB transformation and scale the picture upt to dstW x dstH
    Do NOT touch the pixels that are masked by '*mask'
    This algo works only in 16bit mode
*/
    uint16_t  pix[8];
    uint8_t   *IM, *Y, *Y1, *Y2, *U, *V;
    uint8_t   *scaleY, *scaleU, *scaleV;
    uint8_t   *sY, *sU, *sV;
    uint8_t   *m;
    int       oldY = -1, dstY, lines;

//    printf("[utils] Scaling %d x %d YUV image to %d x %d %d Bit image, Pixmask %d\n",width,height,dstW,dstH,depth, mask);
  scaleY = (uint8_t *)malloc(dstW);
  scaleU = (uint8_t *)malloc(dstW);
  scaleV = (uint8_t *)malloc(dstW);

  for (int y = 0; y < height; y++) {

    dstY = (y * dstH) / height;       // scaling: where to draw the line
    IM = image + dstY * rgb_stride ;
    Y = py + y * y_stride;

    /* ------------------------------------------------------------------------
     * prepare pointer for deinterlacing (set them allways, deintMethod may
     * change any time in the middle of this loop, since it is not protected
     * by a mutex.
     * For deinterlacing with our internal method we need previous and next line
     */
    Y1 = Y2 = Y;

    Y1 -= (y > 0)          ? y_stride : 0;
    Y2 += (y < (height-1)) ? y_stride : 0;

    U = pu + (y/2) * uv_stride;
    V = pv + (y/2) * uv_stride;

    lines = dstY - oldY;
    oldY = dstY;
    if (!lines) continue;
    m=mask;
    sY = scaleY;
    sU = scaleU;
    sV = scaleV;
#ifdef VERT_SCALING
    // maybe i find a better algo to stretch the lines
    for (int x = 0; x < dstW; x++) {
        int srcpix = x * width / dstW;

      if (deintMethod == 2) {
        // deinterlace with FB-intern method
        scaleY[x] = (Y[srcpix] / 2) + (Y1[srcpix] /4) + (Y2[srcpix] / 4);
      } else scaleY[x] = Y[srcpix];

      if ((x < dstW / 2) && (y % 2 == 0)) {
        scaleU[x] = U[srcpix];
        scaleV[x] = V[srcpix];
      }
    }
#else
    sY = Y;
    sU = U;
    sV = V;
    dstW = width;
#endif
    for (int x = 0; x < dstW; x+=8) {
      if (!m) { // no mask given
        mmx_yuv2rgb (sY, sU, sV);
        mmx_unpack (IM, lines, rgb_stride);
      } else { // mask given
        if (*m != 0xFF) { // not all bits masked
          mmx_yuv2rgb (sY, sU, sV);
          if (*m == 0) { // no bit is masked
            mmx_unpack (IM, lines, rgb_stride);
          } else { // at least one bit is masked
            mmx_unpack ((uint8_t *)pix, lines, rgb_stride);
            for (int i = 0; i < 8; i++) {
              if (! (*m & (1 << i)) ) {
                for (int j = 0; j < lines; j++) {
                  *(uint16_t *)(IM - j*rgb_stride) = pix[i];
                }
              }
              IM += 2;
            }
            IM -= 16;
          }
        }
        m++;
      }
      sY += 8;
      sU += 4;
      sV += 4;
      IM += 16;
    }
    if (mask) mask += lines * rgb_stride / 16;
  }
  free(scaleY);
  free(scaleU);
  free(scaleV);
}
#endif // USE_MMX

inline uint8_t clip( int x) {
  if (x<=0)
    return 0;
  if (x>=255)
    return 255;
  return (uint8_t) x;
};

#define YUV420P_TO_RGB(FMT) \
    int y=(((int) *py1)-16)*298;        \
    int u=((int) *pu)-128;              \
    int v=((int) *pv)-128;              \
                                        \
    int r=(409*v+128);                  \
    int g=(-100*u-208*v+128);           \
    int b=(516*u+128);                  \
                                        \
    WRITE_##FMT(dst,clip((y+r)>>8),clip((y+g)>>8),clip((y+b)>>8)); \
    py1++;                              \
                                        \
    y=(((int) *py1)-16)*298;            \
    WRITE_##FMT(dst+SIZE_##FMT,clip((y+r)>>8),clip((y+g)>>8),clip((y+b)>>8)); \
    py1++;                              \
                                        \
    /* second line */                   \
    y=(((int) *py2)-16)*298;            \
    WRITE_##FMT(dst+dst_stride,clip((y+r)>>8),clip((y+g)>>8),clip((y+b)>>8)); \
    py2++;                              \
                                        \
    y=(((int) *py2)-16)*298;            \
    WRITE_##FMT(dst+dst_stride+SIZE_##FMT,clip((y+r)>>8),clip((y+g)>>8),clip((y+b)>>8)); \
    py2++;                              \
                                        \
    dst+=2*SIZE_##FMT;                  \
    pu++;                               \
    pv++;                               

// MMX macros taken from libswscale
/*
 * Copyright (C) 2000, Silicon Integrated System Corp.
 * All Rights Reserved.
 *
 * Author: Olie Lho <ollie@sis.com.tw>
 *
 * This file is part of mpeg2dec, a free MPEG-2 video decoder
 * 
 * 15,24 bpp and dithering from Michael Niedermayer (michaelni@gmx.at)
 * MMX/MMX2 Template stuff from Michael Niedermayer (needed for fast movntq support)
 * context / deglobalize stuff by Michael Niedermayer
 */

#define Y_COEFF      "3*8"
#define VR_COEFF     "4*8"
#define UB_COEFF     "5*8"
#define VG_COEFF     "6*8"
#define UG_COEFF     "7*8"
#define Y_OFFSET     "8*8"
#define U_OFFSET     "9*8"
#define V_OFFSET     "10*8"
#define MASK_00FF    "11*8"
#define RED_MASK     "12*8"
#define GREEN_MASK   "13*8"
#define M24A_MASK    "14*8"
#define M24B_MASK    "15*8"
#define M24C_MASK    "16*8"

static uint64_t __attribute__((aligned(8))) MMX_Constants[]= {
        0,
        0,
        0,
        0x253f253f253f253fULL, /* Y_COEFF    */
        0x3312331233123312ULL, /* VR_COEFF   */
        0x4093409340934093ULL, /* UB_COEFF   */
        0xe5fce5fce5fce5fcULL, /* VG_COEFF   */
        0xf37df37df37df37dULL, /* UG_COEFF   */
        0x0080008000800080ULL, /* Y_OFFSET   */
        0x0400040004000400ULL, /* U_OFFSET   */
        0x0400040004000400ULL, /* V_OFFSET   */
        0x00ff00ff00ff00ffULL, /* mask_00ff  */
        0xf8f8f8f8f8f8f8f8ULL, /* red_mask   */
        0xfcfcfcfcfcfcfcfcULL, /* green_mask */ 
        0x00FF0000FF0000FFULL, /* M24A */  
        0xFF0000FF0000FF00ULL, /* M24B */
        0x0000FF0000FF0000ULL, /* M24C */
};


#define YUV2RGB \
     /* Do the multiply part of the conversion for even and odd pixels,
	register usage:
	mm0 -> Cblue, mm1 -> Cred, mm2 -> Cgreen even pixels,
	mm3 -> Cblue, mm4 -> Cred, mm5 -> Cgreen odd pixels,
	mm6 -> Y even, mm7 -> Y odd */\
     /* convert the chroma part */\
     "punpcklbw %%mm4, %%mm0\n" /* scatter 4 Cb 00 u3 00 u2 00 u1 00 u0 */ \
     "punpcklbw %%mm4, %%mm1\n" /* scatter 4 Cr 00 v3 00 v2 00 v1 00 v0 */ \
\
     "psllw $3, %%mm0\n" /* Promote precision */ \
     "psllw $3, %%mm1\n" /* Promote precision */ \
\
     "psubsw "U_OFFSET"(%0), %%mm0\n" /* Cb -= 128 */ \
     "psubsw "V_OFFSET"(%0), %%mm1\n" /* Cr -= 128 */ \
\
     "movq %%mm0, %%mm2\n" /* Copy 4 Cb 00 u3 00 u2 00 u1 00 u0 */ \
     "movq %%mm1, %%mm3\n" /* Copy 4 Cr 00 v3 00 v2 00 v1 00 v0 */ \
\
     "pmulhw "UG_COEFF"(%0), %%mm2\n" /* Mul Cb with green coeff -> Cb green */ \
     "pmulhw "VG_COEFF"(%0), %%mm3\n" /* Mul Cr with green coeff -> Cr green */ \
\
     "pmulhw "UB_COEFF"(%0), %%mm0\n" /* Mul Cb -> Cblue 00 b3 00 b2 00 b1 00 b0 */\
     "pmulhw "VR_COEFF"(%0), %%mm1\n" /* Mul Cr -> Cred 00 r3 00 r2 00 r1 00 r0 */\
\
     "paddsw %%mm3, %%mm2\n" /* Cb green + Cr green -> Cgreen */\
\
     /* convert the luma part */\
     "movq %%mm6, %%mm7\n" /* Copy 8 Y Y7 Y6 Y5 Y4 Y3 Y2 Y1 Y0 */\
     "pand "MASK_00FF"(%0), %%mm6\n" /* get Y even 00 Y6 00 Y4 00 Y2 00 Y0 */\
\
     "psrlw $8, %%mm7\n" /* get Y odd 00 Y7 00 Y5 00 Y3 00 Y1 */\
\
     "psllw $3, %%mm6\n" /* Promote precision */\
     "psllw $3, %%mm7\n" /* Promote precision */\
\
     "psubw "Y_OFFSET"(%0), %%mm6\n" /* Y -= 16 */\
     "psubw "Y_OFFSET"(%0), %%mm7\n" /* Y -= 16 */\
\
     "pmulhw "Y_COEFF"(%0), %%mm6\n" /* Mul 4 Y even 00 y6 00 y4 00 y2 00 y0 */\
     "pmulhw "Y_COEFF"(%0), %%mm7\n" /* Mul 4 Y odd 00 y7 00 y5 00 y3 00 y1 */\
\
     /* Do the addition part of the conversion for even and odd pixels,
	register usage:
	mm0 -> Cblue, mm1 -> Cred, mm2 -> Cgreen even pixels,
	mm3 -> Cblue, mm4 -> Cred, mm5 -> Cgreen odd pixels,
	mm6 -> Y even, mm7 -> Y odd */\
     "movq %%mm0, %%mm3\n" /* Copy Cblue */\
     "movq %%mm1, %%mm4\n" /* Copy Cred */\
     "movq %%mm2, %%mm5\n" /* Copy Cgreen */\
\
     "paddsw %%mm6, %%mm0\n" /* Y even + Cblue 00 B6 00 B4 00 B2 00 B0 */\
     "paddsw %%mm7, %%mm3\n" /* Y odd + Cblue 00 B7 00 B5 00 B3 00 B1 */\
\
     "paddsw %%mm6, %%mm1\n" /* Y even + Cred 00 R6 00 R4 00 R2 00 R0 */\
     "paddsw %%mm7, %%mm4\n" /* Y odd + Cred 00 R7 00 R5 00 R3 00 R1 */\
\
     "paddsw %%mm6, %%mm2\n" /* Y even + Cgreen 00 G6 00 G4 00 G2 00 G0 */\
     "paddsw %%mm7, %%mm5\n" /* Y odd + Cgreen 00 G7 00 G5 00 G3 00 G1 */\
\
     /* Limit RGB even to 0..255 */\
     "packuswb %%mm0, %%mm0\n" /* B6 B4 B2 B0  B6 B4 B2 B0 */\
     "packuswb %%mm1, %%mm1\n" /* R6 R4 R2 R0  R6 R4 R2 R0 */\
     "packuswb %%mm2, %%mm2\n" /* G6 G4 G2 G0  G6 G4 G2 G0 */\
\
     /* Limit RGB odd to 0..255 */\
     "packuswb %%mm3, %%mm3\n" /* B7 B5 B3 B1  B7 B5 B3 B1 */\
     "packuswb %%mm4, %%mm4\n" /* R7 R5 R3 R1  R7 R5 R3 R1 */\
     "packuswb %%mm5, %%mm5\n" /* G7 G5 G3 G1  G7 G5 G3 G1 */\
\
     /* Interleave RGB even and odd */\
     "punpcklbw %%mm3, %%mm0\n" /* B7 B6 B5 B4 B3 B2 B1 B0 */\
     "punpcklbw %%mm4, %%mm1\n" /* R7 R6 R5 R4 R3 R2 R1 R0 */\
     "punpcklbw %%mm5, %%mm2\n" /* G7 G6 G5 G4 G3 G2 G1 G0 */\

#define WRITE_RGB16_MMX  \
       /* mask unneeded bits off */\
       "pand "RED_MASK"(%0), %%mm0\n" /* b7b6b5b4 b3_0_0_0 b7b6b5b4 b3_0_0_0 */\
       "pand "GREEN_MASK"(%0), %%mm2\n" /* g7g6g5g4 g3g2_0_0 g7g6g5g4 g3g2_0_0 */\
       "pand "RED_MASK"(%0), %%mm1\n" /* r7r6r5r4 r3_0_0_0 r7r6r5r4 r3_0_0_0 */\
\
       "psrlw $3,%%mm0\n" /* 0_0_0_b7 b6b5b4b3 0_0_0_b7 b6b5b4b3 */\
       "pxor %%mm4, %%mm4\n" /* zero mm4 */\
\
       "movq %%mm0, %%mm5\n" /* Copy B7-B0 */\
       "movq %%mm2, %%mm7\n" /* Copy G7-G0 */\
\
       /* convert rgb24 plane to rgb16 pack for pixel 0-3 */\
       "punpcklbw %%mm4, %%mm2\n" /* 0_0_0_0 0_0_0_0 g7g6g5g4 g3g2_0_0 */\
       "punpcklbw %%mm1, %%mm0\n" /* r7r6r5r4 r3_0_0_0 0_0_0_b7 b6b5b4b3 */\
\
       "psllw $3, %%mm2\n" /* 0_0_0_0 0_g7g6g5 g4g3g2_0 0_0_0_0 */\
       "por %%mm2, %%mm0\n" /* r7r6r5r4 r3g7g6g5 g4g3g2b7 b6b5b4b3 */\
\
       MOVNTQ " %%mm0, (%1)\n" /* store pixel 0-3 */\
\
       /* convert rgb24 plane to rgb16 pack for pixel 0-3 */\
       "punpckhbw %%mm4, %%mm7\n" /* 0_0_0_0 0_0_0_0 g7g6g5g4 g3g2_0_0 */\
       "punpckhbw %%mm1, %%mm5\n" /* r7r6r5r4 r3_0_0_0 0_0_0_b7 b6b5b4b3 */\
\
       "psllw $3, %%mm7\n" /* 0_0_0_0 0_g7g6g5 g4g3g2_0 0_0_0_0 */\
       "por %%mm7, %%mm5\n" /* r7r6r5r4 r3g7g6g5 g4g3g2b7 b6b5b4b3*/\
       MOVNTQ " %%mm5, 8 (%1)\n" /* store pixel 4-7 */\

#define WRITE_RGB24_MMX  \
	"movq "M24A_MASK"(%0), %%mm4	\n\t" \
	"movq "M24C_MASK"(%0), %%mm7	\n\t" \
	"pshufw $0x50, %%mm0, %%mm5	\n\t" /* B3 B2 B3 B2  B1 B0 B1 B0 */\
	"pshufw $0x50, %%mm2, %%mm3	\n\t" /* G3 G2 G3 G2  G1 G0 G1 G0 */\
	"pshufw $0x00, %%mm1, %%mm6	\n\t" /* R1 R0 R1 R0  R1 R0 R1 R0 */\
\
	"pand %%mm4, %%mm5		\n\t" /*    B2        B1       B0 */\
	"pand %%mm4, %%mm3		\n\t" /*    G2        G1       G0 */\
	"pand %%mm7, %%mm6		\n\t" /*       R1        R0       */\
\
	"psllq $8, %%mm3		\n\t" /* G2        G1       G0    */\
	"por %%mm5, %%mm6		\n\t"\
	"por %%mm3, %%mm6		\n\t"\
	MOVNTQ" %%mm6, (%1)		\n\t"\
\
	"psrlq $8, %%mm2		\n\t" /* 00 G7 G6 G5  G4 G3 G2 G1 */\
	"pshufw $0xA5, %%mm0, %%mm5	\n\t" /* B5 B4 B5 B4  B3 B2 B3 B2 */\
	"pshufw $0x55, %%mm2, %%mm3	\n\t" /* G4 G3 G4 G3  G4 G3 G4 G3 */\
	"pshufw $0xA5, %%mm1, %%mm6	\n\t" /* R5 R4 R5 R4  R3 R2 R3 R2 */\
\
	"pand "M24B_MASK"(%0), %%mm5	\n\t" /* B5       B4        B3    */\
	"pand %%mm7, %%mm3		\n\t" /*       G4        G3       */\
	"pand %%mm4, %%mm6		\n\t" /*    R4        R3       R2 */\
\
	"por %%mm5, %%mm3		\n\t" /* B5    G4 B4     G3 B3    */\
	"por %%mm3, %%mm6		\n\t"\
	MOVNTQ" %%mm6, 8(%1)		\n\t"\
\
	"pshufw $0xFF, %%mm0, %%mm5	\n\t" /* B7 B6 B7 B6  B7 B6 B6 B7 */\
	"pshufw $0xFA, %%mm2, %%mm3	\n\t" /* 00 G7 00 G7  G6 G5 G6 G5 */\
	"pshufw $0xFA, %%mm1, %%mm6	\n\t" /* R7 R6 R7 R6  R5 R4 R5 R4 */\
\
	"pand %%mm7, %%mm5		\n\t" /*       B7        B6       */\
	"pand %%mm4, %%mm3		\n\t" /*    G7        G6       G5 */\
	"pand "M24B_MASK"(%0), %%mm6	\n\t" /* R7       R6        R5    */\
\
	"por %%mm5, %%mm3		\n\t"\
	"por %%mm3, %%mm6		\n\t"\
	MOVNTQ" %%mm6, 16(%1)		\n\t"\
	"pxor %%mm4, %%mm4		\n\t"\

#define WRITE_RGB32_MMX \
        /* convert RGB plane to RGB packed format, */\
	/* mm0 -> B, mm1 -> R, mm2 -> G, mm3 -> 0, */\
	/* mm4 -> GB, mm5 -> AR pixel 4-7,         */\
	/* mm6 -> GB, mm7 -> AR pixel 0-3          */\
        "pxor %%mm3, %%mm3\n" /* zero mm3 */ \
\
        "movq %%mm0, %%mm6\n" /* B7 B6 B5 B4 B3 B2 B1 B0 */\
	"movq %%mm1, %%mm7\n" /* R7 R6 R5 R4 R3 R2 R1 R0 */\
\
	"movq %%mm0, %%mm4\n" /* B7 B6 B5 B4 B3 B2 B1 B0 */\
	"movq %%mm1, %%mm5\n" /* R7 R6 R5 R4 R3 R2 R1 R0 */\
\
	"punpcklbw %%mm2, %%mm6\n" /* G3 B3 G2 B2 G1 B1 G0 B0 */\
	"punpcklbw %%mm3, %%mm7\n" /* 00 R3 00 R2 00 R1 00 R0 */\
\
	"punpcklwd %%mm7, %%mm6\n" /* 00 R1 B1 G1 00 R0 B0 G0 */\
	MOVNTQ " %%mm6, (%1)\n" /* Store ARGB1 ARGB0 */\
\
	"movq %%mm0, %%mm6\n" /* B7 B6 B5 B4 B3 B2 B1 B0 */\
	"punpcklbw %%mm2, %%mm6\n" /* G3 B3 G2 B2 G1 B1 G0 B0 */\
\
	"punpckhwd %%mm7, %%mm6\n" /* 00 R3 G3 B3 00 R2 B3 G2 */\
	MOVNTQ " %%mm6, 8 (%1)\n" /* Store ARGB3 ARGB2 */\
\
	"punpckhbw %%mm2, %%mm4\n" /* G7 B7 G6 B6 G5 B5 G4 B4 */\
	"punpckhbw %%mm3, %%mm5\n" /* 00 R7 00 R6 00 R5 00 R4 */\
\
	"punpcklwd %%mm5, %%mm4\n" /* 00 R5 B5 G5 00 R4 B4 G4 */\
	MOVNTQ " %%mm4, 16 (%1)\n" /* Store ARGB5 ARGB4 */\
\
	"movq %%mm0, %%mm4\n" /* B7 B6 B5 B4 B3 B2 B1 B0 */\
	"punpckhbw %%mm2, %%mm4\n" /* G7 B7 G6 B6 G5 B5 G4 B4 */\
\
	"punpckhwd %%mm5, %%mm4\n" /* 00 R7 G7 B7 00 R6 B6 G6 */\
	MOVNTQ " %%mm4, 24 (%1)\n" /* Store ARGB7 ARGB6 */\
\
	"pxor %%mm4, %%mm4\n" /* zero mm4 */\

                  
// end of MMX macros from libswscale

void yuv420_to_rgb32(uint8_t *dst, int dst_stride,
                 uint8_t *py1, uint8_t *py2, uint8_t *pu, uint8_t *pv, 
                 int pixel)
{
#ifdef USE_MMX
  pixel/=8;
  __asm__ __volatile__(
      "pxor %%mm4, %%mm4 \n"
      : : : "memory" );
  while (pixel) {
    __asm__ __volatile__ (
          "movd (%1), %%mm6  \n"
          "movd (%2), %%mm0  \n"
          "movd (%3), %%mm1  \n"
          "punpckldq 4(%1), %%mm6  \n"
          YUV2RGB
        	: : "r" (MMX_Constants), "r" (py1), "r" (pu), "r" (pv)
                : "memory");
    __asm__ __volatile__ (
          WRITE_RGB32_MMX
                : : "r" (MMX_Constants),"r" (dst)
                : "memory");
    
    __asm__ __volatile__ (
          "movd (%1), %%mm6  \n"
          "movd (%2), %%mm0  \n"
          "movd (%3), %%mm1  \n"
          "punpckldq 4(%1), %%mm6  \n"
          YUV2RGB
        	: : "r" (MMX_Constants), "r" (py2), "r" (pu), "r" (pv)
                : "memory");
    __asm__ __volatile__ (
          WRITE_RGB32_MMX
                : : "r" (MMX_Constants), "r" (dst+dst_stride)
                : "memory");
    dst+=32;
    py1+=8;
    py2+=8;
    pu+=4;
    pv+=4;
    pixel--;
  };
  EMMS;
#else
  pixel/=2;
  while (pixel) {
    YUV420P_TO_RGB(RGB32);  
    pixel--;
  };
#endif
};

void yuv420_to_rgb24(uint8_t *dst, int dst_stride,
                 uint8_t *py1, uint8_t *py2, uint8_t *pu, uint8_t *pv, 
                 int pixel)
{
#ifdef USE_MMX2
  pixel/=8;
  __asm__ __volatile__(
      "pxor %%mm4, %%mm4 \n"
      : : : "memory" );
  while (pixel) {
    __asm__ __volatile__ (
          "movd (%1), %%mm6  \n"
          "movd (%2), %%mm0  \n"
          "movd (%3), %%mm1  \n"
          "punpckldq 4(%1), %%mm6  \n"
          YUV2RGB
        	: : "r" (MMX_Constants), "r" (py1), "r" (pu), "r" (pv)
                : "memory");
    __asm__ __volatile__ (
          WRITE_RGB24_MMX
                : : "r" (MMX_Constants),"r" (dst)
                : "memory");
    
    __asm__ __volatile__ (
          "movd (%1), %%mm6  \n"
          "movd (%2), %%mm0  \n"
          "movd (%3), %%mm1  \n"
          "punpckldq 4(%1), %%mm6  \n"
          YUV2RGB
        	: : "r" (MMX_Constants), "r" (py2), "r" (pu), "r" (pv)
                : "memory");
    __asm__ __volatile__ (
          WRITE_RGB24_MMX
                : : "r" (MMX_Constants), "r" (dst+dst_stride)
                : "memory");
    dst+=24;
    py1+=8;
    py2+=8;
    pu+=4;
    pv+=4;
    pixel--;
  };
  EMMS;
#else
  pixel/=2;
  while (pixel) {
    YUV420P_TO_RGB(RGB24);  
    pixel--;
  };
#endif
};

void yuv420_to_bgr24(uint8_t *dst, int dst_stride,
                 uint8_t *py1, uint8_t *py2, uint8_t *pu, uint8_t *pv, 
                 int pixel)
{
  pixel/=2;
  while (pixel) {
    YUV420P_TO_RGB(BGR24);  
    pixel--;
  };
};

void yuv420_to_rgb16(uint8_t *dst, int dst_stride,
                 uint8_t *py1, uint8_t *py2, uint8_t *pu, uint8_t *pv, 
                 int pixel)
{
#ifdef USE_MMX
  pixel/=8;
  __asm__ __volatile__(
      "pxor %%mm4, %%mm4 \n"
      : : : "memory" );
  while (pixel) {
    __asm__ __volatile__ (
          "movd (%1), %%mm6  \n"
          "movd (%2), %%mm0  \n"
          "movd (%3), %%mm1  \n"
          "punpckldq 4(%1), %%mm6  \n"
          YUV2RGB
        	: : "r" (MMX_Constants), "r" (py1), "r" (pu), "r" (pv)
                : "memory");
    __asm__ __volatile__ (
          WRITE_RGB16_MMX
                : : "r" (MMX_Constants),"r" (dst)
                : "memory");
    __asm__ __volatile__ (
          "movd (%1), %%mm6  \n"
          "movd (%2), %%mm0  \n"
          "movd (%3), %%mm1  \n"
          "punpckldq 4(%1), %%mm6  \n"
          YUV2RGB
        	: : "r" (MMX_Constants), "r" (py2), "r" (pu), "r" (pv)
                : "memory");
    __asm__ __volatile__ (
          WRITE_RGB16_MMX
                : : "r" (MMX_Constants), "r" (dst+dst_stride)
                : "memory");
    dst+=16;
    py1+=8;
    py2+=8;
    pu+=4;
    pv+=4;
    pixel--;
  };
  EMMS;
#else
  pixel/=2;
  while (pixel) {
    YUV420P_TO_RGB(RGB16);  
    pixel--;
  };
#endif
};

void AlphaBlend(uint8_t *dest,uint8_t *P1,uint8_t *P2,
          uint8_t *alpha,uint16_t count) {
     // printf("%x %x %x \n",P1,P2,alpha);

#ifdef USE_MMX
        __asm__(" pxor %%mm3,%%mm3\n"
                PREFETCH("(%0)\n")
                PREFETCH("(%1)\n")
                PREFETCH("(%2)\n")
                PREFETCH("64(%0)\n")
                PREFETCH("64(%1)\n")
                PREFETCH("64(%2)\n")
                PREFETCH("128(%0)\n")
                PREFETCH("128(%1)\n")
                PREFETCH("128(%2)\n")
                : : "r" (P1), "r" (P2), "r" (alpha) : "memory");

        // I guess this can be further improved...
	// Useing prefetch makes it slower on Athlon64,
	// but faster on the Athlon Tbird...
	// Why is prefetching slower on Athlon64????
        // Because the Athlon64 does automatic block prefetching...
        while (count>8 ) {
#ifdef USE_MMX2
          if (! (count%8 ) )
               __asm__(
                  PREFETCH(" 192(%0)\n")
                  PREFETCH(" 192(%1)\n")
                  PREFETCH(" 192(%2)\n")
                  : : "r" (P1), "r" (P2), "r" (alpha) : "memory");

#endif //USE_MMX2
         __asm__(
                "  movq  (%0),%%mm0\n"
                "  movq  (%1),%%mm1\n"
                "  movq  (%2),%%mm2\n"
                "  movq  %%mm0,%%mm4\n"
                "  movq  %%mm1,%%mm5\n"
                "  movq  %%mm2,%%mm6\n"
                "  punpcklbw %%mm3, %%mm0\n"
                "  punpcklbw %%mm3, %%mm1\n"
                "  punpcklbw %%mm3, %%mm2\n"
                "  punpckhbw %%mm3, %%mm4\n"
                "  punpckhbw %%mm3, %%mm5\n"
                "  punpckhbw %%mm3, %%mm6\n"
                "  psubw %%mm1, %%mm0 \n"
                "  psubw %%mm5, %%mm4 \n"
                "  psraw $1,%%mm0\n"
                "  psraw $1,%%mm4\n"
                "  pmullw %%mm2, %%mm0 \n"
                "  pmullw %%mm6, %%mm4 \n"
                "  psraw $7,%%mm0\n"
                "  psraw $7,%%mm4\n"
                "  paddw %%mm1, %%mm0 \n"
                "  paddw %%mm5, %%mm4 \n"
                "  packuswb %%mm4, %%mm0 \n"
                MOVNTQ " %%mm0,(%3)\n"
                : : "r" (P1), "r" (P2), "r" (alpha),"r"(dest) : "memory");
                count-=8;
                P1+=8;
                P2+=8;
                alpha+=8;
                dest+=8;
       }
       EMMS;
#endif //USE_MMX

#ifdef USE_ALTIVEC 
       vector unsigned char zero = vec_splat_u8(0);
       vector unsigned short one = vec_splat_u16(1);
       vector unsigned short seven = vec_splat_u16(7);
       vector short p1h;
       vector short p1l;
       vector short p2h;
       vector short p2l;
       vector short ah;
       vector short al;
       for (; count>15; count-=16) {
                p1h = (vector short) vec_ld(0,P1);
                p2h = (vector short) vec_ld(0,P2);
                p1l = (vector short) vec_mergel( zero, (vector unsigned char) p1h);
                p2l = (vector short) vec_mergel( zero, (vector unsigned char) p2h);
                ah  = (vector short) vec_ld(0,alpha);
                
                p1h = (vector short) vec_mergeh( zero, (vector unsigned char) p1h);
                p2h = (vector short) vec_mergeh( zero, (vector unsigned char) p2h);
                
                al = (vector short) vec_mergel( zero, (vector unsigned char) ah);
                ah = (vector short) vec_mergeh( zero, (vector unsigned char) ah);
                
                p1h = vec_subs(p1h,p2h);
                p1l = vec_subs(p1l,p2l);
                
                ah = vec_sr(ah, one);
                al = vec_sr(al, one);
       
                p1h = vec_mladd( p1h, ah, (vector short) zero);
                p1l = vec_mladd( p1l, al, (vector short) zero);

                p1h = vec_sra(p1h, seven);
                p1l = vec_sra(p1l, seven);

                p1h = vec_adds(p1h, p2h);
                p1l = vec_adds(p1l, p2l);

                p1h = (vector short) vec_packsu(p1h,p1l);

                vec_st((vector unsigned char) p1h, 0, dest);
                
                P1+=16;
                P2+=16;
                alpha+=16;
                dest+=16;
       }
#endif // USE_ALTIVEC
       
       //fallback version and the last missing bytes...
       for (int i=0; i < count; i++){
          dest[i]=(((uint16_t) P1[i] *(uint16_t) alpha[i]) +
             ((uint16_t) P2[i] *(256-(uint16_t) alpha[i])))  >>8 ;
       }
}

uint64_t getTimeMilis(void) {
    struct timeval tv;
    gettimeofday(&tv,NULL);
    return (int64_t)tv.tv_sec * 1000 + tv.tv_usec / 1000;
}

/* ---------------------------------------------------------------------------
 */
char *getFBName(void)
{
    int   fd;

  if (getenv ("FRAMEBUFFER") && *getenv ("FRAMEBUFFER") != '\0') {
    fd = open (getenv ("FRAMEBUFFER"), O_RDWR);
    if (fd >= 0) {
      close (fd);
      return (strdup (getenv ("FRAMEBUFFER")));
    }
  }

  if ((fd = open ("/dev/fb0", O_RDWR)) >= 0) {
    close (fd);
    return (strdup ("/dev/fb0"));
  }

  if ((fd = open ("/dev/fb/0", O_RDWR)) >= 0) {
    close (fd);
    return (strdup ("/dev/fb/0"));
  }

  return NULL;
}

/* taken from MPlayer's aclib */


#ifndef USE_MMX

void * fast_memcpy(void * to, const void * from, size_t len) {
        memcpy(to,from,len);
}

#else

#define MIN_LEN 0x40
#define MMREG_SIZE 64
#define BLOCK_SIZE 4096

#ifdef __x86_64__
# define  REG_a "rax"
#else
# define  REG_a "eax"
#endif

/* for small memory blocks (<256 bytes) this version is faster */
#define small_memcpy(to,from,n)\
{\
register unsigned long int dummy;\
__asm__ __volatile__(\
	"rep; movsb"\
	:"=&D"(to), "=&S"(from), "=&c"(dummy)\
/* It's most portable way to notify compiler */\
/* that edi, esi and ecx are clobbered in asm block. */\
/* Thanks to A'rpi for hint!!! */\
        :"0" (to), "1" (from),"2" (n)\
	: "memory");\
}

void * fast_memcpy(void * to, const void * from, size_t len)
{
	void *retval;
	size_t i;
	retval = to;
#ifdef USE_MMX2
        /* PREFETCH has effect even for MOVSB instruction ;) */
	__asm__ __volatile__ (
	        PREFETCH(" (%0)\n")
	        PREFETCH(" 64(%0)\n")
	        PREFETCH(" 128(%0)\n")
        	PREFETCH(" 192(%0)\n")
        	PREFETCH(" 256(%0)\n")
		: : "r" (from) );
#endif
        if(len >= MIN_LEN)
	{
	  register unsigned long int delta;
          /* Align destinition to MMREG_SIZE -boundary */
          delta = ((unsigned long int)to)&(MMREG_SIZE-1);
          if(delta)
	  {
	    delta=MMREG_SIZE-delta;
	    len -= delta;
	    small_memcpy(to, from, delta);
	  }
	  i = len >> 6; /* len/64 */
	  len&=63;
        /*
           This algorithm is top effective when the code consequently
           reads and writes blocks which have size of cache line.
           Size of cache line is processor-dependent.
           It will, however, be a minimum of 32 bytes on any processors.
           It would be better to have a number of instructions which
           perform reading and writing to be multiple to a number of
           processor's decoders, but it's not always possible.
        */

	// Align destination at BLOCK_SIZE boundary
	for(; ((long)to & (BLOCK_SIZE-1)) && i>0; i--)
	{
		__asm__ __volatile__ (
        	PREFETCH(" 320(%0)\n")
		"movq (%0), %%mm0\n"
		"movq 8(%0), %%mm1\n"
		"movq 16(%0), %%mm2\n"
		"movq 24(%0), %%mm3\n"
		"movq 32(%0), %%mm4\n"
		"movq 40(%0), %%mm5\n"
		"movq 48(%0), %%mm6\n"
		"movq 56(%0), %%mm7\n"
		MOVNTQ" %%mm0, (%1)\n"
		MOVNTQ" %%mm1, 8(%1)\n"
		MOVNTQ" %%mm2, 16(%1)\n"
		MOVNTQ" %%mm3, 24(%1)\n"
		MOVNTQ" %%mm4, 32(%1)\n"
		MOVNTQ" %%mm5, 40(%1)\n"
		MOVNTQ" %%mm6, 48(%1)\n"
		MOVNTQ" %%mm7, 56(%1)\n"
		: : "r" (from), "r" (to) : "memory");
		from=((const unsigned char *)from)+64;
		to=((unsigned char *)to)+64;
	}

        for(; i>0; i--)
	{
		__asm__ __volatile__ (
        	PREFETCH(" 320(%0)\n")
		"movq (%0), %%mm0\n"
		"movq 8(%0), %%mm1\n"
		"movq 16(%0), %%mm2\n"
		"movq 24(%0), %%mm3\n"
		"movq 32(%0), %%mm4\n"
		"movq 40(%0), %%mm5\n"
		"movq 48(%0), %%mm6\n"
		"movq 56(%0), %%mm7\n"
		MOVNTQ" %%mm0, (%1)\n"
		MOVNTQ" %%mm1, 8(%1)\n"
		MOVNTQ" %%mm2, 16(%1)\n"
		MOVNTQ" %%mm3, 24(%1)\n"
		MOVNTQ" %%mm4, 32(%1)\n"
		MOVNTQ" %%mm5, 40(%1)\n"
		MOVNTQ" %%mm6, 48(%1)\n"
		MOVNTQ" %%mm7, 56(%1)\n"
		: : "r" (from), "r" (to) : "memory");
		from=((const unsigned char *)from)+64;
		to=((unsigned char *)to)+64;
	}

                /* since movntq is weakly-ordered, a "sfence"
		 * is needed to become ordered again. */
                 SFENCE;
		/* enables to use FPU */
		EMMS ;
	}
	/*
	 *	Now do the tail of the block
	 */
	if(len) small_memcpy(to, from, len);
	return retval;
}

#endif // USE_MMX
