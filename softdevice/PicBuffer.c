/*
 * softdevice plugin for VDR
 *
 * Copyright (C) 2005 Martin Wache
 *
 * This code is distributed under the terms and conditions of the
 * GNU GENERAL PUBLIC LICENSE. See the file COPYING for details.
 *
 * $Id: PicBuffer.c,v 1.2 2006/05/29 19:25:52 wachm Exp $
 */
#include <stdlib.h>
#include <string.h>

#include "PicBuffer.h"
#include "utils.h"

//#define PICDEB(out...) {printf("vout_pic[%04d]:",(int)(getTimeMilis() % 10000));printf(out);}

#ifndef PICDEB
#define PICDEB(out...)
#endif

void InitPicBuffer(sPicBuffer *Pic) {
        memset(Pic->pixel,0,sizeof(Pic->pixel));
        Pic->use_count=0;
        Pic->pic_num=-256*256*256*64;
        Pic->format=PIX_FMT_NB;
        Pic->max_width=Pic->max_height=0;
};

void CopyPicBufferContext(sPicBuffer *dest,sPicBuffer *orig){
    dest->dtg_active_format=orig->dtg_active_format;
    dest->aspect_ratio=orig->aspect_ratio;
    dest->pts=orig->pts;
    dest->interlaced_frame=orig->interlaced_frame;
    dest->pict_type=orig->pict_type;
};   

cPicBufferManager::cPicBufferManager() {
  lastPicNum=0;
  for (int i=0; i< LAST_PICBUF; i++) 
          InitPicBuffer(&PicBuffer[i]);
}

cPicBufferManager::~cPicBufferManager() {
        // FIXME release buffers
};

void cPicBufferManager::ReleasePicBuffer(int buf_num) {
        PICDEB("ReleasePicBuffer %d",buf_num);
        sPicBuffer *buf=&PicBuffer[buf_num];

        for (int i=0; i<4; i++) {
                free(buf->pixel[i]);
                buf->pixel[i]=NULL;
        };
        buf->max_height=0;
        buf->max_width=0;
};

void cPicBufferManager::GetChromaSubSample(PixelFormat pix_fmt,
                int &hChromaShift,
                int &vChromaShift) {
        switch (pix_fmt) {
                case PIX_FMT_YUV420P:
                        hChromaShift=vChromaShift=1;
                        break;
                case PIX_FMT_YUV422P:
                        hChromaShift=1;vChromaShift=0;
                        break;
                case PIX_FMT_YUV444P:
                        hChromaShift=0;vChromaShift=0;
                        break;
                      
                default:
                        fprintf(stderr,"warning unsupported pixel format(%d)! \n",pix_fmt);
                        hChromaShift=vChromaShift=1;
        };
};

void cPicBufferManager::LockBuffer(sPicBuffer *picture) {
        PicBufMutex.Lock();
        if (picture)
                picture->use_count++;
        PicBufMutex.Unlock();
};

void cPicBufferManager::UnlockBuffer(sPicBuffer *picture) {
        PicBufMutex.Lock();
        if (picture && picture->use_count>0)
                picture->use_count--;
        PicBufMutex.Unlock();
};

// the following methods are based on ffmpeg's get_buffer and release_buffer
// with the originial copyright notice
/*
 * utils for libavcodec
 * Copyright (c) 2001 Fabrice Bellard.
 * Copyright (c) 2003 Michel Bardiaux for the av_log API
 * Copyright (c) 2002-2004 Michael Niedermayer <michaelni@gmx.at>
 *
*/
int cPicBufferManager::GetFormatBPP(PixelFormat fmt) {
        int pixel_size=1;
        switch(fmt){
        case PIX_FMT_RGB555:
        case PIX_FMT_RGB565:
        case PIX_FMT_YUV422:
        //case PIX_FMT_UYVY422: // FIXME which ffmpeg version
            pixel_size=2;
            break;
        case PIX_FMT_RGB24:
        case PIX_FMT_BGR24:
            pixel_size=3;
            break;
        case PIX_FMT_RGBA32:
            pixel_size=4;
            break;
        default:
            pixel_size=1;
        }
        return pixel_size;
};

#define ALIGN(x, a) (((x)+(a)-1)&~((a)-1))
#define STRIDE_ALIGN 16
#define EDGE_WIDTH 16

void cPicBufferManager::AllocPicBuffer(int buf_num,PixelFormat pix_fmt, 
                int w, int h)  {
        PICDEB("AllocPicBuffer buf_num %d pix_fmt %d (%d,%d)\n",
                        buf_num,pix_fmt,w,h);
        sPicBuffer *buf=&PicBuffer[buf_num];
        int h_chroma_shift, v_chroma_shift;
        int pixel_size=GetFormatBPP(pix_fmt);
        
        GetChromaSubSample(pix_fmt, h_chroma_shift, v_chroma_shift);

        for(int i=0; i<3; i++){
            const int h_shift= i==0 ? 0 : h_chroma_shift;
            const int v_shift= i==0 ? 0 : v_chroma_shift;

            //FIXME next ensures that linesize= 2^x uvlinesize, thats needed because some MC code assumes it
            buf->stride[i]= ALIGN(pixel_size*w>>h_shift, 
                            STRIDE_ALIGN<<(h_chroma_shift-h_shift)); 

            buf->pixel[i]= (uint8_t*)malloc((buf->stride[i]*h>>v_shift)+16); //FIXME 16
            
            if(buf->pixel[i]==NULL) {
                    printf("could not allocate memory for picture buffer!\n") ;
                    exit(-1);
            };
            
            memset(buf->pixel[i], 128, buf->stride[i]*h>>v_shift);
       
        }
        buf->max_width=w;
        buf->max_height=h;
        PICDEB("end AllocPicBuffer buf->pixel[0] %p\n",buf->pixel[0]);
}

sPicBuffer *cPicBufferManager::GetBuffer(PixelFormat pix_fmt,
                    int w, int h) {
    PICDEB("GetBuffer pix_fmt %d frame %p (%d,%d)\n",pix_fmt,pic,w,h);

    //assert(pic->data[0]==NULL);

    PicBufMutex.Lock();
    int buf_num=0;
    while (buf_num<LAST_PICBUF && PicBuffer[buf_num].use_count!=0 ) {
            PICDEB("not using buffer %d: use_count=%d\n",buf_num,
                            PicBuffer[buf_num].use_count);
            buf_num++;
    };

    //assert(buf_num<LAST_PICBUF);
    if (buf_num>=LAST_PICBUF) {
            printf("error finding PicBuffer!\n");
            exit(-1);
    };

    // format change?
    if (PicBuffer[buf_num].pixel[0] &&
                    ( PicBuffer[buf_num].format != pix_fmt
                      || PicBuffer[buf_num].max_width != w
                      || PicBuffer[buf_num].max_height != h ) ) {
//                      || PicBuffer[buf_num].max_width < w
//                      || PicBuffer[buf_num].max_height < h ) ) {
            PICDEB("format change relasing old buf_num %d\n",buf_num);
            ReleasePicBuffer(buf_num);
            PicBuffer[buf_num].pixel[0]=NULL;
    };

    if (!PicBuffer[buf_num].pixel[0]) {
            PICDEB("allocating buf_num %d\n",buf_num);
            PicBuffer[buf_num].pic_num= -256*256*256*64;
            AllocPicBuffer(buf_num,pix_fmt,w,h);
    };
    
    // found or allocated a picture buffer
    lastPicNum++;
    PicBuffer[buf_num].use_count++;
    PicBuffer[buf_num].buf_num = buf_num;
    PicBuffer[buf_num].owner = this;
    PicBufMutex.Unlock();

    PicBuffer[buf_num].age= lastPicNum - PicBuffer[buf_num].pic_num;
    PicBuffer[buf_num].pic_num= lastPicNum;

    PICDEB("end GetBuffer: pic->data[0] %p buf_num %d\n",
                    pic->data[0],buf_num);
    return &PicBuffer[buf_num];
}

void cPicBufferManager::ReleaseBuffer( sPicBuffer *pic ){
    PICDEB("ReleaseBuffer frame %p, data[0] %p\n",
                    pic,pic->data[0]);

    int buf_num=0;
    PicBufMutex.Lock();
    while (buf_num<LAST_PICBUF && PicBuffer[buf_num].pixel[0]!=pic->pixel[0] ) 
            buf_num++;

    //assert(buf_num<LAST_PICBUF);
    if (buf_num>=LAST_PICBUF)  {
            printf("didn't find corresponding PicBuffer!\n");
            exit(-1);
    };
    
    // found PicBuffer
    PicBuffer[buf_num].use_count--;
    PicBuffer[buf_num].buf_num=-1;
    PicBufMutex.Unlock();
}

// end of code based on ffmpeg

void CopyPicBuf(sPicBuffer *dst, sPicBuffer *src,
                int width, int height,
                int cutTop, int cutBottom, 
                int cutLeft, int cutRight) {
       
        //cutTop+=src->edge_height/2;
        //cutLeft+=src->edge_width/2;
        uint8_t *dst_ptr=dst->pixel[0];
        uint8_t *src_ptr=src->pixel[0]+
                (2*cutTop+src->edge_height)*src->stride[0]+
                2*cutLeft+src->edge_width;
        for (int i = height - (cutBottom+cutTop) * 2; i>=0; i--) {
                fast_memcpy(dst_ptr,src_ptr,
                            width - 2 * (cutLeft + cutRight));
                dst_ptr+=dst->stride[0];
                src_ptr+=src->stride[0];
        };
        
        dst_ptr=dst->pixel[1];
        src_ptr=src->pixel[1]+(cutTop+src->edge_height/2)*src->stride[1]+
                cutLeft+src->edge_width/2;
        for (int i = height / 2 - (cutBottom+cutTop); i>=0; i--) {
                fast_memcpy (dst_ptr,src_ptr,
                                width / 2 - (cutLeft + cutRight));
                dst_ptr+=dst->stride[1];
                src_ptr+=src->stride[1];
        };

        dst_ptr=dst->pixel[2];
        src_ptr=src->pixel[2]+(cutTop+src->edge_height/2)*src->stride[2]+
                cutLeft+src->edge_width/2;
        for (int i = height / 2 - (cutBottom+cutTop); i>=0; i--) {
                fast_memcpy (dst_ptr,src_ptr,
                                width / 2 - (cutLeft + cutRight));
                dst_ptr+=dst->stride[2];
                src_ptr+=src->stride[2];
        };
};

void CopyPicBufAlphaBlend(sPicBuffer *dst, sPicBuffer *src,
                uint8_t *OsdPy,
                uint8_t *OsdPu, 
                uint8_t *OsdPv,
                uint8_t *OsdPAlphaY,
                uint8_t *OsdPAlphaUV,
                int OsdStride,
                int width, int height,
                int cutTop, int cutBottom, 
                int cutLeft, int cutRight) {
       
        //cutTop+=src->edge_height/2;
        //cutLeft+=src->edge_width/2;
        uint8_t *dst_ptr=dst->pixel[0];
        uint8_t *src_ptr=src->pixel[0]+
                (2*cutTop+src->edge_height)*src->stride[0]+
                2*cutLeft+src->edge_width;
        uint8_t *osd_ptr=OsdPy+2*cutTop*OsdStride+2*cutLeft;
        uint8_t *alpha_ptr=OsdPAlphaY+2*cutTop*OsdStride+2*cutLeft;
        for (int i = height - (cutBottom+cutTop) * 2; i>=0; i--) {
                AlphaBlend(dst_ptr,osd_ptr,src_ptr,alpha_ptr,
                            width - 2 * (cutLeft + cutRight));
                dst_ptr+=dst->stride[0];
                src_ptr+=src->stride[0];
                osd_ptr+=OsdStride;
                alpha_ptr+=OsdStride;
        };
        
        dst_ptr=dst->pixel[1];
        src_ptr=src->pixel[1]+(cutTop+src->edge_height/2)*src->stride[1]+
                cutLeft+src->edge_width/2;
        osd_ptr=OsdPu+cutTop*OsdStride/2+cutLeft;
        alpha_ptr=OsdPAlphaUV+cutTop*OsdStride/2+cutLeft;
        for (int i = height / 2 - (cutBottom+cutTop); i>=0; i--) {
                AlphaBlend(dst_ptr,osd_ptr,src_ptr,alpha_ptr,
                                width / 2 - (cutLeft + cutRight));
                dst_ptr+=dst->stride[1];
                src_ptr+=src->stride[1];
                osd_ptr+=OsdStride/2;
                alpha_ptr+=OsdStride/2;
        };

        dst_ptr=dst->pixel[2];
        src_ptr=src->pixel[2]+(cutTop+src->edge_height/2)*src->stride[2]+
                cutLeft+src->edge_width/2;
        osd_ptr=OsdPv+cutTop*OsdStride/2+cutLeft;
        alpha_ptr=OsdPAlphaUV+cutTop*OsdStride/2+cutLeft;
        for (int i = height / 2 - (cutBottom+cutTop); i>=0; i--) {
                AlphaBlend(dst_ptr,osd_ptr,src_ptr,alpha_ptr,
                                width / 2 - (cutLeft + cutRight));
                dst_ptr+=dst->stride[2];
                src_ptr+=src->stride[2];
                osd_ptr+=OsdStride/2;
                alpha_ptr+=OsdStride/2;
       };
       /*
        for (int i = cutTop; i < fheight / 2 - cutBottom; i++)
                fast_memcpy (pixels [2] + i * xvWidth / 2 + cutLeft,
                                Pu + i * UVstride + cutLeft,
                                fwidth / 2 - (cutLeft + cutRight));
                                */
};

