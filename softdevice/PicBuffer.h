/*
 * softdevice plugin for VDR
 *
 * Copyright (C) 2005 Martin Wache
 *
 * This code is distributed under the terms and conditions of the
 * GNU GENERAL PUBLIC LICENSE. See the file COPYING for details.
 *
 * $Id: PicBuffer.h,v 1.1 2006/05/27 19:12:41 wachm Exp $
 */
#ifndef __PIC_BUFFER_H__
#define __PIC_BUFFER_H__

#include <avcodec.h>

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
    unsigned int use_count;
    int buf_num;
    int max_width; // maximal size of the picture edge + picture
    int max_height;
    
    int edge_width; // size of edges (needed by some ffmpeg codecs)
    int edge_height;
    int width;  // size of the actual picture (without edges)
    int height;
    int dtg_active_format;
    float aspect_ratio;
    uint64_t pts;
    bool interlaced_frame;
    int pict_type;
   
    cPicBufferManager *owner;
 
    int pic_num;
    int age;
    void *priv_data;
};

void InitPicBuffer(sPicBuffer *Pic);
void CopyPicBufferContext(sPicBuffer *dest,sPicBuffer *orig);

class cPicBufferManager {
public:
        cPicBufferManager();

        virtual ~cPicBufferManager();
      
        int lastPicNum;
#define LAST_PICBUF 10
        struct sPicBuffer PicBuffer[LAST_PICBUF];
        cMutex PicBufMutex;

        sPicBuffer *GetBuffer(PixelFormat pix_fmt,int width, int height);
        void ReleaseBuffer(sPicBuffer *pic);

        void LockBuffer(sPicBuffer *picture);
        void UnlockBuffer(sPicBuffer *picture);
        
        int GetFormatBPP(PixelFormat fmt);
        void GetChromaSubSample(PixelFormat pix_fmt,
                int &hChromaShift,
                int &vChromaShift);
        virtual void ReleasePicBuffer(int buf_num);
        virtual void AllocPicBuffer(int buf_num,PixelFormat pix_fmt,
                        int w, int h);

};

void CopyPicBuf(sPicBuffer *dest, sPicBuffer *src,
                int width, int height,
                int cutTop, int cutBottom, int cutLeft, int cutRight);

void CopyPicBufAlphaBlend(sPicBuffer *dst, 
                sPicBuffer *src,
                uint8_t *OsdPy,uint8_t *OsdPu, 
                uint8_t *OsdPv,uint8_t *OsdPAlphaY,
                uint8_t *OsdPAlphaUV,int width, int height,
                int cutTop, int cutBottom,int cutLeft, int cutRight); 
 
#endif
