/*
 * xscreensaver.h: A plugin for the Video Disk Recorder
 *
 * See the README file for copyright information and how to reach the author.
 *
 * $Id: xscreensaver.h,v 1.2 2004/10/25 02:36:18 lucke Exp $
 */
#ifndef XSCREENSAVER_H
#define XSCREENSAVER_H

#include <X11/Xproto.h>
#include <X11/Xlib.h>
#include <X11/Xatom.h>

#ifdef LIBXDPMS_SUPPORT

/* ----------------------------------------------------------------------------
 * X11 headers are buggy (won't compile C++). As long as this is true
 * we have to caryy a local copy of them ;-) .
 */
#if 0
#include <X11/extensions/dpms.h>
#else

#define DPMSModeOn      0
#define DPMSModeStandby 1
#define DPMSModeSuspend 2
#define DPMSModeOff     3

#ifndef DPMS_SERVER

#include <X11/X.h>
#include <X11/Xmd.h>

extern "C" {
extern Bool DPMSQueryExtension(Display *, int *, int *);
extern Status DPMSGetVersion(Display *, int *, int *);
extern Bool DPMSCapable(Display *);
extern Status DPMSSetTimeouts(Display *, CARD16, CARD16, CARD16);
extern Bool DPMSGetTimeouts(Display *, CARD16 *, CARD16 *, CARD16 *);
extern Status DPMSEnable(Display *);
extern Status DPMSDisable(Display *);
extern Status DPMSForceLevel(Display *, CARD16);
extern Status DPMSInfo(Display *, CARD16 *, BOOL *);
}
#endif

#endif

#endif

#define INTERVAL 50 // number of seconds between each time we ping xscreensaver

// Our xscreensaver handler class
class cScreensaver {
private:
  Display *dpy;
  Window window;
  bool disabled;
  int last;
  Atom XA_SCREENSAVER_VERSION;
  Atom XA_SCREENSAVER;
  Atom XA_DEACTIVATE;
  CARD16 dpms_state;
  int dpms_dummy;
  int FindWindow();
public:
  cScreensaver(Display *dpy);
  ~cScreensaver();
  void MaybeSendDeactivate();
  void DisableScreensaver(bool disable);
};

#endif
