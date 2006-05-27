/*
 * video-vidix.h: A plugin for the Video Disk Recorder
 *
 * See the README file for copyright information and how to reach the author.
 *
 * $Id: video-vidix.h,v 1.10 2006/05/27 19:12:41 wachm Exp $
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

    void SetParams(int Ystride, int UVstride);

public:
  cVidixVideoOut(cSetupStore *setupStore);
  virtual ~cVidixVideoOut();

#if VDRVERSNUM >= 10307
  virtual void ClearOSD();
  virtual void AdjustOSDMode();  
  virtual void GetOSDDimension(int &OsdWidth,int &OsdHeight,
                               int &xPan, int &yPan);
  virtual void GetOSDMode(int &Depth,bool &HasAlpha, bool &AlphaInversed,
		  bool &IsYUV,uint8_t *&PixelMask)
  { Depth=Bpp; HasAlpha=false;AlphaInversed=false; 
	  IsYUV=(current_osdMode == OSDMODE_SOFTWARE);
	  PixelMask=NULL;};
  virtual void GetLockOsdSurface(uint8_t *&osd, int &stride, 
                  bool *&dirtyLines);
#else
  virtual void Refresh();
#endif

  virtual void CloseOSD();
//  virtual void OpenOSD();  
  virtual void YUV(sPicBuffer *buf);
  virtual void Pause(void);

  bool MatchPixelFormat(void);
  void AllocLayer(void);
};

#endif // VIDEO_VIDIX_H
