/*
 * xscreensaver.h: A plugin for the Video Disk Recorder
 *
 * See the README file for copyright information and how to reach the author.
 *
 * $Id: xscreensaver.h,v 1.1 2004/10/18 03:28:37 iampivot Exp $
 */
#ifndef XSCREENSAVER_H
#define XSCREENSAVER_H

#include <X11/Xproto.h>
#include <X11/Xlib.h>
#include <X11/Xatom.h>

#ifdef LIBXDPMS_SUPPORT
#include <X11/extensions/dpms.h>
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
#ifdef LIBXDPMS_SUPPORT
  static BOOL dpms_on;
  CARD16 dpms_state;
  int dpms_dummy;
#endif
  int FindWindow();
public:
  cScreensaver(Display *dpy);
  ~cScreensaver();
  void MaybeSendDeactivate();
  void DisableScreensaver(bool disable);
};

#endif
