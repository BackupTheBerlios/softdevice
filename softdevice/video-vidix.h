/*
 * video-vidix.h: A plugin for the Video Disk Recorder
 *
 * See the README file for copyright information and how to reach the author.
 *
 * $Id: video-vidix.h,v 1.2 2005/02/18 13:31:27 wachm Exp $
 */

#ifndef VIDEO_VIDIX_H
#define VIDEO_VIDIX_H

#include "video.h"
#include <linux/fb.h>
#include "vidixlib.h"
#include "fourcc.h"


class cVidixVideoOut : public cVideoOut {
private:
    uint8_t * osd;
    uint8_t * fb;

    int fbdev;
    int fb_line_len;

    struct fb_fix_screeninfo fb_finfo;
    struct fb_var_screeninfo fb_vinfo;

    int orig_cmaplen;
    __u16 * orig_cmap;

    char               * vidix_name;
    int                vidix_version;
    VDL_HANDLE         vidix_handler;
    vidix_capability_t vidix_cap;
    vidix_playback_t   vidix_play;
    vidix_fourcc_t     vidix_fourcc;
    vidix_yuv_t        dstrides;
    vidix_grkey_t      gr_key;
    uint8_t            next_frame;

public:
  cVidixVideoOut();
  virtual ~cVidixVideoOut();

#if VDRVERSNUM >= 10307
  virtual void ClearOSD();
  virtual void Refresh(cBitmap *Bitmap);
  virtual void GetOSDDimension(int &OsdWidth,int &OsdHeight); 
#else
  virtual void Refresh();
#endif

  virtual void CloseOSD();
//  virtual void OpenOSD();  
  virtual void YUV(uint8_t *Py, uint8_t *Pu, uint8_t *Pv, int Width, int Height, int Ystride, int UVstride);
  virtual void Pause(void);
};

#endif // VIDEO_VIDIX_H
