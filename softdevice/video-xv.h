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
 * $Id: video-xv.h,v 1.3 2004/10/29 20:36:07 lucke Exp $
 */

#ifndef VIDEO_XV_H
#define VIDEO_XV_H
#include "video.h"

#include <pthread.h>

// ?? remote.h thread.h why previous without include
#include <vdr/remote.h>
#include <vdr/thread.h>

#include <sys/ipc.h>
#include <sys/shm.h>

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/keysym.h>
#include <X11/extensions/XShm.h>
#include <X11/extensions/Xvlib.h>

#define FOURCC_YV12       0x32315659   /* 4:2:0 Planar: Y + V + U  (3 planes) */
#define FOURCC_YUY2       0x32595559   /* 4:2:2 Packed: Y0+U0+Y1+V0 (1 plane) */

#define XV_FORMAT_MASK    0x03
#define XV_FORMAT_ASIS    0x00
#define XV_FORMAT_NORMAL  0x01
#define XV_FORMAT_WIDE    0x02

#define XV_SIZE_MASK      0x0c
#define XV_SIZE_NORMAL    0x04
#define XV_SIZE_QUARTER   0x08

#define XV_NOSAWINDOW     0x10  /* not use at the moment	*/

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

  void Restore();

public:
  cXvPortAttributeStore();
  ~cXvPortAttributeStore();
  void SetXInfo(Display *dpy, XvPortID port);
  void SetValue(char *name, int value);
  void SetColorkey(int value);
  void Increment(char *name);
  void Decrement(char *name);
  void Save();
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
  int               initialized,
                    osd_refresh_counter,
                    osd_skip_counter,
                    osd_x, osd_y,
                    osd_w, osd_h,

                    /* -------------------------------------------------------
                     * could be specified via argv or parameters
                     */
                    width, height,
                    attr,
                    len,
                    scale_size,
                    format;

  GC                gc, osd_gc;
  XEvent            event;
  XvPortID          port;
  XShmSegmentInfo   shminfo, osd_shminfo;
  XvImage           *xv_image;
  XImage            *osd_image;
  unsigned char     *outbuffer,
                    *osd_buffer,
                    *osd_argb_buffer,
                    *pixels[3];
  char              *w_name, *i_name;
  unsigned int      use_xv_port;
  cXvPortAttributeStore   attributeStore;

  size_t size;
  int xres, yres;
  uint64_t lastUpdate;

  bool              fullScreen;
  void toggleFullScreen(void);


public:
  cXvVideoOut();
  cXvVideoOut (int aspect = DV_FORMAT_NORMAL, int port = 0, int crop = 0,
               int xres = XV_SRC_WIDTH, int yres = XV_SRC_HEIGHT);
  virtual ~cXvVideoOut();
  void ProcessEvents ();
  void ShowOSD (int skip, int do_sync);

#if VDRVERSNUM >= 10307
  virtual void Refresh(cBitmap *Bitmap);
#else
  virtual void Refresh();
#endif

  virtual void CloseOSD();
  virtual bool Initialize (void);
  virtual bool Reconfigure (int format = FOURCC_YUY2);
  virtual void YUV(uint8_t *Py, uint8_t *Pu, uint8_t *Pv, int Width, int Height, int Ystride, int UVstride);
  virtual void Pause(void);
  virtual bool GetInfo(int *fmt, unsigned char **dest,int *w, int *h);
};

/* ---------------------------------------------------------------------------
 */
class cXvRemote : public cRemote, private cThread {
  private:
    bool        active;
    Display     *dpy;
    Window      win;
    cXvVideoOut *video_out;

    virtual void  Action(void);
  public:
                  cXvRemote(const char *Name, cXvVideoOut *vout);
                  ~cXvRemote();
    void          SetX11Info (Display *d, Window w);
    void          XvRemoteStart (void);
    void          PutKey (KeySym key);
//    virtual bool  Initialize(void);
};

#endif // VIDEO_XV_H
