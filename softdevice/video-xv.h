/*
 * video-xv.h: A plugin for the Video Disk Recorder providing output
 *             capabilities via Xv
 *
 * Copyright Stefan Lucke, Roland Praml
 *
 * Xv code is taken from libdv  http://sourceforge.net/projects/libdv/
 * libdv is now LGPL. Let's see/discuss if that will get in trouble
 * with GPL of this plugin.
 *
 * Original copyright of display.h of libdv is:
 *     Copyright (C) Charles 'Buck' Krasic - April 2000
 *     Copyright (C) Erik Walthinsen - April 2000
 *
 * $Id: video-xv.h,v 1.29 2007/07/11 20:08:35 lucke Exp $
 */

#ifndef VIDEO_XV_H
#define VIDEO_XV_H
#include "video.h"

#include <pthread.h>


#include <sys/ipc.h>
#include <sys/shm.h>

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/keysym.h>
#include <X11/extensions/XShm.h>
#include <X11/extensions/Xvlib.h>

#if XINERAMA_SUPPORT
# include <X11/extensions/Xinerama.h>
#endif

#ifndef STAND_ALONE
#include <vdr/remote.h>
#include <vdr/thread.h>
#else
#include "VdrReplacements.h"
#endif

#define FOURCC_YV12       0x32315659   /* 4:2:0 Planar: Y + V + U  (3 planes) */
#define FOURCC_I420       0x30323449   /* 4:2:0 Planar: Y + U + V  (3 planes) */
#define FOURCC_YUY2       0x32595559   /* 4:2:2 Packed: Y0+U0+Y1+V0 (1 plane) */

#define XV_FORMAT_MASK    0x03
#define XV_FORMAT_ASIS    0x00
#define XV_FORMAT_NORMAL  0x01
#define XV_FORMAT_WIDE    0x02

#define XV_SIZE_MASK      0x0c
#define XV_SIZE_NORMAL    0x04
#define XV_SIZE_QUARTER   0x08

#define XV_NOSAWINDOW     0x10  /* not use at the moment*/

#define XV_SRC_HEIGHT         576
#define XV_SRC_WIDTH          736


#define XV_DEST_HEIGHT        XV_SRC_HEIGHT
#define XV_DEST_WIDTH_4_3     ((XV_DEST_HEIGHT/3)*4)
#define XV_DEST_WIDTH_16_9    ((XV_DEST_HEIGHT/9)*16)
#define XV_DEST_WIDTH_14_9    ((XV_DEST_HEIGHT/9)*14)

/* ---------------------------------------------------------------------------
 */
class cXvPortAttributeStore {
private:
  Display       *dpy;
  XvPortID      port;
  XvAttribute   *portAttributes;
  int           portAttributeCount,
                *portAttributeSaveValues,
                *portAttributeCurrentValues;
  Atom          *portAttributeAtoms;
  cSetupStore   *setupStore;
  int           currBrightness,
                currContrast,
                currHue,
                currSaturation;

  void Restore();

public:
  cXvPortAttributeStore();
  ~cXvPortAttributeStore();
  void SetXInfo(Display *dpy, XvPortID port, cSetupStore *setupStore);
  void SetValue(char *name, int value);
  void SetValuePercent(char *name, int value);
  int GetValuePercent(int index);
  void SetColorkey(int value);
  void Increment(char *name);
  void Decrement(char *name);
  void Save();
  bool HasAttribute(char *name);
  void CheckVideoParmChange();
};

/* ---------------------------------------------------------------------------
 */
class cXvVideoOut : public cVideoOut {
private:
  /* -----------------------------------------------------------
   * Xv specific members
   */
  Display           *dpy;
  Screen            *scn;
  Window            rwin, win;
  Cursor            hidden_cursor;
  Bool              cursor_visible;
  long              motion_time, button_time;
  Atom              net_wm_STATE_FULLSCREEN,
                    net_wm_STATE_STAYS_ON_TOP,
                    net_wm_STATE_ABOVE,
                    net_wm_STATE;
  int               toggleInProgress,
                    xv_initialized,
                    /* -------------------------------------------------------
                     * could be specified via argv or parameters
                     */
                    xvWidth, xvHeight,
                    width, height,
                    format;
  bool              needsFullOSDRedraw;
  /* -------------------------------------------------------------------------
   * Xinerama specific members (not all depend on Xinerame available)
   */
  int               xin_screen,
                    xin_num_screens;
  bool              xin_mode;
#if XINERAMA_SUPPORT
  XineramaScreenInfo  *xin_screen_info;
#endif

  GC                gc;
  XvPortID          port;
public:
  bool              useShm;
  XShmSegmentInfo   shminfo, osd_shminfo;
  XvImage           *xv_image;
  XImage            *osd_image;
  int               osd_max_width,osd_max_height;
  int               xv_max_width,xv_max_height;
  sPicBuffer        privBuf;
  sPicBuffer        osdBuf;

  XvImage           *dr_image[LAST_PICBUF];
  XShmSegmentInfo   dr_shminfo[LAST_PICBUF];

private:
  unsigned char     *osd_buffer,
                    *pixels[3];
  char              *w_name, *i_name;
  unsigned int      use_xv_port;
  cXvPortAttributeStore   attributeStore;

  uint64_t lastUpdate;

  bool              fullScreen;

  void  toggleFullScreen(void),
        AdjustDisplayRatio(void),
        AdjustXineramaScreen(void);
  int   GetScreenWidth(void),
        GetScreenHeight();
  void  ClearXvArea(uint8_t y, uint8_t u, uint8_t v);

public:
  cXvVideoOut(cSetupStore *setupStore, cSetupSoftlog *softlog);
  virtual ~cXvVideoOut();
  virtual void ProcessEvents ();
  void ShowOSD ();

  virtual void ReleasePicBuffer(int buf_num);
  virtual bool AllocPicBuffer(int buf_num,PixelFormat pix_fmt,
                        int w, int h);
  virtual void ClearOSD();
  virtual void AdjustOSDMode();
  virtual int GetOSDColorkey();
  virtual void GetOSDDimension(int &OsdWidth,int &OsdHeight,
                               int &xPan, int &yPan);
  virtual void GetOSDMode(int &Depth, bool &HasAlpha, bool &AlphaInversed,
                  bool &IsYUV);
  virtual bool OSDNeedsRedraw(void);
  virtual void GetLockOsdSurface(uint8_t *&osd, int &stride,
                  bool *&dirtyLines);
  virtual void CommitUnlockOsdSurface();
  virtual bool Initialize (void);
  virtual bool Reconfigure (int format = 0,
                  int width = XV_SRC_WIDTH, int height = XV_SRC_HEIGHT);
  int GetFormat() const
  {return format;};
  void DeInitXv();
  void CreateXvImage(Display *dpy,XvPortID port,XvImage *&xv_image,
                  XShmSegmentInfo &shminfo,int format, int &width, int &height);
  void DestroyXvImage(Display *dpy,XvPortID port,
                  XvImage *&xv_image,
                  XShmSegmentInfo &shminfo );
  int PutXvImage(XvImage *xv_image, int edge_width=0, int edge_height=0);
  virtual void YUV(sPicBuffer *buf);
  virtual void Pause(void);

  virtual void Suspend();
  virtual bool Resume();
};

extern cSoftRemote        *xvRemote;

#endif // VIDEO_XV_H
