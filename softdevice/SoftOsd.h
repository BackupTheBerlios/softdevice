/*
 * softdevice plugin for VDR
 *
 * Copyright (C) 2005 Martin Wache
 *
 * This code is distributed under the terms and conditions of the
 * GNU GENERAL PUBLIC LICENSE. See the file COPYING for details.
 *
 * $Id: SoftOsd.h,v 1.1 2006/01/07 14:29:35 wachm Exp $
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

#include "vdr/config.h"

#if VDRVERSNUM >= 10307 // only for the new osd interface...

#include "vdr/osd.h"
#include "vdr/thread.h"
#include "video.h"

#define X_OFFSET 0 
#define Y_OFFSET 0

#define OSD_STRIDE (736)

#define COLOR_RGB16(r,g,b) (((b >> 3)& 0x1F) | ((g & 0xF8) << 2)| ((r & 0xF8)<<10) )


struct color {
    unsigned char b;
    unsigned char g;
    unsigned char r;
    unsigned char a;
};

class cVideoOut;

/* ---------------------------------------------------------------------------
 */
class cSoftOsd : public cOsd {
private:
    cVideoOut *videoOut;
protected:
    int xOfs,yOfs;
    uint32_t *OSD_Bitmap;
    bool dirty_lines[OSD_STRIDE];
    cMutex dirty_Mutex;
    
    void (*OutputConvert)(uint8_t * dest, color * pixmap, int Pixel);
    uint8_t *pixelMask;
    enum PixFormat {
	    PF_None,
	    PF_ARGB32,
	    PF_inverseAlpha_ARGB32,
	    PF_pseudoAlpha_ARGB32,
	    PF_AYUV
    };
    PixFormat bitmap_Format;

    void ConvertPalette(tColor *dest_palette, const tColor *orig_palette, 
		    int maxColors);
    
public:
    cSoftOsd(cVideoOut *VideoOut, int XOfs, int XOfs);
    virtual ~cSoftOsd();

    bool SetMode(int Depth, bool HasAlpha, bool AlphaInversed, 
                 bool IsYUV, uint8_t *PixelMask=NULL);
    
    bool FlushBitmaps(bool OnlyDirty);
    bool DrawConvertBitmap(cBitmap *Bitmap, bool OnlyDirty);
    virtual void Flush(void);
    
    void Clear();
    
    static void ARGB_to_AYUV(uint8_t * dest, color * pixmap, int Pixel);
    static void ARGB_to_ARGB32(uint8_t * dest, color * pixmap, int Pixel);
    static void ARGB_to_RGB32(uint8_t * dest, color * pixmap, int Pixel);
    static void ARGB_to_RGB24(uint8_t * dest, color * pixmap, int Pixel);
    static void ARGB_to_RGB16(uint8_t * dest, color * pixmap, int Pixel);
    static void ARGB_to_RGB16_PixelMask(uint8_t * dest, color * pixmap, 
                    int Pixel);

    void CreatePixelMask(uint8_t * dest, color * pixmap, int Pixel);


    static void AYUV_to_AYUV420P(uint8_t *PY1,uint8_t *PY2,
		    uint8_t *PU, uint8_t *PV,
		    uint8_t *PAlphaY1, uint8_t *PAlphaY2, uint8_t *PAlphaUV,
		    color * pixmap1, color *pixmap2, int Pixel);

    // YUV planar modes
    void CopyToBitmap(uint8_t *PY,uint8_t *PU, uint8_t *PV,
		    uint8_t *PAlphaY,uint8_t *PAlphaUV,
                    int Ystride, int UVstride,
                    int dest_Width, int dest_Height, bool RefreshAll=false);

    // ARGB packed modes
    void CopyToBitmap(uint8_t * dest, int linesize,
                    int dest_Width, int dest_Height, bool RefreshAll=false,
                    bool *dirtyLines=NULL);
    void ScaleVUpCopyToBitmap(uint8_t * dest, int linesize,
                    int dest_Width, int dest_Height, bool RefreshAll=false,
                    bool *dirtyLines=NULL);
    void ScaleVDownCopyToBitmap(uint8_t * dest, int linesize,
                    int dest_Width, int dest_Height, bool RefreshAll=false,
                    bool *dirtyLines=NULL);


    
 private:
    void ScaleUpHoriz_MMX(uint8_t * dest, int dest_Width, color * pixmap,int Pixel);
    void ScaleDownHoriz_MMX(uint8_t * dest, int dest_Width, color * pixmap,int Pixel);
    void ScaleDownVert_MMX(uint8_t * dest, int linesize, int32_t new_pixel_height, 
                int start_pos,
                color ** pixmap, int Pixel);
    void ScaleUpVert_MMX(uint8_t *dest, int linesize, int32_t new_pixel_height, 
                int start_pos,
                color **pixmap, int Pixel);

 public:
	    
};

#endif //VDRVERSUM >= 10307
#endif
