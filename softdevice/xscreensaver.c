/*
 * xscreensaver.c: A plugin for the Video Disk Recorder
 *
 * See the README file for copyright information and how to reach the author.
 *
 * Code adapted by Torgeir Veimo from jwz' xscreensaver remote control code
 * (simplified similar to Gerd Knorr's xawtv) which carries the following 
 * copyright notice:
 *
 * xscreensaver-command, Copyright (c) 1991-1998
 * by Jamie Zawinski <jwz@jwz.org>
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation.  No representations are made about the suitability of this
 * software for any purpose.  It is provided "as is" without express or 
 * implied warranty.
 * 
 * $Id: xscreensaver.c,v 1.3 2004/10/25 02:36:18 lucke Exp $
 */

#include <vdr/plugin.h>
#include "xscreensaver.h"
#include "utils.h"

static XErrorHandler old_handler = 0;
static Bool got_badwindow = False;
static BOOL dpms_on;

static int BadWindow_ehandler(Display *dpy, XErrorEvent *error) {
  if (error->error_code == BadWindow) {
    got_badwindow = True;
    return 0;
  } else {
    if (!old_handler)
      isyslog("[softdevice-xscreensaver]: old handler not found!\n");
    return(*old_handler) (dpy, error);
  }
}

cScreensaver::~cScreensaver() {
#ifdef LIBXDPMS_SUPPORT
  if (disabled &&
      DPMSQueryExtension(dpy, &dpms_dummy, &dpms_dummy) &&
      DPMSCapable(dpy) &&
      dpms_on) {
    DPMSEnable(dpy);
  }
#endif
}

cScreensaver::cScreensaver(Display *dpy) {
  this->dpy = dpy;

  XA_SCREENSAVER_VERSION = XInternAtom(dpy, "_SCREENSAVER_VERSION", False);
  XA_SCREENSAVER = XInternAtom(dpy, "SCREENSAVER", False);
  XA_DEACTIVATE = XInternAtom(dpy, "DEACTIVATE", False);

  window = FindWindow();
  if (window) {
    dsyslog("[softdevice-xscreensaver]: window is 0x%lx\n", window);
  } else {
    dsyslog("[softdevice-xscreensaver]: xscreensaver not running\n");
  }
  disabled = false; // don't disable screensaver until asked to
  last = 0;
}

void cScreensaver::DisableScreensaver(bool disable) {
  struct timeval current;
  gettimeofday(&current, NULL);
  last = current.tv_sec;
  this->disabled = disable;

#ifdef LIBXDPMS_SUPPORT
  if ((DPMSQueryExtension(dpy, &dpms_dummy, &dpms_dummy)) && (DPMSCapable(dpy))) {
    Status stat;
    if (disable) {
      dsyslog("[softdevice-xscreensaver]: disabling DPMS\n");
      DPMSInfo(dpy, &dpms_state, &dpms_on);
      stat = DPMSDisable(dpy);
      dsyslog("[softdevice-xscreensaver]: disabling DPMS stat: %d\n", stat);
    } else {
      dsyslog("[softdevice-xscreensaver]: reenabling DPMS\n");
      if (!DPMSEnable(dpy)) {
        dsyslog("[softdevice-xscreensaver]: DPMS not available?\n");
      } else {
        // According to mplayer sources: DPMS does not seem to be enabled unless we call DPMSInfo
        DPMSForceLevel(dpy, DPMSModeOn);
        DPMSInfo(dpy, &dpms_state, &dpms_on);
        if (dpms_on) {
          dsyslog("[softdevice-xscreensaver]: Successfully enabled DPMS\n");
        } else {
          dsyslog("[softdevice-xscreensaver]: Could not enable DPMS\n");
        }
      }
    }
  }
#endif
}

void cScreensaver::MaybeSendDeactivate(void) {
  if (window && disabled) {
    struct timeval current;
    gettimeofday(&current, NULL);
    if (current.tv_sec - last > INTERVAL) {
      last = current.tv_sec;
      //dsyslog("[softdevice-xscreensaver]: sending xscreensaver deactivation command DPMS\n");

      XEvent event;
      event.xany.type = ClientMessage;
      event.xclient.display = dpy;
      event.xclient.window = window;
      event.xclient.message_type = XA_SCREENSAVER;
      event.xclient.format = 32;
      memset(&event.xclient.data, 0, sizeof(event.xclient.data));
      event.xclient.data.l[0] = XA_DEACTIVATE;
      if (!XSendEvent (dpy, window, False, 0L, &event)) {
        esyslog("[softdevice-xscreensaver]: failed to send deactivation command\n");
        return;
      }
      XSync (dpy, 0);
    }
  }
}

int cScreensaver::FindWindow() {
  unsigned int i;
  Window root = RootWindowOfScreen (DefaultScreenOfDisplay (dpy));
  Window root2, parent, *kids;
  unsigned int nkids;
   
  if (! XQueryTree (dpy, root, &root2, &parent, &kids, &nkids))
    esyslog("[softdevice-xscreensaver]: unexpected error looking up xscreensaver window\n");
  if (root != root2)
    esyslog("[softdevice-xscreensaver]: unexpected error looking up xscreensaver window\n");
  if (parent)
    esyslog("[softdevice-xscreensaver]: unexpected error looking up xscreensaver window\n");
  if (! (kids && nkids))
    return 0;
  for (i = 0; i < nkids; i++) {
    Atom type;
    int format;
    unsigned long nitems, bytesafter;
    unsigned char *v;
    int status;

/* We're walking the list of root-level windows and trying to find
   the one that has a particular property on it.  We need to trap
   BadWindows errors while doing this, because it's possible that
   some random window might get deleted in the meantime.  (That
   window won't have been the one we're looking for.)
*/
    XSync (dpy, False);
    if (old_handler)
      esyslog("[softdevice-xscreensaver]: unexpected error looking up xscreensaver window\n");
    got_badwindow = False;
    old_handler = XSetErrorHandler (BadWindow_ehandler);
    status = XGetWindowProperty(dpy, kids[i], XA_SCREENSAVER_VERSION,
      0, 200, False, XA_STRING, &type, &format, &nitems, &bytesafter, (unsigned char **) &v);
    XSync (dpy, False);
    XSetErrorHandler (old_handler);
    old_handler = 0;

    if (got_badwindow) {
      status = BadWindow;
      got_badwindow = False;
    }

    if (status == Success && type != None) {
      return kids[i];
    }
  }
  return 0;
}
