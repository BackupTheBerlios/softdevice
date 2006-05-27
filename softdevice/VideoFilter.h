/*
 * VideoFilter.h: A plugin for the Video Disk Recorder
 *
 * See the README file for copyright information and how to reach the author.
 *
 * $Id: VideoFilter.h,v 1.1 2006/05/27 19:12:41 wachm Exp $
 */
#ifndef __VIDEOFILTER_H__
#define __VIDEOFILTER_H__

#ifdef HAVE_CONFIG
# include "config.h"
#endif

#ifdef PP_LIBAVCODEC
#include <stdint.h> //needed by postproc.h
  #include <postprocess.h>
  //#include <postproc/postprocess.h>
#endif //PP_LIBAVCODEC

#include "video.h"

class cVideoFilter {
protected:
        cVideoOut *vout;

public:
        cVideoFilter(cVideoOut *VideoOut);
        virtual ~cVideoFilter();

        virtual void Filter(sPicBuffer *&dest, sPicBuffer *orig)
        {};
        // Filter a Picture

        bool AllocateCheckBuffer(sPicBuffer *&dest, sPicBuffer *orig);
        // If dest is NULL or doesn't match the format of *orig
        // a picture buffer will be (re)allocated

        bool AllocateBuffer(sPicBuffer *&dest, sPicBuffer *orig);
        // Allocates a picture buffer in dest with the same
        // picture format and size as orig
};

class cVideoMirror : public cVideoFilter {
        sPicBuffer *outBuf;
public:
        cVideoMirror(cVideoOut *VideoOut);
        virtual ~cVideoMirror();
   
        virtual void Filter(sPicBuffer *&dest, sPicBuffer *orig);
};

class cDeintLibav : public cVideoFilter {
        sPicBuffer *outBuf;
public:
        cDeintLibav(cVideoOut *VideoOut);
        virtual ~cDeintLibav();
   
        virtual void Filter(sPicBuffer *&dest, sPicBuffer *orig);
};

class cImageConvert : public cVideoFilter {
        sPicBuffer *outBuf;
public:
        cImageConvert(cVideoOut *VideoOut);
        virtual ~cImageConvert();
   
        virtual void Filter(sPicBuffer *&dest, sPicBuffer *orig);
};

#ifdef PP_LIBAVCODEC
class cLibAvPostProc : public cVideoFilter {
        int width, height;
        PixelFormat pix_fmt;
        pp_mode_t *ppmode;
        pp_context_t *ppcontext;
        sPicBuffer *outBuf;
        int currentDeintMethod,currentppMethod,currentppQuality;
public:
        cLibAvPostProc(cVideoOut *VideoOut);
        virtual ~cLibAvPostProc();
   
        virtual void Filter(sPicBuffer *&dest, sPicBuffer *orig);
};
#endif //PP_LIBAVCODEC

#endif //__VIDEOFILTER_H__
