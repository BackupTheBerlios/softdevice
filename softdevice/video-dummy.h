/*
 * video-fb.h: A plugin for the Video Disk Recorder
 *
 * See the README file for copyright information and how to reach the author.
 *
 * $Id: video-dummy.h,v 1.3 2006/05/27 19:12:41 wachm Exp $
 */

#ifndef VIDEO_DUMMY_H
#define VIDEO_DUMMY_H
#include "video.h"

class cDummyVideoOut : public cVideoOut {
private:
public:
  cDummyVideoOut(cSetupStore *setupStore);
  virtual ~cDummyVideoOut();

#if VDRVERSNUM >= 10307
  virtual void Refresh(cBitmap *Bitmap);
#else
  virtual void Refresh();
#endif

  virtual void YUV(sPicBuffer *buf);
  virtual void Pause(void);
};

#endif // VIDEO_DUMMY_H
