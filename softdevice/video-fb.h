/*
 * video-fb.h: A plugin for the Video Disk Recorder
 *
 * See the README file for copyright information and how to reach the author.
 *
 * $Id: video-fb.h,v 1.8 2006/05/27 19:12:41 wachm Exp $
 */

#ifndef VIDEO_FB_H
#define VIDEO_FB_H
#include "video.h"
#include <linux/fb.h>

class cFBVideoOut : public cVideoOut {
private:
  int fbdev;
  struct fb_fix_screeninfo fb_finfo;
  struct fb_var_screeninfo fb_orig_vinfo;
  struct fb_var_screeninfo fb_vinfo;

  int orig_cmaplen;
  __u16 * orig_cmap;

  uint8_t *PixelMask;
  size_t size;
  int line_len;
  unsigned char * fb;	// Framebuffer memory
public:
  cFBVideoOut(cSetupStore *setupStore);
  virtual ~cFBVideoOut();
#if VDRVERSNUM >= 10307
  virtual void OpenOSD();
  virtual void ClearOSD();
  virtual void GetOSDMode(int &Depth, bool &HasAlpha, bool &AlphaInversed, 
		  bool &IsYUV, uint8_t *&pixelmask)
  { Depth=16;HasAlpha=false;IsYUV=false;pixelmask=PixelMask; };
  virtual void GetLockOsdSurface(uint8_t *&osd, int &stride, 
                  bool *&dirtyLines);
  virtual void CommitUnlockOsdSurface();
  virtual void GetOSDDimension(int &OsdWidth,int &OsdHeight,
                               int &xPan, int &yPan);
#else
  virtual void Refresh();
#endif
  virtual void YUV(sPicBuffer *Pic);
  virtual void Pause(void);
};

#endif // FRAMEBUFFER_H
