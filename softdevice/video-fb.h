/*
 * video-fb.h: A plugin for the Video Disk Recorder
 *
 * See the README file for copyright information and how to reach the author.
 *
 * $Id: video-fb.h,v 1.11 2007/05/10 19:54:44 wachm Exp $
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

  size_t size;
  int line_len;
  unsigned char * fb;	// Framebuffer memory
  sPicBuffer privBuf;
public:
  cFBVideoOut(cSetupStore *setupStore, cSetupSoftlog *Softlog);
  virtual ~cFBVideoOut();
  virtual void GetOSDMode(int &Depth, bool &HasAlpha, bool &AlphaInversed,
		  bool &IsYUV, uint8_t *&pixelmask)
  { IsYUV=true;};
  virtual void GetOSDDimension(int &OsdWidth,int &OsdHeight,
                               int &xPan, int &yPan);
  virtual void YUV(sPicBuffer *Pic);
};

#endif // FRAMEBUFFER_H
