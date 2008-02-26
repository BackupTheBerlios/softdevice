/*
 * softdevice plugin for VDR
 *
 * Copyright (C) 2005 Martin Wache
 *
 * This code is distributed under the terms and conditions of the
 * GNU GENERAL PUBLIC LICENSE. See the file COPYING for details.
 *
 * $Id: PicBuffer.h,v 1.9 2008/02/26 08:06:18 lucke Exp $
 */
#ifndef __PIC_BUFFER_H__
#define __PIC_BUFFER_H__

#ifdef HAVE_CONFIG
# include "config.h"
#endif

extern "C" {
#if HAVE_FFMPEG_LOCAL
# include <libavcodec/avcodec.h>
#else
# include <avcodec.h>
#endif
}

#ifndef STAND_ALONE
#include <vdr/plugin.h>
#include <vdr/remote.h>
#else
#include "VdrReplacements.h"
#endif

class cPicBufferManager;

typedef struct sPicBuffer {
    PixelFormat format;
    uint8_t *pixel[4];
    int stride[4];
    unsigned int use_count; // count the users of this buffer
    int buf_num;
    int max_width; // maximal size of the picture edge + picture
    int max_height;
    cPicBufferManager *owner;

    // picture context
    int edge_width; // size of edges (needed by some ffmpeg codecs)
    int edge_height;
    int width;  // size of the actual picture (without edges)
    int height;
    int dtg_active_format;
    float aspect_ratio;
    uint64_t pts;
    bool interlaced_frame;
    int pict_type;

    int pic_num; // to calculate the age
    int age; // needed by ffmpeg
    void *priv_data;
};

void InitPicBuffer(sPicBuffer *Pic);
void FillPicBuffer(sPicBuffer *Pic, int color);
void ClearPicBuffer(sPicBuffer *Pic);
void CopyPicBufferContext(sPicBuffer *dest,sPicBuffer *orig);
bool AllocatePicBuffer(sPicBuffer *buf,PixelFormat pix_fmt,int w, int h);
void DeallocatePicBuffer(sPicBuffer *buf);
int GetFormatBPP(PixelFormat fmt);
void GetChromaSubSample(PixelFormat pix_fmt,
                int &hChromaShift,
                int &vChromaShift);

class cPicBufferManager {
public:
        int lastPicNum;
#define LAST_PICBUF 10
        struct sPicBuffer PicBuffer[LAST_PICBUF];
        cMutex PicBufMutex;

        cPicBufferManager();

        virtual ~cPicBufferManager();

        inline sPicBuffer *PicBuf(unsigned int buf_num)
        { return ( buf_num<LAST_PICBUF ? &PicBuffer[buf_num] : NULL); };
        // returns the address of the buffer buf_num

        int GetBufNum(sPicBuffer *PicBuf);
        // returns the buffer number of the picture buffer PicBuf
        // returns -1 if the buffer has not been found

        sPicBuffer *GetBuffer(PixelFormat pix_fmt,int width, int height);
        // get a picture buffer ( to be called from decoders of filters )
        void ReleaseBuffer(sPicBuffer *pic);
        // release it again ( after use )

        void LockBuffer(sPicBuffer *picture);
        // I want to keep this buffer ( not requested by me via GetBuffer() )
        // longer
        void UnlockBuffer(sPicBuffer *picture);
        // don't need it anymore


        virtual bool AllocPicBuffer(int buf_num,PixelFormat pix_fmt,
                        int w, int h);
        // actually allocates memory for the buffer. Can be overloaded for
        // direct rendering.
        // Has to set up max_width/max_height and format!

        virtual void ReleasePicBuffer(int buf_num);
        // releases the memory of the buffer again
};

// copy the contents of a picture buffer into the other
void CopyPicBuf(sPicBuffer *dest, sPicBuffer *src,
                int cutTop, int cutBottom, int cutLeft, int cutRight);

// Copy the contents of a picture buffer into the other.
// If the dimension of the destination buffer don't match the
// dimensions of the source, the image is scaled using a low quality
// next neighbour algorithm
void CopyScalePicBuf(sPicBuffer *dest, sPicBuffer *src,
                int sxoff, int syoff, int swidth, int sheight,
                int dxoff, int dyoff, int dwidth, int dheight,
                int cutTop, int cutBottom, int cutLeft, int cutRight);

void CopyScalePicBufAlphaBlend(sPicBuffer *dest, sPicBuffer *src,
                int sxoff, int syoff, int swidth, int sheight,
                int dxoff, int dyoff, int dwidth, int dheight,
                uint8_t *OsdPy,uint8_t *OsdPu, uint8_t *OsdPv,
                uint8_t *OsdPAlphaY, uint8_t *OsdPAlphaUV,int OsdStride,
                int cutTop, int cutBottom, int cutLeft, int cutRight);

void CopyPicBufAlphaBlend(sPicBuffer *dst,
                sPicBuffer *src,
                uint8_t *OsdPy,uint8_t *OsdPu, uint8_t *OsdPv,
                uint8_t *OsdPAlphaY, uint8_t *OsdPAlphaUV,int OsdStride,
                int cutTop, int cutBottom,int cutLeft, int cutRight);

#endif
