/*
 * VideoFilter.c: A plugin for the Video Disk Recorder
 *
 * See the README file for copyright information and how to reach the author.
 *
 * $Id: VideoFilter.c,v 1.3 2006/05/30 18:57:21 wachm Exp $
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
            setupStore.mirror = 0;
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
            setupStore.deintMethod = 0;
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
            setupStore.deintMethod = 0;
            return;
    }
    CopyPicBufferContext(dest,orig);
    dest->interlaced_frame=false;
}

/*---------------------------cImageConvert---------------------------------*/

cImageConvert::cImageConvert(cVideoOut *vOut) 
        : cVideoFilter(vOut), outBuf(NULL) {
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
                dest->pts=orig->pts;
                dest->edge_width=dest->edge_height=0;
                dest->dtg_active_format=orig->dtg_active_format;
                dest->aspect_ratio=orig->aspect_ratio;
                dest->interlaced_frame=orig->interlaced_frame;
        };
        dest=outBuf;

        if ( !outBuf ) {
		dest=orig;
                fprintf(stderr,"[softdevice] no picture buffer is allocated for image converting!\n");
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

        if (img_convert(&avpic_dest,PIX_FMT_YUV420P,
                                &avpic_src, orig->format,
                                orig->width, orig->height) < 0) {
                dest=orig;
                fprintf(stderr,
                                "[softdevice] error, libavcodec img_convert failure\n");
                return;
        }
    CopyPicBufferContext(dest,orig);
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
                setupStore.deintMethod = setupStore.ppMethod = 0;
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
                        || currentDeintMethod != setupStore.deintMethod 
                        || currentppMethod != setupStore.ppMethod
                        || currentppQuality != setupStore.ppQuality ) {
                // reallocate ppmode if method or quality changed
                currentDeintMethod = setupStore.deintMethod;
                currentppMethod = setupStore.ppMethod;
                currentppQuality = setupStore.ppQuality;

                if (ppmode)  {
                        pp_free_mode (ppmode);
                        ppmode = NULL;
                }
                char mode[60]="";
                if (setupStore.getPPdeintValue() && setupStore.getPPValue())
                        sprintf(mode,"%s,%s",setupStore.getPPdeintValue(),
                                        setupStore.getPPValue());
                else if (setupStore.getPPdeintValue() )
                        sprintf(mode,"%s",setupStore.getPPdeintValue());
                else if (setupStore.getPPValue() )
                        sprintf(mode,"%s",setupStore.getPPValue());

                ppmode = pp_get_mode_by_name_and_quality(mode, currentppQuality);
        }
        if (ppmode == NULL || ppcontext == NULL) {
                dest=orig;
                fprintf(stderr,
                        "[softdevice] pp-filter %s couldn't be initialized,\n"
                        "[softdevice] switching postprocessing off !\n",
                        setupStore.getPPValue());
                setupStore.deintMethod = setupStore.ppMethod = 0;
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

