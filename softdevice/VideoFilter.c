/*
 * VideoFilter.c: A plugin for the Video Disk Recorder
 *
 * See the README file for copyright information and how to reach the author.
 *
 * $Id: VideoFilter.c,v 1.8 2007/09/16 10:07:59 lucke Exp $
 */
#include "VideoFilter.h"

//#define FILDEB(out...) printf(out)

#ifndef FILDEB
#define FILDEB(out...)
#endif

cVideoFilter::cVideoFilter(cVideoOut *VideoOut)
        : vout(VideoOut) {
};

cVideoFilter::~cVideoFilter() {
};

bool cVideoFilter::AllocateCheckBuffer(sPicBuffer *&dest, sPicBuffer *orig) {
        if (!orig) {
                fprintf(stderr,"Error in AllocateCheckBuffer, orig==NULL!\n");
                return false;
        };

        if (!dest)
                return AllocateBuffer(dest, orig);

        if ( dest->format != orig->format ||
             dest->max_width != orig->width ||
             dest->max_height != orig->height ) {
                fprintf(stderr,"reallocating buffer format %d-%d w: %d-%d h: %d-%d \n",
                      dest->format,orig->format,
                      dest->max_width, orig->width,
                      dest->max_height, orig->height);
                vout->ReleaseBuffer(dest);
                AllocateBuffer(dest, orig);
                if (!dest)
                        return false;
        };
        dest->edge_width=dest->edge_height=0;
        return true;
};

bool cVideoFilter::AllocateBuffer(sPicBuffer *&dest, sPicBuffer *orig) {
        if (!orig) {
                fprintf(stderr,"Error in AllocateBuffer, orig==NULL!\n");
                return false;
        };

        printf("allocating buffer format orig->format %d\n",orig->format);
        dest=vout->GetBuffer(orig->format, orig->width, orig->height);

        dest->width = orig->width;
        dest->height = orig->height;
        dest->pts=orig->pts;
        dest->edge_width=dest->edge_height=0;
        dest->dtg_active_format=orig->dtg_active_format;
        dest->aspect_ratio=orig->aspect_ratio;
        dest->interlaced_frame=orig->interlaced_frame;

        return (dest!=NULL);
};

/*--------------------------------------------------------------------------*/

cVideoMirror::cVideoMirror(cVideoOut *vOut)
        : cVideoFilter(vOut), outBuf(NULL) {
};

cVideoMirror::~cVideoMirror() {
        if (outBuf) {
                vout->ReleaseBuffer(outBuf);
        };
};

void cVideoMirror::Filter(sPicBuffer *&dest, sPicBuffer *orig) {
    FILDEB("cVideoMirror::Filter\n");
    uchar *ptr_src1, *ptr_src2;
    uchar *ptr_dest1, *ptr_dest2;

    AllocateCheckBuffer(outBuf, orig);
    dest=outBuf;

    if ( !outBuf ) {
            dest=orig;
            fprintf(stderr,
                "[softdevice] no picture buffer is allocated for mirroring !\n"
                "[softdevice] switching mirroring off !\n");
            setupStore->mirror = 0;
            return;
    }

    // mirror luminance
    ptr_src1  = orig->pixel[0] + orig->edge_height * orig->stride[0]
            + orig->edge_width;

    for (int h = 0; h < dest->height; h++)
    {
            ptr_dest1 = dest->pixel[0] + h * dest->stride[0];
            for (int w = dest->width-1; w >=0; w--)
            {
                    *ptr_dest1 = ptr_src1[h * orig->stride[0] + w ];
                    ptr_dest1++;
            }
    }

    // mirror chrominance
    int h_shift;
    int v_shift;
    avcodec_get_chroma_sub_sample(dest->format,&h_shift,&v_shift);

    ptr_src1  = orig->pixel[1] + (orig->edge_height>>v_shift) * orig->stride[1]
            + (orig->edge_width >> h_shift);
    ptr_src2  = orig->pixel[2] + (orig->edge_height>>v_shift) * orig->stride[2]
            + (orig->edge_width >> h_shift);


    for (int h = 0; h < dest->height >> v_shift ; h++) {
            ptr_dest1 = dest->pixel[1] + h * dest->stride[1];
            ptr_dest2 = dest->pixel[2] + h * dest->stride[2];
            for (int w = (dest->width >> h_shift) - 1; w >= 0; w--)
            {
                    *ptr_dest1 = ptr_src1[h * orig->stride[1] + w ];
                    *ptr_dest2 = ptr_src2[h * orig->stride[2] + w ];
                    ptr_dest1++;
                    ptr_dest2++;
            }
    };

    CopyPicBufferContext(dest,orig);
}

/*--------------------------------------------------------------------------*/

cDeintLibav::cDeintLibav(cVideoOut *vOut)
        : cVideoFilter(vOut), outBuf(NULL) {
};

cDeintLibav::~cDeintLibav() {
        if (outBuf) {
                vout->ReleaseBuffer(outBuf);
        };
};

void cDeintLibav::Filter(sPicBuffer *&dest, sPicBuffer *orig) {
    AVPicture           avpic_src, avpic_dest;
    FILDEB("cDeintLibav::Filter orig %p format %d (%d,%d) buf_num %d\n",orig,
                    orig->format,orig->max_width,orig->max_height,orig->buf_num);

    AllocateCheckBuffer(outBuf, orig);
    dest=outBuf;

    if ( !outBuf ) {
            dest=orig;
            fprintf(stderr,
                "[softdevice] no picture buffer is allocated for deinterlacing!\n"
                "[softdevice] switching deinterlacing off !\n");
            setupStore->deintMethod = 0;
            return;
    }

    int h_shift;
    int v_shift;
    avcodec_get_chroma_sub_sample(dest->format,&h_shift,&v_shift);

    avpic_src.data[0] = orig->pixel[0]
            + orig->edge_height * orig->stride[0]
            + orig->edge_width;

    avpic_src.data[1] = orig->pixel[1]
            + (orig->edge_height >> v_shift) * orig->stride[1]
            + (orig->edge_width >> h_shift);

    avpic_src.data[2] = orig->pixel[2]
            + (orig->edge_height >> v_shift) * orig->stride[2]
            + (orig->edge_width >> h_shift);

    memcpy(avpic_src.linesize,orig->stride,sizeof(avpic_src.linesize));

    memcpy(avpic_dest.data,dest->pixel,sizeof(avpic_dest.data));
    memcpy(avpic_dest.linesize,dest->stride,sizeof(avpic_dest.linesize));

    if (avpicture_deinterlace(&avpic_dest, &avpic_src, orig->format,
                            orig->width, orig->height) < 0) {
            dest=orig;
            fprintf(stderr,
                            "[softdevice] error, libavcodec deinterlacer failure\n"
                            "[softdevice] switching deinterlacing off !\n");
            setupStore->deintMethod = 0;
            return;
    }
    CopyPicBufferContext(dest,orig);
    dest->interlaced_frame=false;
}

/*---------------------------cImageConvert---------------------------------*/

cImageConvert::cImageConvert(cVideoOut *vOut)
        : cVideoFilter(vOut), outBuf(NULL)
#ifdef USE_SWSCALE
          , img_convert_ctx(NULL), ctx_width(0),
          ctx_height(0), ctx_fmt(0)
#endif
{
};

cImageConvert::~cImageConvert() {
        if (outBuf) {
                vout->ReleaseBuffer(outBuf);
        };
};

void cImageConvert::Filter(sPicBuffer *&dest, sPicBuffer *orig) {
        FILDEB("cImageConvert::Filter\n");
        AVPicture           avpic_src, avpic_dest;

        if ( !outBuf ||
                        outBuf->max_width != orig->width ||
                        outBuf->max_height != orig->height ) {

                if (outBuf)
                        vout->ReleaseBuffer(outBuf);

                outBuf=dest=vout->GetBuffer(PIX_FMT_YUV420P,
                                orig->width, orig->height);

                dest->width = orig->width;
                dest->height = orig->height;
                dest->edge_width=dest->edge_height=0;
        };
        dest=outBuf;

        if ( !outBuf ) {
		dest=orig;
                fprintf(stderr,"[softdevice] no picture buffer is allocated for image converting!\n");
                return;
        }
#ifdef USE_SWSCALE
        if ( ctx_width != orig->width || ctx_height != orig->height
               || ctx_fmt != orig->format ) {
                free(img_convert_ctx);
                img_convert_ctx=NULL;
        };

        if (!img_convert_ctx) {
                img_convert_ctx = sws_getContext(orig->width, orig->height,
                                PIX_FMT_YUV420P,
                                orig->width, orig->height,
                                orig->format,
                                SWS_BICUBIC, NULL, NULL, NULL);
                if (!img_convert_ctx) {
                        fprintf(stderr,"[softdevice] could not get SWScaler context. No image converting!\n");
                        dest=orig;
                        return;
                };
                ctx_width=orig->width;
                ctx_height=orig->height;
                ctx_fmt=orig->format;
        };
#endif
        int h_shift;
        int v_shift;
        avcodec_get_chroma_sub_sample(orig->format,&h_shift,&v_shift);

        avpic_src.data[0] = orig->pixel[0]
                + orig->edge_height * orig->stride[0]
                + orig->edge_width;

        avpic_src.data[1] = orig->pixel[1]
                + (orig->edge_height >> v_shift) * orig->stride[1]
                + (orig->edge_width >> h_shift);

        avpic_src.data[2] = orig->pixel[2]
                + (orig->edge_height >> v_shift) * orig->stride[2]
                + (orig->edge_width >> h_shift);

        memcpy(avpic_src.linesize,orig->stride,sizeof(avpic_src.linesize));

        memcpy(avpic_dest.data,dest->pixel,sizeof(avpic_dest.data));
        memcpy(avpic_dest.linesize,dest->stride,sizeof(avpic_dest.linesize));

#ifdef USE_SWSCALE
       sws_scale(img_convert_ctx, avpic_src.data, avpic_src.linesize,
                        0, orig->height, avpic_dest.data, avpic_dest.linesize);
#else
        if (img_convert(&avpic_dest,PIX_FMT_YUV420P,
                                &avpic_src, orig->format,
                                orig->width, orig->height) < 0) {
                dest=orig;
                fprintf(stderr,
                                "[softdevice] error, libavcodec img_convert failure\n");
                return;
        }
#endif
        CopyPicBufferContext(dest,orig);
}

/*---------------------------cBorderDetect---------------------------------*/
/*

Based on a patch by Roland Praml.

How Borderdetection works:
Scan the picture from top and bottom to the middle for bright lines, omitting
1/6 of the width on left and right side. After BODER_MIN_SIZE lines which are
on average brigther than BORDER_BLACK, stop scanning. During the scan, edges
are detected by subtracting the top (bottom) pixel from the current pixel.
If more than 1/6*width of the differences are greater than EDGE_DIFF, the
position is marked as an edge.

*/
#define BORDER_MIN_SIZE 4
#define FRAMES_BEFORE_SWITCH 25
#define BORDER_BLACK 10
#define EDGE_DIFF 30

cBorderDetect::cBorderDetect(cVideoOut *vOut)
        : cVideoFilter(vOut), currOrigAspect(-1.0), currDetAspect(-1.0),
          newDetAspect(-1.0),currBlackBorder(0),frame_count(0) {

};

cBorderDetect::~cBorderDetect() {
};

void cBorderDetect::Filter(sPicBuffer *&dest, sPicBuffer *orig) {
    int black_border=0;
    int     lim;
    float   new_aspect;

    dest = orig; // "copy" do not modify

    double tmp_asp = currDetAspect;

    if (orig->aspect_ratio > 1.43)
            // 16/9 Frame
            return;

    int width=orig->width/6;
    int height=orig->height/4;
    int not_black_count=0;

    // 4/3 Frame
    // start of first line
    uint8_t *pic_start=orig->pixel[0]
            +(orig->edge_height+1)*orig->stride[0]
            +orig->edge_width;
    // start of last line
    uint8_t *pic_end=orig->pixel[0]
            +(orig->edge_height+orig->height-2)*orig->stride[0]
            +orig->edge_width;

    int brightness;
    int edge_pixel;
    int edge_pos=-1;
    int stride=orig->stride[0];
    // scan 1/6 from top and bottom border
    for (black_border=1; black_border < height; black_border++) {
            brightness=0;
            edge_pixel=0;

            // scan 4/6 of line
            for (int y = width ; y < width*5; y++) {
                    // scan upper border (4/6)
                    brightness += *(pic_start+y);
                    edge_pixel += !!(((int)*(pic_start-stride+y))
                                    -((int)*(pic_start+y)) < -EDGE_DIFF);
                    // scan lower border (4/6)
                    brightness += *(pic_end+y);
                    edge_pixel += !!(((int)*(pic_end+stride+y))
                                    -((int)*(pic_end+y))  < -EDGE_DIFF);
            }
            brightness -= width*8*16;
            brightness /= width * 8;

            if (edge_pixel > width)
                    edge_pos=black_border;

            if (brightness > BORDER_BLACK)
                    not_black_count++;
            else not_black_count=0;

            // break if we have found BORDER_MIN_SIZE non black lines
            if (not_black_count>BORDER_MIN_SIZE)
                    break;

            // next lines
            pic_start+=stride;
            pic_end-=stride;
    }
    if (edge_pos>0) {
#if 0
            // show detected edge
            memset(orig->pixel[1]
                +(orig->edge_height+edge_pos)/2*orig->stride[1]
                +(orig->edge_width+width)/2,0xFF,4*width/2);
            memset(orig->pixel[1]
                +(orig->edge_height+orig->height-edge_pos-1)/2*orig->stride[1]
                +(orig->edge_width+width)/2,0xFF,4*width/2);
#endif
            //printf("Picture is bright enough\n");
            // calculate new aspect with the detected borders
            new_aspect = orig->aspect_ratio * float(orig->height)
                    / (float)(orig->height - edge_pos * 2);

            // 4/3 = 1.33  16/9 = 1.77  mid = 1,55
            if (new_aspect > 1.65)
                    tmp_asp = 16.0 / 9.0;
            else if (new_aspect > 1.45)
                    tmp_asp = 14.0 / 9.0; //4.0 / 3.0;
            else tmp_asp = 4.0 / 3.0;
            //printf("Bordersize: %d  Calculated aspect %f\n",edge_pos, new_aspect);
            lim = (orig->height / 2) -
                     (orig->aspect_ratio * orig->height) /
                       ((16.0 / 9.0) * 2);
    }

    if (tmp_asp == newDetAspect && newDetAspect != currDetAspect ) {
            frame_count++;
            if ( frame_count > FRAMES_BEFORE_SWITCH ) {
                    currDetAspect=tmp_asp;
                    currBlackBorder = (edge_pos > lim) ? lim : edge_pos;
                    fprintf(stderr,
                            "new Aspect detected %f (%f) / bb %d %d\n",
                            tmp_asp, new_aspect, edge_pos, lim);
            }
    } else  frame_count=0;
    newDetAspect=tmp_asp;
#if 1
    if (currDetAspect>0.0) {
            orig->aspect_ratio=currDetAspect;
            orig->edge_height+=currBlackBorder;
            orig->height-=2*currBlackBorder;
/*            printf("%f %d edge_height %d height %d\n",
                            currDetAspect,currBlackBorder,
                            orig->edge_height, orig->height);*/
    }
#endif
}

/*---------------------------cLibAvPostProc---------------------------------*/
#ifdef PP_LIBAVCODEC
cLibAvPostProc::cLibAvPostProc(cVideoOut *vOut)
        : cVideoFilter(vOut), width(-1), height(-1), pix_fmt(PIX_FMT_NB),
          ppmode(NULL), ppcontext(NULL), outBuf(NULL),
          currentDeintMethod(-1), currentppMethod(-1), currentppQuality(-1) {
};

cLibAvPostProc::~cLibAvPostProc() {
        if (ppcontext) {
                pp_free_context(ppcontext);
                ppcontext = NULL;
        }
        if (ppmode)  {
                pp_free_mode (ppmode);
                ppmode = NULL;
        }
        if (outBuf) {
                vout->ReleaseBuffer(outBuf);
        };
};

void cLibAvPostProc::Filter(sPicBuffer *&dest, sPicBuffer *orig) {
        AVPicture           avpic_src, avpic_dest;

        AllocateCheckBuffer(outBuf, orig);
        dest=outBuf;

        if ( !outBuf ) {
                dest=orig;
                fprintf(stderr,
                                "[softdevice] no picture buffer is allocated for post processing!\n"
                                "[softdevice] switching post processing off !\n");
                setupStore->deintMethod = setupStore->ppMethod = 0;
                return;
        }

        if (ppcontext == NULL ||
                        orig->width != width ||
                        orig->height != height||
                        orig->format != pix_fmt) {
                width=orig->width;height=orig->height;
                pix_fmt=orig->format;

                // reallocate ppcontext if format or size of picture changed
                if (ppcontext)
                {
                        pp_free_context(ppcontext);
                        ppcontext = NULL;
                }
                /* set one of this values instead of 0 in pp_get_context for
                 * processor-independent optimations:
                 PP_CPU_CAPS_MMX, PP_CPU_CAPS_MMX2, PP_CPU_CAPS_3DNOW
                 */
                int flags=0;
#ifdef USE_MMX
                flags|=PP_CPU_CAPS_MMX;
#endif
#ifdef USE_MMX2
                flags|=PP_CPU_CAPS_MMX2;
#endif
                if (pix_fmt == PIX_FMT_YUV420P)
                        flags|=PP_FORMAT_420;
                else if (pix_fmt == PIX_FMT_YUV422P)
                        flags|=PP_FORMAT_422;
                else if (pix_fmt == PIX_FMT_YUV444P)
                        flags|=PP_FORMAT_444;

                ppcontext = pp_get_context(width, height,flags);
        }

        if ( ppmode == NULL
                        || currentDeintMethod != setupStore->deintMethod
                        || currentppMethod != setupStore->ppMethod
                        || currentppQuality != setupStore->ppQuality ) {
                // reallocate ppmode if method or quality changed
                currentDeintMethod = setupStore->deintMethod;
                currentppMethod = setupStore->ppMethod;
                currentppQuality = setupStore->ppQuality;

                if (ppmode)  {
                        pp_free_mode (ppmode);
                        ppmode = NULL;
                }
                char mode[60]="";
                if (setupStore->getPPdeintValue() && setupStore->getPPValue())
                        sprintf(mode,"%s,%s",setupStore->getPPdeintValue(),
                                        setupStore->getPPValue());
                else if (setupStore->getPPdeintValue() )
                        sprintf(mode,"%s",setupStore->getPPdeintValue());
                else if (setupStore->getPPValue() )
                        sprintf(mode,"%s",setupStore->getPPValue());

                ppmode = pp_get_mode_by_name_and_quality(mode, currentppQuality);
        }
        if (ppmode == NULL || ppcontext == NULL) {
                dest=orig;
                fprintf(stderr,
                        "[softdevice] pp-filter %s couldn't be initialized,\n"
                        "[softdevice] switching postprocessing off !\n",
                        setupStore->getPPValue());
                setupStore->deintMethod = setupStore->ppMethod = 0;
                return;
        }

        int h_shift;
        int v_shift;
        avcodec_get_chroma_sub_sample(orig->format,&h_shift,&v_shift);

        avpic_src.data[0] = orig->pixel[0]
                + orig->edge_height * orig->stride[0]
                + orig->edge_width;

        avpic_src.data[1] = orig->pixel[1]
                + (orig->edge_height >> v_shift) * orig->stride[1]
                + (orig->edge_width >> h_shift);

        avpic_src.data[2] = orig->pixel[2]
                + (orig->edge_height >> v_shift) * orig->stride[2]
                + (orig->edge_width >> h_shift);

        memcpy(avpic_src.linesize,orig->stride,sizeof(avpic_src.linesize));

        memcpy(avpic_dest.data,dest->pixel,sizeof(avpic_dest.data));
        memcpy(avpic_dest.linesize,dest->stride,sizeof(avpic_dest.linesize));

        pp_postprocess(avpic_src.data,
                        avpic_src.linesize,
                        avpic_dest.data,
                        avpic_dest.linesize,
                        width,
                        height,
                        NULL,//picture->qscale_table,
                        0,//picture->qstride,
                        ppmode,
                        ppcontext,
                        orig->pict_type);
        CopyPicBufferContext(dest,orig);
}
#endif //PP_LIBAVCODEC

