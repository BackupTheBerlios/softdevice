/*
 * video-fb.h: A plugin for the Video Disk Recorder
 *
 * See the README file for copyright information and how to reach the author.
 *
 * $Id: video-dummy.h,v 1.1 2004/08/01 05:07:05 lucke Exp $
 */

#ifndef VIDEO_DUMMY_H
#define VIDEO_DUMMY_H
#include "video.h"

class cDummyVideoOut : public cVideoOut {
private:
public:
  cDummyVideoOut();
  virtual ~cDummyVideoOut();

#if VDRVERSNUM >= 10307
  virtual void Refresh(cBitmap *Bitmap);
#else
  virtual void Refresh();
#endif

  virtual void YUV(uint8_t *Py, uint8_t *Pu, uint8_t *Pv, int Width, int Height, int Ystride, int UVstride);
  virtual void Pause(void);
};

#endif // VIDEO_DUMMY_H
