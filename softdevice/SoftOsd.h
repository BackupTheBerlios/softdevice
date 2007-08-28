/*
 * softdevice plugin for VDR
 *
 * Copyright (C) 2005 Martin Wache
 *
 * This code is distributed under the terms and conditions of the
 * GNU GENERAL PUBLIC LICENSE. See the file COPYING for details.
 *
 * $Id: SoftOsd.h,v 1.16 2007/08/28 22:28:36 lucke Exp $
 */

#ifndef __SOFTOSD_H__
#define __SOFTOSD_H__

// osd some constants and macros
#define OPACITY_THRESHOLD 0x9FLL
#define TRANSPARENT_THRESHOLD 0x1FLL
#define COLOR_KEY 0x00000000LL

#define OSD_WIDTH   720
#define OSD_HEIGHT  576

#define IS_BACKGROUND(a) (((a) < OPACITY_THRESHOLD) && ((a) > TRANSPARENT_THRESHOLD))
#define IS_TRANSPARENT(a) ((a) < TRANSPARENT_THRESHOLD)
#define IS_OPAQUE(a) ((a) > OPACITY_THRESHOLD)

#include <vdr/config.h>

#include <vdr/osd.h>
#include <vdr/thread.h>
#include "video.h"

#define X_OFFSET 0
#define Y_OFFSET 0

#define OSD_STRIDE (736)

#define COLOR_RGB16(r,g,b) (((b >> 3)& 0x1F) | ((g & 0xF8) << 2)| ((r & 0xF8)<<10) )

#define GET_A(x) ((x) >> 24 & 0xFF)
#define GET_R(x) ((x) >> 16 & 0xFF)
#define GET_G(x) ((x) >>  8 & 0xFF)
#define GET_B(x) ((x) >>  0 & 0xFF)

#define SET_A(x) ((x) << 24 & 0xFF000000)
#define SET_R(x) ((x) << 16 & 0x00FF0000)
#define SET_G(x) ((x) <<  8 & 0x0000FF00)
#define SET_B(x) ((x) <<  0 & 0x000000FF)
/*
struct color {
    unsigned char b;
    unsigned char g;
    unsigned char r;
    unsigned char a;
};
*/
typedef uint32_t color;

class cVideoOut;

/* ---------------------------------------------------------------------------
 */
class cSoftOsd : public cOsd,cThread {
private:
    cMutex voutMutex; // lock all operations on videoOut!
    cVideoOut *videoOut;
protected:
    static int colorkey;
    int      xOfs, yOfs;
    int      xPan, yPan;
    uint32_t *OSD_Bitmap;
    bool dirty_lines[OSD_HEIGHT+10];
    cMutex dirty_Mutex;

    void (*OutputConvert)(uint8_t * dest, color * pixmap, int Pixel, int odd);
    enum PixFormat {
            PF_None,
            PF_ARGB32,
            PF_inverseAlpha_ARGB32,
            PF_pseudoAlpha_ARGB32,
            PF_AYUV,
            PF_BGRA32
    };
    PixFormat bitmap_Format;

    void ConvertPalette(tColor *dest_palette, const tColor *orig_palette,
                    int maxColors);

    bool active;
    bool close;
    int ScreenOsdWidth;
    int ScreenOsdHeight;
public:
#if VDRVERSNUM >= 10509
    cSoftOsd(cVideoOut *VideoOut, int XOfs, int XOfs, uint level);
#else
    cSoftOsd(cVideoOut *VideoOut, int XOfs, int XOfs);
#endif
    virtual ~cSoftOsd();
    virtual void Flush(void);

    // Create a copy of the osd layer for the Grab() method.
    // Does *not* change anything in the osd!!
    void StealToBitmap(uint8_t *PY,uint8_t *PU, uint8_t *PV,
                    uint8_t *PAlphaY,uint8_t *PAlphaUV,
                    int Ystride, int UVstride,
                    int dest_Width, int dest_Height);

protected:
    bool SetMode(int Depth, bool HasAlpha, bool AlphaInversed, bool IsYUV);

    bool FlushBitmaps(bool OnlyDirty);
    bool DrawConvertBitmap(cBitmap *Bitmap, bool OnlyDirty);

    void OsdCommit();
    // may only be called if the caller holds voutMutex

    void Clear();
    virtual void Action();

    static void ARGB_to_AYUV(uint32_t * dest, color * pixmap, int Pixel);
    static void ARGB_to_ARGB32(uint8_t * dest, color * pixmap, int Pixel,
                    int odd);
    static void ARGB_to_BGRA32(uint8_t * dest, color * pixmap, int Pixel,
                    int odd);
    static void ARGB_to_RGB32(uint8_t * dest, color * pixmap, int Pixel,
                    int odd);
    static void ARGB_to_RGB24(uint8_t * dest, color * pixmap, int Pixel,
                    int odd);
    static void ARGB_to_RGB16(uint8_t * dest, color * pixmap, int Pixel,
                    int odd);

    static void AYUV_to_AYUV420P(uint8_t *PY1,uint8_t *PY2,
                    uint8_t *PU, uint8_t *PV,
                    uint8_t *PAlphaY1, uint8_t *PAlphaY2, uint8_t *PAlphaUV,
                    color * pixmap1, color *pixmap2, int Pixel);

    // YUV planar modes
    void CopyToBitmap(uint8_t *PY,uint8_t *PU, uint8_t *PV,
                    uint8_t *PAlphaY,uint8_t *PAlphaUV,
                    int Ystride, int UVstride,
                    int dest_Width, int dest_Height, bool RefreshAll=false);

    void NoVScaleCopyToBitmap(uint8_t *PY,uint8_t *PU, uint8_t *PV,
                    uint8_t *PAlphaY,uint8_t *PAlphaUV,
                    int Ystride, int UVstride,
                    int dest_Width, int dest_Height, bool RefreshAll=false);

    void ScaleVDownCopyToBitmap(uint8_t *PY,uint8_t *PU, uint8_t *PV,
                    uint8_t *PAlphaY,uint8_t *PAlphaUV,
                    int Ystride, int UVstride,
                    int dest_Width, int dest_Height, bool RefreshAll=false);


    // ARGB packed modes
    void CopyToBitmap(uint8_t * dest, int linesize,
                    int dest_Width, int dest_Height, bool RefreshAll=false,
                    bool *dirtyLines=NULL);
    void NoVScaleCopyToBitmap(uint8_t * dest, int linesize,
                    int dest_Width, int dest_Height, bool RefreshAll=false,
                    bool *dirtyLines=NULL);
    void ScaleVUpCopyToBitmap(uint8_t * dest, int linesize,
                    int dest_Width, int dest_Height, bool RefreshAll=false,
                    bool *dirtyLines=NULL);
    void ScaleVDownCopyToBitmap(uint8_t * dest, int linesize,
                    int dest_Width, int dest_Height, bool RefreshAll=false,
                    bool *dirtyLines=NULL);



 private:
    void NoScaleHoriz_MMX(uint32_t * dest, int dest_Width, color * pixmap,int Pixel);
    void ScaleUpHoriz_MMX(uint32_t * dest, int dest_Width, color * pixmap,int Pixel);
    void ScaleDownHoriz_MMX(uint32_t * dest, int dest_Width, color * pixmap,int Pixel);
    void ScaleDownVert_MMX(uint32_t * dest, int linesize, int32_t new_pixel_height,
                int start_pos,
                color ** pixmap, int Pixel);
    void ScaleUpVert_MMX(uint32_t *dest, int linesize, int32_t new_pixel_height,
                int start_pos,
                color **pixmap, int Pixel);

};

#endif
