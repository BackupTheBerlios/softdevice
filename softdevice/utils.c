/*
 * utils.c: A plugin for the Video Disk Recorder
 *
 * See the README file for copyright information and how to reach the author.
 *
 * $Id: utils.c,v 1.3 2005/03/03 18:16:26 lucke Exp $
 */

// --- plain C MMX functions (i'm too lazy to put this in a class)


/*
 * MMX conversion functions taken from the mpeg2dec project
 * Copyright (C) 2000-2002 Silicon Integrated System Corp.
 * All Rights Reserved.
 *
 * Author: Olie Lho <ollie@sis.com.tw>
*/    

#include "utils.h"
#include "setup-softdevice.h"

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

uint64_t getTimeMilis(void) {
    struct timeval tv;
    gettimeofday(&tv,NULL);
    return (int64_t)tv.tv_sec * 1000 + tv.tv_usec / 1000;
}
