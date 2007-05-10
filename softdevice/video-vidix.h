/*
 * video-vidix.h: A plugin for the Video Disk Recorder
 *
 * See the README file for copyright information and how to reach the author.
 *
 * $Id: video-vidix.h,v 1.13 2007/05/10 19:54:44 wachm Exp $
 */

#ifndef VIDEO_VIDIX_H
#define VIDEO_VIDIX_H

#include "video.h"
#include <linux/fb.h>
#include "vidixlib.h"
#include "fourcc.h"

class cVidixVideoOut : public cVideoOut {
private:
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
    bool               useVidixAlpha;
    vidix_video_eq_t   vidix_curr_eq;
    int                vidix_curr_deinterlace;
    void SetParams(int Ystride, int UVstride);

public:
  cVidixVideoOut(cSetupStore *setupStore, cSetupSoftlog *Softlog);
  virtual ~cVidixVideoOut();

  virtual void ClearOSD();
  virtual void AdjustOSDMode();
  virtual void GetOSDDimension(int &OsdWidth,int &OsdHeight,
                               int &xPan, int &yPan);
  virtual void GetOSDMode(int &Depth,
                          bool &HasAlpha,
                          bool &AlphaInversed,
		                      bool &IsYUV,
		                      uint8_t *&PixelMask);
  virtual void GetLockOsdSurface(uint8_t *&osd,
                                 int &stride,
                                 bool *&dirtyLines);
  virtual void CloseOSD();
//  virtual void OpenOSD();
  virtual void YUV(sPicBuffer *buf);
  virtual void Pause(void);

  bool MatchPixelFormat(void);
  void AllocLayer(void);
};

#endif // VIDEO_VIDIX_H
