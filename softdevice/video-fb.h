/*
 * video-fb.h: A plugin for the Video Disk Recorder
 *
 * See the README file for copyright information and how to reach the author.
 *
 * $Id: video-fb.h,v 1.1 2004/08/01 05:07:04 lucke Exp $
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
  size_t size;
  int line_len;
  unsigned char * fb;	// Framebuffer memory
public:
  cFBVideoOut();
  virtual ~cFBVideoOut();
#if VDRVERSNUM >= 10307
  virtual void Refresh(cBitmap *Bitmap);
#else
  virtual void Refresh();
#endif
  virtual void YUV(uint8_t *Py, uint8_t *Pu, uint8_t *Pv, int Width, int Height, int Ystride, int UVstride);
  virtual void Pause(void);
};

#endif // FRAMEBUFFER_H
