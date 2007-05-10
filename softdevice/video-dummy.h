/*
 * video-fb.h: A plugin for the Video Disk Recorder
 *
 * See the README file for copyright information and how to reach the author.
 *
 * $Id: video-dummy.h,v 1.5 2007/05/10 19:54:44 wachm Exp $
 */

#ifndef VIDEO_DUMMY_H
#define VIDEO_DUMMY_H
#include "video.h"

class cDummyVideoOut : public cVideoOut {
private:
public:
  cDummyVideoOut(cSetupStore *setupStore, cSetupSoftlog *Softlog);
  virtual ~cDummyVideoOut();

  virtual void Refresh(cBitmap *Bitmap);
  virtual void YUV(sPicBuffer *buf);
  virtual void Pause(void);
};

#endif // VIDEO_DUMMY_H
