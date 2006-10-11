/*
 * softdevice plugin for VDR
 *
 * Copyright (C) 2005 Martin Wache
 *
 * This code is distributed under the terms and conditions of the
 * GNU GENERAL PUBLIC LICENSE. See the file COPYING for details.
 *
 * $Id: PicBuffer.c,v 1.11 2006/10/11 19:50:55 lucke Exp $
 */
#include <stdlib.h>
#include <string.h>

#include "PicBuffer.h"
#include "utils.h"
#include "video.h"

//#define PICDEB(out...) {printf("vout_pic[%04d]:",(int)(getTimeMilis() % 10000));printf(out);}

#ifndef PICDEB
#define PICDEB(out...)
#endif

void InitPicBuffer(sPicBuffer *Pic) {
        memset(Pic,0,sizeof(sPicBuffer));
        Pic->pic_num=-256*256*256*64;
        Pic->format=PIX_FMT_NB;
        Pic->buf_num=-1;
};

void CopyPicBufferContext(sPicBuffer *dest,sPicBuffer *orig){
    dest->dtg_active_format=orig->dtg_active_format;
    dest->aspect_ratio=orig->aspect_ratio;
    dest->pts=orig->pts;
    dest->interlaced_frame=orig->interlaced_frame;
    dest->pict_type=orig->pict_type;
};

void ClearPicBuffer(sPicBuffer *Pic) {
        if (!Pic || !Pic->pixel[0])
                return;
         PICDEB("ClearPicBuffer Pic %p pixel[0] %p max_height %d stride[0] %d\n",
                        Pic, Pic->pixel[0], Pic->max_height, Pic->stride[0]);

        switch (Pic->format) {
                case PIX_FMT_YUV420P :
                        memset(Pic->pixel[0],0,Pic->max_height*Pic->stride[0]);
                        memset(Pic->pixel[1],128,
                                        (Pic->max_height>>1)*Pic->stride[1]);
                        memset(Pic->pixel[2],128,
                                        (Pic->max_height>>1)*Pic->stride[2]);
                        break;
                case PIX_FMT_YUV422 :
                        {
                                uint32_t *tmp=(uint32_t *)Pic->pixel[0];
                                for (int i=0; i<Pic->max_height*
                                                Pic->max_width/2; i++) {
                                        *tmp=0x80008000;
                                        tmp++;
                                };
                                break;
                        };
                default:
                        fprintf(stderr,"Warning, unsupported format in ClearPicBuffer!\n");
        };
};

/*----------------------------------------------------------------------*/
cPicBufferManager::cPicBufferManager() {
        lastPicNum=0;
        for (int i=0; i< LAST_PICBUF; i++)
                InitPicBuffer(&PicBuffer[i]);
}

cPicBufferManager::~cPicBufferManager() {
        for (int i=0; i<LAST_PICBUF; i++)
                if ( PicBuffer[i].pixel[0] ) {
//                        if ( PicBuffer[i].use_count > 0 )
//                                fprintf(stderr,"Warning! Use_count of PicBuffer not zero in PicBufferManager destructor!\n");

                        ReleasePicBuffer( i );
                };
};

int cPicBufferManager::GetBufNum(sPicBuffer *buf) {
        if ( !buf || buf->owner!=this )
                return -1;

        int buf_num=0;
        while ( buf != &PicBuffer[buf_num]  && buf_num < LAST_PICBUF )
                buf_num++;

        return (buf_num < LAST_PICBUF ? buf_num : -1);
};

void cPicBufferManager::ReleasePicBuffer(int buf_num) {
        PICDEB("ReleasePicBuffer %d pixel[0] %p\n",buf_num,PicBuffer[buf_num].pixel[0]);
        sPicBuffer *buf=&PicBuffer[buf_num];

        for (int i=0; i<4; i++) {
                //printf("release %d: %p\n",i,buf->pixel[i]);
                free(buf->pixel[i]);
                buf->pixel[i]=NULL;
        };
        InitPicBuffer(buf);
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
                        fprintf(stderr,"Warning unsupported pixel format(%d)! \n",pix_fmt);
                        hChromaShift=vChromaShift=1;
        };
};

void cPicBufferManager::LockBuffer(sPicBuffer *picture) {
        PICDEB("LockBuffer buf->pixel[0] %p\n",picture->pixel[0]);
        PicBufMutex.Lock();
        if (picture)
                picture->use_count++;
        else fprintf(stderr,
                    "Error! Trying to lock a nil picture... Ignoring.\n");
        PicBufMutex.Unlock();
};

void cPicBufferManager::UnlockBuffer(sPicBuffer *picture) {
        if (!picture) {
                fprintf(stderr,
                    "Error! Trying to unlock a nil picture... Ignoring.\n");
                return;
        };
        PICDEB("UnlockBuffer buf %p pixel[0] %p %p\n",picture,picture->pixel[0]);
        PicBufMutex.Lock();
        if ( picture->use_count>0 )
                picture->use_count--;
        else fprintf(stderr,"Warning, trying to unlock buffer with use_count 0!\n");
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

bool cPicBufferManager::AllocPicBuffer(int buf_num,PixelFormat pix_fmt,
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
                    return false;
            };

            memset(buf->pixel[i], 128, buf->stride[i]*h>>v_shift);

        }
        buf->max_width=w;
        buf->max_height=h;
        buf->format=pix_fmt;
        PICDEB("end AllocPicBuffer buf %p buf->pixel[0] %p\n",
                        buf,buf->pixel[0]);
        return true;
}

sPicBuffer *cPicBufferManager::GetBuffer(PixelFormat pix_fmt,
                    int w, int h) {
    PICDEB("GetBuffer pix_fmt %d (%d,%d)\n",pix_fmt,w,h);

    PicBufMutex.Lock();
    int buf_num=0;
    while (buf_num<LAST_PICBUF && PicBuffer[buf_num].use_count!=0 ) {
            PICDEB("not using buffer %d: use_count=%d\n",buf_num,
                            PicBuffer[buf_num].use_count);
            buf_num++;
    };

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
            PICDEB("format %d-%d width %d-%d height %d-%d\n",
                     PicBuffer[buf_num].format, pix_fmt,
                     PicBuffer[buf_num].max_width, w,
                     PicBuffer[buf_num].max_height, h);

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

    PICDEB("end GetBuffer: PicBuffer.pixel[0] %p buf_num %d\n",
                    PicBuffer[buf_num].pixel[0],buf_num);
    return &PicBuffer[buf_num];
}

void cPicBufferManager::ReleaseBuffer( sPicBuffer *pic ){
    PICDEB("ReleaseBuffer frame %p, pixel[0] %p\n",
                    pic,pic->pixel[0]);

    int buf_num=0;
    PicBufMutex.Lock();
    while (buf_num<LAST_PICBUF && PicBuffer[buf_num].pixel[0]!=pic->pixel[0] )
            buf_num++;

    if (buf_num>=LAST_PICBUF)  {
            fprintf(stderr,"ReleaseBuffer din't find corresponding PicBuffer!\n");
            exit(-1);
    };

    // found PicBuffer
    PicBuffer[buf_num].use_count--;
    PicBuffer[buf_num].buf_num=-1;
    PicBufMutex.Unlock();
    PICDEB("end ReleaseBuffer bufnum %d use_count %d\n",buf_num,PicBuffer[buf_num].use_count);
}

// end of code based on ffmpeg

/*------------------------------------------------------------------------*/
static void CopyPicBuf_YUV420P_YUY2(sPicBuffer *dst, sPicBuffer *src,
                int width, int height,
                int cutTop, int cutBottom,
                int cutLeft, int cutRight) {
        PICDEB("CopyPicBuf_YUV420P_YUY2 width %d height %d\n",width,height);

        dst->interlaced_frame=src->interlaced_frame;
        uint8_t *dst_ptr=dst->pixel[0]+
                (2*cutTop+dst->edge_height)*dst->stride[0]+
                4*cutLeft+dst->edge_width;// 4*cutLeft ?? <- !!!! [2->4]

        uint8_t *py=src->pixel[0]+
                (2*cutTop+src->edge_height)*src->stride[0]+
                2*cutLeft+src->edge_width;

        uint8_t *pu=src->pixel[1]+
                (cutTop+src->edge_height/2)*src->stride[1]+
                cutLeft+src->edge_width/2;

        uint8_t *pv=src->pixel[2]+
                (cutTop+src->edge_height/2)*src->stride[2]+
                cutLeft+src->edge_width/2;

        int dstStride=dst->stride[0];
        int lumStride=src->stride[0];
        int chromStride=src->stride[1];

        height -= 2 * (cutTop + cutBottom);
        width  -= 2 * (cutLeft + cutRight);

        if (src->interlaced_frame) {
                for(int y=height/4; y--; ) {
                        /* ---------------------------------------------
                         * take chroma line x (it's from field A) for packing
                         * with luma lines y * 2 and y * 2 + 2
                         */
                        yv12_to_yuy2_il_mmx2_line (dst_ptr,
                                        dst_ptr + dstStride * 2, width >> 1,
                                        py, py + lumStride *2,
                                        pu, pv);
                        /* ----------------------------------------------
                         * take chroma line x+1 (it's from field B) for packing
                         * with luma lines y * 2 + 1 and y * 2 + 3
                         */
                        yv12_to_yuy2_il_mmx2_line (dst_ptr + dstStride,
                               dst_ptr + dstStride * 3, width >> 1,
                               py + lumStride, py + lumStride * 3,
                               pu + chromStride, pv + chromStride);

                        py  += 4*lumStride;
                        pu  += 2*chromStride;
                        pv  += 2*chromStride;
                        dst_ptr += 4*dstStride;
                }
        } else {
                for(int y=height/2; y--; ) {
                        yv12_to_yuy2_il_mmx2_line (dst_ptr,
                                        dst_ptr + dstStride , width >> 1,
                                        py, py + lumStride ,
                                        pu, pv);
                        py  += 2*lumStride;
                        pu  += chromStride;
                        pv  += chromStride;
                        dst_ptr += 2*dstStride;
                }
        }
        SFENCE;
        EMMS;
}

/*------------------------------------------------------------------------*/
static void CopyPicBuf_YUV420P(sPicBuffer *dst, sPicBuffer *src,
                int width, int height,
                int cutTop, int cutBottom,
                int cutLeft, int cutRight) {

        int copy_width = width - 2 * (cutLeft + cutRight);
        int copy_height = height - 2 *  (cutBottom + cutTop) ;

        uint8_t *dst_ptr=dst->pixel[0]+
                (2*cutTop+dst->edge_height)*dst->stride[0]+
                 2*cutLeft+dst->edge_width;
        uint8_t *src_ptr=src->pixel[0]+
                (2*cutTop+src->edge_height)*src->stride[0]+
                2*cutLeft+src->edge_width;
        for (int i = copy_height; i>=0; i--) {
                fast_memcpy(dst_ptr,src_ptr,copy_width);
                dst_ptr+=dst->stride[0];
                src_ptr+=src->stride[0];
        };

        dst_ptr=dst->pixel[1]+(cutTop+dst->edge_height/2)*dst->stride[1]+
                cutLeft+dst->edge_width/2;
        src_ptr=src->pixel[1]+(cutTop+src->edge_height/2)*src->stride[1]+
                cutLeft+src->edge_width/2;
        copy_width=width / 2 - (cutLeft + cutRight);
        copy_height = height /2 - (cutBottom+cutTop);
        for (int i = copy_height; i--; ) {
                fast_memcpy (dst_ptr,src_ptr,copy_width);
                dst_ptr+=dst->stride[1];
                src_ptr+=src->stride[1];
        };

        dst_ptr=dst->pixel[2]+(cutTop+dst->edge_height/2)*dst->stride[1]+
                cutLeft+dst->edge_width/2;
        src_ptr=src->pixel[2]+(cutTop+src->edge_height/2)*src->stride[2]+
                cutLeft+src->edge_width/2;
        for (int i = copy_height; i--; ) {
                fast_memcpy (dst_ptr,src_ptr,copy_width);
                dst_ptr+=dst->stride[2];
                src_ptr+=src->stride[2];
        };
};

/*------------------------------------------------------------------------*/
void CopyPicBuf(sPicBuffer *dst, sPicBuffer *src,
                int cutTop, int cutBottom,
                int cutLeft, int cutRight) {
        int width=0;
        int height=0;
        dst->edge_width=0;
        dst->edge_height=0;

        if ( src->width+src->edge_width <= dst->max_width) {
                dst->edge_width = src->edge_width;
                width = dst->width = src->width;
        } else {
                width = dst->width = dst->max_width;
                dst->edge_width = 0;
        };

        if ( src->height+src->edge_height <= dst->max_height) {
                dst->edge_height = src->edge_height;
                dst->height = height = src->height;
        } else {
                height = dst->height = dst->max_height;
                dst->edge_height=0;
        };

        if ( dst->format == PIX_FMT_YUV420P )
                CopyPicBuf_YUV420P(dst,src,width,height,
                                cutTop,cutBottom,
                                cutLeft,cutRight);
        else if ( dst->format == PIX_FMT_YUV422 )
                CopyPicBuf_YUV420P_YUY2(dst,src,width,height,
                                cutTop,cutBottom,
                                cutLeft,cutRight);
        else fprintf(stderr,"Unsupported format in CopyPicBuf!\n");
}

/*------------------------------------------------------------------------*/
void CopyPicBufAlphaBlend_YUV420P_YUY2(sPicBuffer *dst, sPicBuffer *src,
                int width, int height,
                uint8_t *OsdPy,
                uint8_t *OsdPu,
                uint8_t *OsdPv,
                uint8_t *OsdPAlphaY,
                uint8_t *OsdPAlphaUV,
                int OsdStride,
                int cutTop, int cutBottom,
                int cutLeft, int cutRight) {
        PICDEB("CopyPicBufAlphaBlend_YUV420P_YUY2 width %d height %d\n",
                        width,height);

        uint8_t tmp_y[2*width];
        uint8_t tmp_u[width];
        uint8_t tmp_v[width];

        dst->interlaced_frame=src->interlaced_frame;
        uint8_t *dst_ptr=dst->pixel[0]+
                (2*cutTop+dst->edge_height)*dst->stride[0]+
                4*cutLeft+dst->edge_width;// 4*cutLeft ?? <- !!!! [2->4]

        uint8_t *py=src->pixel[0]+
                (2*cutTop+src->edge_height)*src->stride[0]+
                2*cutLeft+src->edge_width;
        uint8_t *osd_py=OsdPy+2*cutTop*OsdStride+2*cutLeft;
        uint8_t *alpha_py=OsdPAlphaY+2*cutTop*OsdStride+2*cutLeft;

        uint8_t *pu=src->pixel[1]+
                (cutTop+src->edge_height/2)*src->stride[1]+
                cutLeft+src->edge_width/2;
        uint8_t *osd_pu=OsdPu+cutTop*OsdStride/2+cutLeft;

        uint8_t *pv=src->pixel[2]+
                (cutTop+src->edge_height/2)*src->stride[2]+
                cutLeft+src->edge_width/2;
        uint8_t *osd_pv=OsdPv+cutTop*OsdStride/2+cutLeft;

        uint8_t *alpha_puv=OsdPAlphaUV+cutTop*OsdStride/2+cutLeft;

        int dstStride=dst->stride[0];
        int lumStride=src->stride[0];
        int chromStride=src->stride[1];

        height -= 2 * (cutTop + cutBottom);
        width  -= 2 * (cutLeft + cutRight);

        if (src->interlaced_frame) {
                for(int y=height/4; y--; ) {
                        /* ---------------------------------------------
                         * take chroma line x (it's from field A) for packing
                         * with luma lines y * 2 and y * 2 + 2
                         */
                        AlphaBlend(tmp_y,osd_py,
                                        py,alpha_py,width);
                        AlphaBlend(&tmp_y[width],osd_py+2*OsdStride,
                                        py+2*lumStride,
                                        alpha_py+2*OsdStride,width);

                        AlphaBlend(tmp_u,osd_pu,
                                        pu,alpha_puv,width>>1);
                        AlphaBlend(tmp_v,osd_pv,
                                        pv,alpha_puv,width>>1);

                        yv12_to_yuy2_il_mmx2_line (dst_ptr,
                                        dst_ptr + dstStride * 2, width >> 1,
                                        tmp_y, &tmp_y[width],
                                        tmp_u, tmp_v);
                        /* ----------------------------------------------
                         * take chroma line x+1 (it's from field B) for packing
                         * with luma lines y * 2 + 1 and y * 2 + 3
                         */
                        AlphaBlend(tmp_y,osd_py+OsdStride,
                                        py+lumStride,
                                        alpha_py+OsdStride,width);
                        AlphaBlend(&tmp_y[width],osd_py+3*OsdStride,
                                        py+3*lumStride,
                                        alpha_py+3*OsdStride,width);

                        AlphaBlend(tmp_u,osd_pu+OsdStride/2,
                                        pu+chromStride,
                                        alpha_puv+OsdStride/2,width>>1);
                        AlphaBlend(tmp_v,osd_pv+OsdStride/2,
                                        pv+chromStride,
                                        alpha_puv+OsdStride/2,width>>1);

                        yv12_to_yuy2_il_mmx2_line (dst_ptr+dstStride,
                                        dst_ptr + dstStride * 3, width >> 1,
                                        tmp_y, &tmp_y[width],
                                        tmp_u, tmp_v);

                        osd_py += 4*OsdStride;
                        alpha_py += 4*OsdStride;
                        osd_pu += OsdStride;
                        osd_pv += OsdStride;
                        alpha_puv += OsdStride;

                        py  += 4*lumStride;
                        pu  += 2*chromStride;
                        pv  += 2*chromStride;
                        dst_ptr += 4*dstStride;
                }
        } else {
                for(int y=height/2; y--; ) {
                        /* ---------------------------------------------
                         * take chroma line x (it's from field A) for packing
                         * with luma lines y * 2 and y * 2 + 2
                         */
                        AlphaBlend(tmp_y,osd_py,
                                        py,alpha_py,width);
                        AlphaBlend(&tmp_y[width],osd_py+OsdStride,
                                        py+lumStride,
                                        alpha_py+OsdStride,width);

                        AlphaBlend(tmp_u,osd_pu,
                                        pu,alpha_puv,width>>1);
                        AlphaBlend(tmp_v,osd_pv,
                                        pv,alpha_puv,width>>1);

                        yv12_to_yuy2_il_mmx2_line (dst_ptr,
                                        dst_ptr + dstStride , width >> 1,
                                        tmp_y, &tmp_y[width],
                                        tmp_u, tmp_v);

                        osd_py += 2*OsdStride;
                        alpha_py += 2*OsdStride;
                        osd_pu += OsdStride/2;
                        osd_pv += OsdStride/2;
                        alpha_puv += OsdStride/2;

                        py  += 2*lumStride;
                        pu  += chromStride;
                        pv  += chromStride;
                        dst_ptr += 2*dstStride;
                }
        }
        SFENCE;
        EMMS;

};

/*------------------------------------------------------------------------*/
void CopyPicBufAlphaBlend_YUV420P(sPicBuffer *dst, sPicBuffer *src,
                int width, int height,
                uint8_t *OsdPy,
                uint8_t *OsdPu,
                uint8_t *OsdPv,
                uint8_t *OsdPAlphaY,
                uint8_t *OsdPAlphaUV,
                int OsdStride,
                int cutTop, int cutBottom,
                int cutLeft, int cutRight) {

        int copy_width = width - 2 * (cutLeft + cutRight);
        int copy_height = height - 2 *  (cutBottom + cutTop) ;

        uint8_t *dst_ptr=dst->pixel[0]+
                (2*cutTop+dst->edge_height)*dst->stride[0]+
                2*cutLeft+dst->edge_width;
        uint8_t *src_ptr=src->pixel[0]+
                (2*cutTop+src->edge_height)*src->stride[0]+
                2*cutLeft+src->edge_width;
        uint8_t *osd_ptr=OsdPy+2*cutTop*OsdStride+2*cutLeft;
        uint8_t *alpha_ptr=OsdPAlphaY+2*cutTop*OsdStride+2*cutLeft;
        for (int i = copy_height ; i--; ) {
                AlphaBlend(dst_ptr,osd_ptr,src_ptr,alpha_ptr,copy_width);
                dst_ptr+=dst->stride[0];
                src_ptr+=src->stride[0];
                osd_ptr+=OsdStride;
                alpha_ptr+=OsdStride;
        };

        dst_ptr=dst->pixel[1]+(cutTop+dst->edge_height/2)*dst->stride[1]+
                cutLeft+dst->edge_width/2;
        src_ptr=src->pixel[1]+(cutTop+src->edge_height/2)*src->stride[1]+
                cutLeft+src->edge_width/2;
        osd_ptr=OsdPu+cutTop*OsdStride/2+cutLeft;
        alpha_ptr=OsdPAlphaUV+cutTop*OsdStride/2+cutLeft;
        copy_width = width / 2 - (cutLeft + cutRight);
        copy_height = height / 2 - (cutBottom+cutTop);
        for (int i = copy_height; i--; ) {
                AlphaBlend(dst_ptr,osd_ptr,src_ptr,alpha_ptr,copy_width);
                dst_ptr+=dst->stride[1];
                src_ptr+=src->stride[1];
                osd_ptr+=OsdStride/2;
                alpha_ptr+=OsdStride/2;
        };

        dst_ptr=dst->pixel[2]+(cutTop+dst->edge_height/2)*dst->stride[1]+
                cutLeft+dst->edge_width/2;
        src_ptr=src->pixel[2]+(cutTop+src->edge_height/2)*src->stride[2]+
                cutLeft+src->edge_width/2;
        osd_ptr=OsdPv+cutTop*OsdStride/2+cutLeft;
        alpha_ptr=OsdPAlphaUV+cutTop*OsdStride/2+cutLeft;
        for (int i = copy_height; i--; ) {
                AlphaBlend(dst_ptr,osd_ptr,src_ptr,alpha_ptr,copy_width);
                dst_ptr+=dst->stride[2];
                src_ptr+=src->stride[2];
                osd_ptr+=OsdStride/2;
                alpha_ptr+=OsdStride/2;
       };
};

/*------------------------------------------------------------------------*/
void CopyPicBufAlphaBlend(sPicBuffer *dst, sPicBuffer *src,
                uint8_t *OsdPy,
                uint8_t *OsdPu,
                uint8_t *OsdPv,
                uint8_t *OsdPAlphaY,
                uint8_t *OsdPAlphaUV,
                int OsdStride,
                int cutTop, int cutBottom,
                int cutLeft, int cutRight) {

        int width=0;
        int height=0;
        dst->edge_width=0;
        dst->edge_height=0;

        if ( src->width+src->edge_width <= dst->max_width) {
                dst->edge_width = src->edge_width;
                width = dst->width = src->width;
        } else {
                width = dst->width = dst->max_width;
                dst->edge_width = 0;
        };

        if ( src->height+src->edge_height <= dst->max_height) {
                dst->edge_height = src->edge_height;
                dst->height = height = src->height;
        } else {
                height = dst->height = dst->max_height;
                dst->edge_height=0;
        };

        if ( width > OSD_FULL_WIDTH )
                width = OSD_FULL_WIDTH;

        if ( height > OSD_FULL_HEIGHT )
                height = OSD_FULL_HEIGHT;

        if ( dst->format == PIX_FMT_YUV420P )
                CopyPicBufAlphaBlend_YUV420P(dst,src,width,height,
                                OsdPy, OsdPu, OsdPv,
                                OsdPAlphaY, OsdPAlphaUV, OsdStride,
                                cutTop,cutBottom,
                                cutLeft,cutRight);
        else if ( dst->format == PIX_FMT_YUV422 )
                CopyPicBufAlphaBlend_YUV420P_YUY2(dst,src,width,height,
                                OsdPy, OsdPu, OsdPv,
                                OsdPAlphaY, OsdPAlphaUV, OsdStride,
                                cutTop,cutBottom,
                                cutLeft,cutRight);
        else fprintf(stderr,"Unsupported format in CopyPicBuf!\n");
};

