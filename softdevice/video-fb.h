/*
 * video-fb.h: A plugin for the Video Disk Recorder
 *
 * See the README file for copyright information and how to reach the author.
 *
 * $Id: video-fb.h,v 1.4 2005/03/03 20:22:17 lucke Exp $
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
public:
  cFBVideoOut(cSetupStore *setupStore);
  virtual ~cFBVideoOut();
#if VDRVERSNUM >= 10307
  virtual void OpenOSD(int X, int Y);
  virtual void ClearOSD();
  virtual void Refresh(cBitmap *Bitmap);
  virtual void GetOSDDimension(int &OsdWidth,int &OsdHeight);
#else
  virtual void Refresh();
#endif
  virtual void YUV(uint8_t *Py, uint8_t *Pu, uint8_t *Pv, int Width, int Height, int Ystride, int UVstride);
  virtual void Pause(void);
};

#endif // FRAMEBUFFER_H
