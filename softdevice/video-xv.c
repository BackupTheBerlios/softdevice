/*
 * video-xv.c: A plugin for the Video Disk Recorder providing output
 *             capabilities via Xv
 *
 * Copyright Stefan Lucke, Roland Praml
 *
 * Xv code is taken from libdv  http://sourceforge.net/projects/libdv/
 * libdv is now LGPL. Let's see/discuss if that will get in trouble
 * with GPL of this plugin.
 *
 * Original copyright of display.c of libdv is:
 *     Copyright (C) Charles 'Buck' Krasic - April 2000
 *     Copyright (C) Erik Walthinsen - April 2000
 *
 * $Id: video-xv.c,v 1.57 2006/07/16 02:09:22 iampivot Exp $
 */

#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <fcntl.h>
//#include <vdr/plugin.h>
#include "video-xv.h"
#include "xscreensaver.h"
#include "utils.h"
#include "setup-softdevice.h"

#define PATCH_VERSION "2006-04-24"

static pthread_mutex_t  xv_mutex = PTHREAD_MUTEX_INITIALIZER;
cSoftRemote        *xvRemote = NULL;
static cScreensaver     *xScreensaver = NULL;

static XErrorHandler old_handler = 0;
static Bool got_error = False;

static int XvError_Handler(Display *dpy, XErrorEvent *error) {
  esyslog("got error code: %d \n",error->error_code);
  got_error=true;
  if ( error->error_code == BadMatch
       || error->error_code== BadAlloc ) {
          return 0;
  };

  if (old_handler)
    return (*old_handler)(dpy,error);
  else return 0;
};

/* ---------------------------------------------------------------------------
 */
/* ---------------------------------------------------------------------------
 */
cXvPortAttributeStore::cXvPortAttributeStore() {
  portAttributes = NULL;
  portAttributeSaveValues = NULL;
  portAttributeCurrentValues = NULL;
  portAttributeAtoms = NULL;
  portAttributeCount = 0;
  currBrightness = currContrast = currHue = currSaturation = 0;
}

/* ---------------------------------------------------------------------------
 */
cXvPortAttributeStore::~cXvPortAttributeStore() {
  Restore();

  portAttributeCount = 0;
  if (portAttributeSaveValues) {
    free (portAttributeSaveValues);
    portAttributeSaveValues = NULL;
  }
  if (portAttributeCurrentValues) {
    free (portAttributeCurrentValues);
    portAttributeCurrentValues = NULL;
  }


  if (portAttributeAtoms) {
    for (int i = 0; i < portAttributeCount; ++i) {
      if (portAttributeAtoms[i]) {
        XFree((void *)portAttributeAtoms[i]);
        portAttributeAtoms[i] = (Atom) NULL;
      }
    }
    free (portAttributeAtoms);
    portAttributeAtoms = NULL;
  }

  portAttributeCount = 0;
}

/* ---------------------------------------------------------------------------
 */
void cXvPortAttributeStore::SetXInfo(Display      *dpy,
                                     XvPortID     port,
                                     cSetupStore  *setupStore)
{
  this->dpy = dpy;
  this->port = port;
  this->setupStore = setupStore;
}

/* ---------------------------------------------------------------------------
 */
void cXvPortAttributeStore::SetValue(char *name, int value)
{
  for (int i = 0; i < portAttributeCount; ++i)
  {
    if (!strcmp(name,portAttributes[i].name))
    {
      if (value <= portAttributes[i].max_value &&
          value >= portAttributes[i].min_value)
      {
        portAttributeCurrentValues[i] = value;
        XvSetPortAttribute(dpy,port,portAttributeAtoms[i],portAttributeCurrentValues[i]);
      }
      return;
    }
  }
}

/* ---------------------------------------------------------------------------
 */
void cXvPortAttributeStore::SetValuePercent(char *name, int value)
{
  for (int i = 0; i < portAttributeCount; ++i)
  {
    if (!strcmp(name,portAttributes[i].name))
    {
      value = (int) ((double) portAttributes[i].min_value +
                      ((double) portAttributes[i].max_value -
                        (double) portAttributes[i].min_value) *
                      (double) value / 100.0);
      if (value <= portAttributes[i].max_value &&
          value >= portAttributes[i].min_value)
      {
        portAttributeCurrentValues[i] = value;
        XvSetPortAttribute(dpy,port,portAttributeAtoms[i],portAttributeCurrentValues[i]);
      }
      return;
    }
  }
}

/* ---------------------------------------------------------------------------
 */
void cXvPortAttributeStore::SetColorkey(int value)
{
  for (int i = 0; i < portAttributeCount; ++i)
  {
    if (!strcmp("XV_COLORKEY",portAttributes[i].name))
    {
      portAttributeCurrentValues[i] = value;
      XvSetPortAttribute(dpy,port,portAttributeAtoms[i],portAttributeCurrentValues[i]);
      return;
    }
  }
}

/* ---------------------------------------------------------------------------
 */
void cXvPortAttributeStore::Increment(char *name)
{
  for (int i = 0; i < portAttributeCount; ++i)
  {
    if (!strcmp(name,portAttributes[i].name))
    {
      if (portAttributes[i].max_value > portAttributeCurrentValues[i])
      {
        portAttributeCurrentValues[i]++;
        XvSetPortAttribute(dpy,port,portAttributeAtoms[i],portAttributeCurrentValues[i]);
      }
      return;
    }
  }
}

/* ---------------------------------------------------------------------------
 */
void cXvPortAttributeStore::Decrement(char *name)
{
  for (int i = 0; i < portAttributeCount; ++i)
  {
    if (!strcmp(name,portAttributes[i].name))
    {
      if (portAttributes[i].min_value < portAttributeCurrentValues[i])
      {
        portAttributeCurrentValues[i]--;
        XvSetPortAttribute(dpy,port,portAttributeAtoms[i],portAttributeCurrentValues[i]);
      }
      return;
    }
  }
}

/* ---------------------------------------------------------------------------
 */
void cXvPortAttributeStore::Save()
{
  portAttributes = XvQueryPortAttributes(dpy,port,&portAttributeCount);
  if (portAttributes)
  {
    portAttributeAtoms = (Atom *) calloc(sizeof (Atom *),portAttributeCount);
    portAttributeCurrentValues = (int *) calloc(sizeof(int),portAttributeCount);
    portAttributeSaveValues = (int *) calloc(sizeof(int),portAttributeCount);
    for (int i = 0; i < portAttributeCount; i++)
    {
      portAttributeAtoms[i] = XInternAtom(dpy,portAttributes[i].name,False);
      if (portAttributes[i].flags & XvGettable)
      {
        XvGetPortAttribute(dpy,port,portAttributeAtoms[i],&portAttributeSaveValues[i]);
        portAttributeCurrentValues[i] = portAttributeSaveValues[i];
      }
      if (!strcmp(portAttributes[i].name, "XV_BRIGHTNESS"))
        setupStore->vidCaps |= CAP_BRIGHTNESS;
      if (!strcmp(portAttributes[i].name, "XV_CONTRAST"))
        setupStore->vidCaps |= CAP_CONTRAST;
      if (!strcmp(portAttributes[i].name, "XV_HUE"))
        setupStore->vidCaps |= CAP_HUE;
      if (!strcmp(portAttributes[i].name, "XV_SATURATION"))
        setupStore->vidCaps |= CAP_SATURATION;

      dsyslog("[XvVideoOut]:"
              "   %-25s %-4sXvGettable %-4sXvSettable "
              "(%8d [0x%08x] - %8d [0x%08x]) (%8d [0x%08x]",
              portAttributes[i].name,
              (portAttributes[i].flags & XvGettable) ? "": "NOT-",
              (portAttributes[i].flags & XvSettable) ? "": "NOT-",
              portAttributes[i].min_value,
              portAttributes[i].min_value,
              portAttributes[i].max_value,
              portAttributes[i].max_value,
              portAttributeCurrentValues[i],
              portAttributeCurrentValues[i]);

    }
  }
}

/* ---------------------------------------------------------------------------
 */
bool cXvPortAttributeStore::HasAttribute(char *name)
{
  for (int i = 0; i < portAttributeCount; ++i)
  {
    if (!strcmp(name,portAttributes[i].name))
    {
      return true;
    }
  }
  return false;
}

/* ---------------------------------------------------------------------------
 */
void cXvPortAttributeStore::CheckVideoParmChange()
{
    int changed = 0;

  if (currBrightness != setupStore->vidBrightness)
  {
    changed++;
    currBrightness = setupStore->vidBrightness;
    SetValuePercent("XV_BRIGHTNESS", currBrightness);
  }
  if (currContrast != setupStore->vidContrast)
  {
    changed++;
    currContrast = setupStore->vidContrast;
    SetValuePercent("XV_CONTRAST", currContrast);
  }
  if (currHue != setupStore->vidHue)
  {
    changed++;
    currHue = setupStore->vidHue;
    SetValuePercent("XV_HUE", currHue);
  }
  if (currSaturation != setupStore->vidSaturation)
  {
    changed++;
    currSaturation = setupStore->vidSaturation;
    SetValuePercent("XV_SATURATION", currSaturation);
  }
  if (changed)
    XSync(dpy, False);
}

/* ---------------------------------------------------------------------------
 */
void cXvPortAttributeStore::Restore()
{
  dsyslog("[XvVideoOut]: restoring attribute values");
  for (int i = 0; i < portAttributeCount; i++)
  {
    if (portAttributes[i].flags & XvSettable)
    {
      dsyslog("[XvVideoOut]: %-25s %8d [0x%08x] ",
               portAttributes[i].name,
               portAttributeSaveValues[i],
               portAttributeSaveValues[i]);
      if (XvSetPortAttribute(dpy,port,portAttributeAtoms[i],portAttributeSaveValues[i]) != Success) {
        dsyslog("[XvVideoOut]: restore FAILED");
      }
    }
  }
  if (portAttributes)
    XSync(dpy,False);
}
/* ---------------------------------------------------------------------------
 */
void cXvVideoOut::toggleFullScreen(void)
{
    XEvent      e;
    XSizeHints  wmHints;
    Atom        wmAtom;
    int         x, y, w, h;

  fullScreen = !fullScreen;

  if (fullScreen)
  {
    // window -> fullscreen
      XWindowAttributes winAttr;
      Window            dummyWin;


    if (XGetWindowAttributes(dpy, win, &winAttr))
    {
        XVisualInfo vistemp, *vinfo;
        int         dummy;

      vinfo = XGetVisualInfo(dpy, VisualIDMask, &vistemp, &dummy);
      XTranslateCoordinates (dpy, win, winAttr.root,
                             -winAttr.border_width,
                             -winAttr.border_width,
                             &old_x, &old_y, &dummyWin);
    }
    else
    {
      old_x       = dx;
      old_y       = dy;
    }
    old_dwidth  = dwidth;
    old_dheight = dheight;

#if XINERAMA_SUPPORT
    if (xin_mode)
    {
        int i;

      i = (xin_screen != -1) ? xin_screen : 0;
      x = xin_screen_info[i]. x_org;
      y = xin_screen_info[i]. y_org;
      w = xin_screen_info[i]. width;
      h = xin_screen_info[i]. height;
    }
    else
#endif
    {
      x = y = 0;
      w = DisplayWidth(dpy,DefaultScreen(dpy));
      h = DisplayHeight(dpy,DefaultScreen(dpy));
    }
  }
  else
  {
    // fullscreen -> window
    x = old_x;
    y = old_y;
    w = old_dwidth;
    h = old_dheight;
  }

  wmHints.flags = PSize | PPosition | PWinGravity;
  wmHints.win_gravity = StaticGravity;
  wmHints.x = x;
  wmHints.y = y;
  wmHints.width  = w;
  wmHints.height = h;
  wmHints.max_width  = 0;
  wmHints.max_height = 0;

  if ((wmAtom = XInternAtom (dpy, "_MOTIF_WM_HINTS", False)))
  {
    long  mwmHints [5];

    mwmHints [0] = 2;
    mwmHints [2] = (fullScreen) ? 0 : 1;
    mwmHints [1] = mwmHints [3] = mwmHints [4] = 0;
    XChangeProperty (dpy, win, wmAtom, wmAtom,
                     32, PropModeReplace, (unsigned char *) mwmHints, 5);
  }

  XSetWMNormalHints(dpy, win, &wmHints);

  memset(&e,0,sizeof(e));
  e.xclient.type = ClientMessage;
  e.xclient.message_type = net_wm_STATE;
  e.xclient.display = dpy;
  e.xclient.window = win;
  e.xclient.format = 32;
  e.xclient.data.l[0] = (fullScreen) ? 1 : 0;
  if (net_wm_STATE_ABOVE)
    e.xclient.data.l[1] = net_wm_STATE_ABOVE;
  else //if (net_wm_STATE_STAYS_ON_TOP)
    e.xclient.data.l[1] = net_wm_STATE_STAYS_ON_TOP;
  if (net_wm_STATE_FULLSCREEN)
    e.xclient.data.l[1] = net_wm_STATE_FULLSCREEN;
  XSendEvent(dpy, DefaultRootWindow(dpy), False, SubstructureRedirectMask, &e);

  XReparentWindow (dpy, win, DefaultRootWindow(dpy), x, y);
  XMoveResizeWindow (dpy, win, x, y, w, h);

  //XMapRaised (dpy, win);
  //XRaiseWindow (dpy, win);

  XFlush (dpy);
  xScreensaver->DisableScreensaver(fullScreen); // enable of disable based on fullScreen state
}

/* ---------------------------------------------------------------------------
 */
void cXvVideoOut::AdjustDisplayRatio()
{
    double              displayAspect, displayRatio;

  /* --------------------------------------------------------------------------
   * set PAR values
   */
  displayAspect = (double) DisplayWidthMM(dpy, DefaultScreen(dpy)) /
                    (double) DisplayHeightMM(dpy, DefaultScreen(dpy));
  if (xin_mode)
    displayRatio  = (double) GetScreenWidth() / (double) GetScreenHeight();
  else
    displayRatio  = (double) DisplayWidth(dpy, DefaultScreen(dpy)) /
                      (double) DisplayHeight(dpy, DefaultScreen(dpy));

  SetParValues(displayAspect, displayRatio);

}

/* ---------------------------------------------------------------------------
 */
void cXvVideoOut::AdjustXineramaScreen()
{
#if XINERAMA_SUPPORT
    XineramaScreenInfo  *new_screen_info;
    int                 new_num_screens;

  if (!xin_mode)
    return;

  new_screen_info = XineramaQueryScreens (dpy, &new_num_screens);
  if (new_screen_info)
  {
    if (xin_screen_info)
      XFree(xin_screen_info);

    xin_screen_info = new_screen_info;
    xin_num_screens = new_num_screens;
  }

  for (int i = 0; i < xin_num_screens; ++i)
  {
    if (dx >= xin_screen_info[i]. x_org &&
        dx <= (xin_screen_info[i]. x_org +
                xin_screen_info[i]. width) &&
        dy >= xin_screen_info[i]. y_org &&
        dy <= (xin_screen_info[i]. y_org +
                xin_screen_info[i]. height))
    {
      if (i != xin_screen)
      {
        fprintf(stderr,
                "[XvVideoOut]: Xinerama-Screen changed to %d\n", i);
        xin_screen = i;
      }
      break;
    }
  }

  AdjustDisplayRatio();

#endif
}

/* ---------------------------------------------------------------------------
 */
int cXvVideoOut::GetScreenWidth()
{
#if XINERAMA_SUPPORT
  if (xin_mode)
    return xin_screen_info[(xin_screen != -1) ? xin_screen : 0]. width;
#endif

  return DisplayWidth(dpy,DefaultScreen(dpy));
}

/* ---------------------------------------------------------------------------
 */
int cXvVideoOut::GetScreenHeight()
{
#if XINERAMA_SUPPORT
  if (xin_mode)
    return xin_screen_info[(xin_screen != -1) ? xin_screen : 0]. height;
#endif

  return DisplayHeight(dpy,DefaultScreen(dpy));
}

//#define EVDEB(out...) {printf("EVDEB:");printf(out);}
#define EVDEB(out...)

/* ---------------------------------------------------------------------------
 */
void cXvVideoOut::ProcessEvents ()
{

    char            buffer [80];
    int             map_count = 0;
    XComposeStatus  compose;
    KeySym          keysym;
    struct timeval  current_time;
    XEvent            event;

  if (!videoInitialized)
    return;

  pthread_mutex_lock(&xv_mutex);
  while (XCheckMaskEvent (dpy, /* win, */
                                 PointerMotionMask |
                                 ButtonPressMask |
                                 KeyPressMask |
                                 ExposureMask |
                                 ConfigureNotify |
                                 StructureNotifyMask,
                                 &event))
  {
    switch (event.type)
    {
      case MotionNotify:
      case ButtonPress:
        gettimeofday(&current_time, NULL);
        motion_time = current_time.tv_sec;
        if(event.xbutton.button==Button1) {
          if(button_time - current_time.tv_sec == 0) {
            toggleFullScreen();
          }
          button_time = current_time.tv_sec;
        }
        if(cursor_visible == False) {
          XUndefineCursor(dpy, win);
          cursor_visible = True;
        }
        break;
      case ConfigureNotify:
        EVDEB("ConfigureNotify\n");
        map_count++;
        dx = event.xconfigure.x;
        dy = event.xconfigure.y;
        dwidth = event.xconfigure.width;
        dheight = event.xconfigure.height;
        AdjustXineramaScreen();
        RecalculateAspect();

        if (toggleInProgress &&
            ((fullScreen &&
               dwidth == GetScreenWidth() &&
               dheight == GetScreenHeight()) ||
              (!fullScreen &&
               dwidth == old_dwidth &&
               dheight == old_dheight))) {
          toggleInProgress = 0;
        }
        break;
      case KeyPress:
        if(cursor_visible == False) {
          XUndefineCursor(dpy, win);
          cursor_visible = True;
        }
        gettimeofday(&current_time, NULL);
        button_time = current_time.tv_sec;

        XLookupString (&event.xkey, buffer, 80, &keysym, &compose);
        switch (keysym)
        {
          case XK_Shift_L: case XK_Shift_R: case XK_Control_L: case XK_Control_R:
          case XK_Caps_Lock: case XK_Shift_Lock: case XK_Meta_L:
          case XK_Meta_R: case XK_Alt_L: case XK_Alt_R:
            break;
          case 'f':
            if (!toggleInProgress) {
              toggleFullScreen();
              toggleInProgress++;
            }
            break;
          case 'b':
            if (xv_initialized)
              attributeStore.Decrement("XV_BRIGHTNESS");
            break;
          case 'B':
            if (xv_initialized)
              attributeStore.Increment("XV_BRIGHTNESS");
            break;
          case 'c':
            if (xv_initialized)
              attributeStore.Decrement("XV_CONTRAST");
            break;
          case 'C':
            if (xv_initialized)
              attributeStore.Increment("XV_CONTRAST");
            break;
          case 'h':
            if (xv_initialized)
              attributeStore.Decrement("XV_HUE");
            break;
          case 'H':
            if (xv_initialized)
              attributeStore.Increment("XV_HUE");
            break;
          case 's':
            if (xv_initialized)
              attributeStore.Decrement("XV_SATURATION");
            break;
          case 'S':
            if (xv_initialized)
              attributeStore.Increment("XV_SATURATION");
            break;
          case 'o':
            if (xv_initialized)
              attributeStore.Decrement("XV_OVERLAY_ALPHA");
            break;
          case 'O':
            if (xv_initialized)
              attributeStore.Increment("XV_OVERLAY_ALPHA");
            break;
          case 'g':
            if (xv_initialized)
              attributeStore.Decrement("XV_GRAPHICS_ALPHA");
            break;
          case 'G':
            if (xv_initialized)
              attributeStore.Increment("XV_GRAPHICS_ALPHA");
            break;
          case 'a':
            if (xv_initialized)
              attributeStore.Decrement("XV_ALPHA_MODE");
            break;
          case 'A':
            if (xv_initialized)
              attributeStore.Increment("XV_ALPHA_MODE");
            break;
#ifdef SUSPEND_BY_KEY
          case 'r':
          case 'R':
            setupStore->shouldSuspend=!setupStore->shouldSuspend;
#endif
          default:
            if (xvRemote &&
                !setupStore->CatchRemoteKey(xvRemote->Name(), keysym)) {
              xvRemote->PutKey (keysym);
            }
            break;
        }
        break;

      case MapNotify:
        EVDEB("MapNotify\n");
        map_count++;
        if (videoInitialized) {
          XSetInputFocus(dpy,
                   win,
                   RevertToParent,
                   CurrentTime);
        }
        break;
      case UnmapNotify:
        EVDEB("UnmapNotify\n");
        if (xv_initialized)
          XvStopVideo (dpy,port,win);
        if(cursor_visible == False) {
          XUndefineCursor(dpy, win);
          cursor_visible = True;
        }
        break;
      case Expose:
        EVDEB("Expose\n");
        map_count++;
        break;
      default:
        EVDEB("unhandled event: %d\n",event.type);
        break;
    }
  }

  if (xv_initialized) {
    if (map_count) {
      XClearArea (dpy, win, 0, 0, 0, 0, False);
      ShowOSD();
      PutXvImage();
      XSync(dpy, False);
    }
    attributeStore.CheckVideoParmChange();
  }

  if(cursor_visible == True) {
    gettimeofday(&current_time, NULL);
    if(current_time.tv_sec - motion_time >= 2) {
      XDefineCursor(dpy, win, hidden_cursor);
      cursor_visible = False;
    }
  }
  xScreensaver->MaybeSendDeactivate();
  pthread_mutex_unlock(&xv_mutex);
}

/* ---------------------------------------------------------------------------
 */
cXvVideoOut::cXvVideoOut(cSetupStore *setupStore)
              : cVideoOut(setupStore)
{
  OSDpresent = false;
  OSDpseudo_alpha = true;
  toggleInProgress = 0;
  xv_initialized=false;
  /* -------------------------------------------------------------------------
   * could be specified by argv ! TODO
   */
  display_aspect = current_aspect = setupStore->xvAspect;
  screenPixelAspect = -1;
  xvWidth  = width  = XV_SRC_WIDTH;
  xvHeight = height = XV_SRC_HEIGHT;

  format = FOURCC_YV12;
  use_xv_port = 0;
  w_name = "vdr";
  i_name = "vdr";

  Xres=width;
  Yres=height;

  xin_mode = false;
  xin_screen = -1;

#if XINERAMA_SUPPORT
  xin_screen_info = NULL;
#endif

  /*
   * default settings: source, destination and logical widht/height
   * are set to our well known dimensions.
   */
  fwidth = lwidth = old_dwidth = dwidth = swidth = width;
  fheight = lheight = old_dheight = dheight = sheight = height;

  if (current_aspect == DV_FORMAT_NORMAL) {
    lwidth = dwidth = XV_DEST_WIDTH_4_3;
    lheight = dheight = XV_DEST_HEIGHT;
  } else {
    lwidth = dwidth = XV_DEST_WIDTH_16_9;
  }
}

/* ---------------------------------------------------------------------------
 */
bool cXvVideoOut::Initialize (void)
{
    int                 scn_id, rc;
    XWMHints            wmhints;
    XSizeHints          hints;
    XGCValues           values;
    XTextProperty       x_wname, x_iname;
    struct timeval      current_time;
    int                 retry = 0;

  dsyslog("[XvVideoOut]: patch version (%s)", PATCH_VERSION);

  if(!(dpy = XOpenDisplay(NULL))) {
    fprintf(stderr, "[XvVideoOut]: Could not connect to X-server");
    dsyslog("[XvVideoOut]: Could not connect to X-server");
    return false;
  } /* if */

  rwin = DefaultRootWindow(dpy);
  scn_id = DefaultScreen(dpy);

  /* -------------------------------------------------------------------------
   * Check if xinerama is active
   */
#if XINERAMA_SUPPORT
  if (XineramaIsActive(dpy))
  {
      int                 i;

    xin_screen_info = XineramaQueryScreens (dpy, &xin_num_screens);
    if (xin_screen_info)
    {
      for (i = 0; i < xin_num_screens; ++i)
      {
        fprintf(stderr,
                "[XvVideoOut]: Xinerama Screen %d: %d,%d  %dx%d\n",
                i,
                xin_screen_info[i]. x_org,
                xin_screen_info[i]. y_org,
                xin_screen_info[i]. width,
                xin_screen_info[i]. height);
      }
      xin_mode = xin_num_screens > 0;
    } else {
      fprintf (stderr, "[XvVideoOut]: NO Xinerama Screens present\n");
    }
  }
#endif
  /* -------------------------------------------------------------------------
   * default settings which allow arbitraray resizing of the window
   */
  hints.flags = PSize | PMaxSize | PMinSize;
  hints.min_width = width / 16;
  hints.min_height = height / 16;

  /* -------------------------------------------------------------------------
   * maximum dimensions for Xv support are about 2048x2048
   */
  hints.max_width = 2048;
  hints.max_height = 2048;

  wmhints.input = True;
  wmhints.flags = InputHint;
  XStringListToTextProperty(&w_name, 1 ,&x_wname);
  XStringListToTextProperty(&i_name, 1 ,&x_iname);

  AdjustDisplayRatio();

  if (flags & XV_FORMAT_MASK) {
    hints.flags |= PAspect;
    if (flags & XV_FORMAT_WIDE) {
      hints.min_aspect.x = hints.max_aspect.x = XV_DEST_WIDTH_16_9;
    } else {
      hints.min_aspect.x = hints.max_aspect.x = XV_DEST_WIDTH_4_3;
    }
    hints.min_aspect.y = hints.max_aspect.y = XV_DEST_HEIGHT;
  }

  win = XCreateSimpleWindow(dpy,
                            rwin,
                            0, 0,
                            dwidth, dheight,
                            0,
                            XWhitePixel(dpy, scn_id),
                            XBlackPixel(dpy, scn_id));
  XSetWMProperties(dpy, win,
                   &x_wname, &x_iname,
                   NULL, 0,
                   &hints, &wmhints, NULL);

  XSelectInput(dpy, win, KeyPressMask | ExposureMask |
                         ConfigureNotify | StructureNotifyMask |
                         PointerMotionMask | ButtonPressMask);

  XMapRaised(dpy, win);
  //XNextEvent(dpy, &event);

  gc = XCreateGC(dpy, win, 0, &values);

  /*
   * Create transparent cursor
   */
  Pixmap cursor_mask;
  XColor dummy_col;

  const char cursor_data[] = { 0x0 };

  cursor_mask = XCreateBitmapFromData(dpy, win, cursor_data, 1, 1);
  hidden_cursor = XCreatePixmapCursor(dpy, cursor_mask, cursor_mask,
                                      &dummy_col, &dummy_col, 0, 0);
  XFreePixmap(dpy, cursor_mask);
  cursor_visible = True;
  gettimeofday(&current_time, NULL);
  button_time = motion_time = current_time.tv_sec;

  net_wm_STATE_FULLSCREEN = XInternAtom(dpy, "_NET_WM_STATE_FULLSCREEN", False);
  net_wm_STATE_STAYS_ON_TOP = XInternAtom(dpy, "_NET_WM_STATE_STAYS_ON_TOP", False);
  net_wm_STATE_ABOVE = XInternAtom(dpy, "_NET_WM_STATE_ABOVE", False);
  net_wm_STATE = XInternAtom(dpy, "_NET_WM_STATE", False);
  fullScreen = false;

  /* -----------------------------------------------------------------------
   * now let's do some OSD initialisations
   */
  char *dispName=XDisplayName(NULL);
  bool isLocal=true;

  dispName=rindex(dispName,':');
  if ( dispName && atoi(dispName + 1) > 9 )
          isLocal = false;

  osd_max_width=DisplayWidth(dpy,DefaultScreen(dpy));
  osd_max_height=DisplayHeight(dpy,DefaultScreen(dpy));
  useShm=XShmQueryExtension(dpy) && isLocal;

  old_handler = XSetErrorHandler(XvError_Handler);

init_osd:
  if (useShm) {

          osd_image = XShmCreateImage (dpy,
                          XDefaultVisual (dpy, scn_id),
                          XDefaultDepth (dpy, scn_id),
                          ZPixmap,NULL,
                          &osd_shminfo,
                          osd_max_width,osd_max_height);
                          //width, height);
          if (osd_image) {
                  dsyslog("[XvVideoOut]: Initialize XShmCreateImage Successful (%p)", osd_image);
          } else {
                  dsyslog("[XvVideoOut]: Initialize ERROR: XShmCreateImage FAILED !");
          }

          osd_shminfo.shmid = shmget (IPC_PRIVATE,
                          osd_image->bytes_per_line*osd_max_height,
                          //osd_image->bytes_per_line*height,
                          IPC_CREAT | 0777);
          fprintf(stderr,"[XvVideoOut]: osd_image shmid = %d\n",osd_shminfo.shmid);
          if (osd_shminfo.shmid == -1) {
                  dsyslog("[XvVideoOut]: Initialize ERROR: shmget FAILED !");
          } else {
                  dsyslog("[XvVideoOut]: Initialize shmget Successful (%d bytes)",
                                  osd_image->bytes_per_line*height);
          }

          osd_shminfo.shmaddr = (char *) shmat(osd_shminfo.shmid, NULL, 0);
          osd_buffer = (unsigned char *) osd_shminfo.shmaddr;
          osd_image->data = (char *) osd_buffer;

          if (osd_image->data == (char *) -1L) {
                  dsyslog("[XvVideoOut]: Initialize ERROR: shmat FAILED !");
          } else {
                  dsyslog("[XvVideoOut]: Initialize shmat Successful");
          }

          osd_shminfo.readOnly = False;

          rc = XShmAttach(dpy, &osd_shminfo);

          if (osd_shminfo. shmid > 0)
                  shmctl (osd_shminfo. shmid, IPC_RMID, 0);
  } else {
          osd_image = XGetImage(dpy, win, 0, 0,
                          osd_max_width, osd_max_height, AllPlanes,
                          ZPixmap);
          if (osd_image) {
                  dsyslog("[XvVideoOut]: Initialize XGetImage Successful (%p)", osd_image);
		              osd_buffer = (unsigned char *) osd_image->data;
          } else {
                  dsyslog("[XvVideoOut]: Initialize ERROR: XGetImage FAILED !");
		              osd_buffer = NULL;
          }
  };
  if (osd_image) {
          Bpp=osd_image->bits_per_pixel;
          fprintf(stderr, "[XvVideoOut]: got osd_image: width"
                          " %d height %d, bytes per line %d\n",
                          osd_max_width, osd_max_height,
                          osd_image->bytes_per_line);
  };
  rc = XClearArea (dpy, win, 0, 0, 0, 0, True);

  rc = XSync(dpy, False);

  if (got_error)
  {
    got_error=0;retry++;
    osd_max_width=720;
    osd_max_height=536;
    if (retry<2) {
            fprintf(stderr,"Error initializing osd image. Retry.\n");
            goto init_osd;
    };
    fprintf(stderr,"Error initializing osd image. Exit.\n");
    exit(-1);
  };
  if (old_handler) {
          XSetErrorHandler(old_handler);
          old_handler = NULL;
  };


  if (!xScreensaver)
  {
    xScreensaver = new cScreensaver(dpy);
  }

  pthread_mutex_lock(&xv_mutex);
  /* -------------------------------------------------------------------------
   * get up our remote running
   */
  if (!xvRemote)
  {
    xvRemote = new cSoftRemote ("softdevice-xv");
  }

  dsyslog("[XvVideoOut]: initialized OK");

  if (setupStore->xvFullscreen)
  {
    toggleFullScreen();
    setupStore->xvFullscreen=0;
  }

  pthread_mutex_unlock(&xv_mutex);
  videoInitialized = true;
  return true;
}

/* ---------------------------------------------------------------------------
 */
bool cXvVideoOut::Reconfigure(int format)
{
    int                 fmt_cnt, k, rc,
                        got_port, got_fmt;
    unsigned int        ad_cnt,
                        i;
    XvAdaptorInfo       *ad_info;
    XvImageFormatValues *fmt_info;
    int                 len=0;

  pthread_mutex_lock(&xv_mutex);

  /*
   * So let's first check for an available adaptor and port
   */
  if(Success == XvQueryAdaptors(dpy, rwin, &ad_cnt, &ad_info)) {

    for(i = 0, got_port = False; i < ad_cnt; ++i) {
      dsyslog("[XvVideoOut]: %s: available ports %ld - %ld",
              ad_info[i].name,
              ad_info[i].base_id,
              ad_info[i].base_id +
              ad_info[i].num_ports - 1);

      if (use_xv_port != 0 &&
          (use_xv_port < ad_info[i].base_id ||
           use_xv_port >= ad_info[i].base_id+ad_info[i].num_ports)) {
        dsyslog("[XvVideoOut]: %s: skipping (looking for port %i)",
                ad_info[i].name,
                use_xv_port);
        continue;
      }

      if (!(ad_info[i].type & XvImageMask)) {
        dsyslog("[XvVideoOut]: %s: XvImage NOT in capabilty list (%s%s%s%s%s )",
                ad_info[i].name,
                (ad_info[i].type & XvInputMask) ? " XvInput"  : "",
                (ad_info[i]. type & XvOutputMask) ? " XvOutput" : "",
                (ad_info[i]. type & XvVideoMask)  ?  " XvVideo"  : "",
                (ad_info[i]. type & XvStillMask)  ?  " XvStill"  : "",
                (ad_info[i]. type & XvImageMask)  ?  " XvImage"  : "");
        continue;
      } /* if */
      fmt_info = XvListImageFormats(dpy, ad_info[i].base_id,&fmt_cnt);
      if (!fmt_info || fmt_cnt == 0) {
        dsyslog("[XvVideoOut]: %s: NO supported formats", ad_info[i].name);
        continue;
      } /* if */
      for(got_fmt = False, k = 0; k < fmt_cnt; ++k) {
        if (format == fmt_info[k].id) {
          got_fmt = True;
          break;
        } /* if */
      } /* for */
      if (!got_fmt) {
        dsyslog("[XvVideoOut]: %s: format %#08x is NOT in format list:",
                ad_info[i].name,
                format);
        for (k = 0; k < fmt_cnt; ++k) {
          dsyslog("[XvVideoOut]:   %#08x[%s] ", fmt_info[k].id, fmt_info[k].guid);
        }
        continue;
      } /* if */

      for(port = ad_info[i].base_id, k = 0;
          (unsigned int) k < ad_info[i].num_ports;
          ++k, ++(port)) {
        if (use_xv_port != 0 && use_xv_port != port)
          continue;
        if(!XvGrabPort(dpy, port, CurrentTime)) {
            unsigned int    encodingCount, n;
            XvEncodingInfo  *encodingInfo;

          dsyslog("[XvVideoOut]: grabbed port %ld", port);
          got_port = True;
          XvQueryEncodings(dpy,ad_info[i].base_id,&encodingCount,&encodingInfo);
          for (n = 0; n < encodingCount; ++n)
          {
            if (!strcmp(encodingInfo[n].name, "XV_IMAGE"))
            {
              fprintf(stderr, "[XvVideoOut]: max area size %lu x %lu\n",
                      encodingInfo[n].width, encodingInfo[n].height);
              dsyslog("[XvVideoOut]: max area size %lu x %lu",
                      encodingInfo[n].width, encodingInfo[n].height);

              if (setupStore->xvMaxArea) {
                /* ------------------------------------------------------------
                 * adjust width to 8 byte boundary and height to an even
                 * number of lines.
                 */
                xvWidth  = (encodingInfo[n].width & ~7);
                xvHeight = (encodingInfo[n].height & ~1);
              } else {
                xvWidth  = XV_SRC_WIDTH;
                xvHeight = XV_SRC_HEIGHT;
              }
              fprintf(stderr, "[XvVideoOut]: using area size %d x %d\n",
                      xvWidth, xvHeight);
              dsyslog("[XvVideoOut]: using area size %d x %d",
                      xvWidth, xvHeight);
              len = xvWidth * xvHeight * 2;
            }
          }
          break;
        } /* if */
      } /* for */
      if(got_port)
        break;
    } /* for */

  } else {
    /* Xv extension probably not present */
    esyslog("[XvVideoOut]: (ERROR) no Xv extensions found! No video possible!");
    pthread_mutex_unlock(&xv_mutex);
    return true;
  } /* else */

  if(!ad_cnt) {
    esyslog("[XvVideoOut]: (ERROR) no adaptor found! No video possible!");
    pthread_mutex_unlock(&xv_mutex);
    return true;
  }
  if(!got_port) {
    esyslog("[XvVideoOut]: (ERROR) could not grab any port! No video possible!");
    pthread_mutex_unlock(&xv_mutex);
    return true;
  }

  attributeStore.SetXInfo(dpy,port,setupStore);
  attributeStore.Save();
  attributeStore.SetColorkey(0x00000000);
  attributeStore.SetValue("XV_AUTOPAINT_COLORKEY",0);

  /*
   * Now we do shared memory allocation etc..
   */
  int retry=0;
  old_handler = XSetErrorHandler(XvError_Handler);
retry_image:
  CreateXvImage(dpy,port,xv_image,shminfo,
                  format,xvWidth, xvHeight);

  switch (format) {
    case FOURCC_YV12:
    case FOURCC_I420:
      pixels[0] = (uint8_t *) (xv_image->data + xv_image->offsets[0]);
      pixels[1] = (uint8_t *) (xv_image->data + xv_image->offsets[1]);
      pixels[2] = (uint8_t *) (xv_image->data + xv_image->offsets[2]);
      break;
    case FOURCC_YUY2:
      pixels[0] = (uint8_t *) (xv_image->data + xv_image->offsets[0]);
      break;
    default:
      break;
  }

  this->format = format;

  ClearXvArea (0, 128, 128);
  rc = PutXvImage();
  rc = XClearArea (dpy, win, 0, 0, 0, 0, True);
  rc = XSync(dpy, False);

  if (got_error)
  {
    got_error=0;retry++;
    xvWidth=XV_SRC_WIDTH;
    xvHeight = XV_SRC_HEIGHT;
    if (retry<2) {
            fprintf(stderr,"Error initializing Xv. Retry.\n");
            goto retry_image;
    };
    fprintf(stderr,"Error intializing Xv. Exit.\n");
    exit(-1);
  };
  if (old_handler) {
          XSetErrorHandler(old_handler);
          old_handler=NULL;
  };


  pthread_mutex_unlock(&xv_mutex);

  videoInitialized = true;
  xv_initialized = 1;
  return true;
}

/* ---------------------------------------------------------------------------
 */
void cXvVideoOut::ClearXvArea(uint8_t y, uint8_t u, uint8_t v)
{
    uint8_t   tu;
    uint32_t  tyuy2, *pyuy2;

  switch (format) {
    case FOURCC_I420:
      tu = u;
      u = v;
      v = tu;
    case FOURCC_YV12:
      memset (pixels [0], y, xvWidth*xvHeight);
      memset (pixels [1], u, xvWidth*xvHeight/4);
      memset (pixels [2], v, xvWidth*xvHeight/4);
      break;
    case FOURCC_YUY2:
      tyuy2 = y + (u << 8) + (y << 16) + (v << 24);
      pyuy2 = (uint32_t *) pixels[0];

      for (int i = 0; i < xvWidth / 2; ++i)
        *pyuy2++ = tyuy2;

      for (int i = 1; i < xvHeight; ++i, pyuy2 += xvWidth / 2)
        fast_memcpy(pyuy2, pixels[0], xvWidth*2);

      break;
    default:
      break;
  }

}

/* ---------------------------------------------------------------------------
 */
void cXvVideoOut::CreateXvImage(Display *dpy,XvPortID port,
                  XvImage *&xv_image,
                  XShmSegmentInfo &shminfo,
                  int format, int &width, int &height) {
  int len=0;
  if (useShm)
  {
    xv_image = XvShmCreateImage(dpy, port,
        format, NULL,
        width, height,
        &shminfo);
    if (xv_image) {
      dsyslog("[XvVideoOut]: XvShmCreateImage Successful (%p)", xv_image);
    } else {
      dsyslog("[XvVideoOut]: ERROR: XvShmCreateImage FAILED !");
      exit(-1);
    }

    shminfo.shmid = shmget(IPC_PRIVATE,
        xv_image->data_size,
        IPC_CREAT | 0777);

    if (shminfo.shmid == -1) {
      dsyslog("[XvVideoOut]: ERROR: shmget FAILED !");
    } else {
      dsyslog("[XvVideoOut]: shmget Successful (%d bytes)",len);
    }

    /* compile fix for gcc 3.4.3
    */
    shminfo.shmaddr = (char *) shmat(shminfo.shmid, NULL, 0);
    xv_image->data = (char *) shminfo.shmaddr;

    if (xv_image->data == (char *) -1L) {
      dsyslog("[XvVideoOut]: ERROR: shmat FAILED !");
      exit(-1);
    } else {
      dsyslog("[XvVideoOut]: shmat Successful");
    }
    shminfo.readOnly = False;
    int rc = XShmAttach(dpy, &shminfo);
    dsyslog("[XvVideoOut]: XShmAttach    rc = %d %s",
        rc,(rc == 1) ? "(should be OK)":"(thats NOT OK!)");

    // request remove after detaching
    if (shminfo. shmid > 0)
      shmctl (shminfo. shmid, IPC_RMID, 0);
  }else
  {
    xv_image = XvCreateImage(dpy, port,format, NULL,
        width, height);
    if (!xv_image) {
      dsyslog("[XvVideoOut]: ERROR: XvCreateImage FAILED !");
      exit(-1);
    };

    xv_image->data = (char*)malloc(xv_image->data_size);
    if (!xv_image->data) {
      dsyslog("[XvVideoOut]: ERROR: malloc xv_image FAILED !");
      exit(-1);
    };
  };
};

/*----------------------------------------------------------------------------
 */

int cXvVideoOut::PutXvImage() {
  if (useShm)
    return XvShmPutImage(dpy, port,
                     win, gc,
                     xv_image,
                     sxoff, syoff,      /* sx, sy */
                     swidth, sheight,   /* sw, sh */
                     lxoff,  lyoff,     /* dx, dy */
                     lwidth, lheight,   /* dw, dh */
                     False);
  else
    return XvPutImage(dpy, port,
                     win, gc,
                     xv_image,
                     sxoff, syoff,      /* sx, sy */
                     swidth, sheight,   /* sw, sh */
                     lxoff,  lyoff,     /* dx, dy */
                     lwidth, lheight    /* dw, dh */
                     );
};

/* ---------------------------------------------------------------------------
 */
void cXvVideoOut::Pause(void)
{
}

/* ---------------------------------------------------------------------------
 */
void cXvVideoOut::Suspend(void)
{
  if (!xv_initialized)
    return;

  pthread_mutex_lock(&xv_mutex);

  XvStopVideo(dpy, port, win);
  XvUngrabPort(dpy, port, CurrentTime);


  if (xv_image->data) {
    if(useShm && shminfo.shmaddr)
    {
      shmdt(shminfo.shmaddr);
      shminfo.shmaddr = NULL;
      XShmDetach(dpy, &shminfo);
    }
    if (!useShm) {
      free(xv_image->data);
    };
    xv_image->data = NULL;
  };
  pthread_mutex_unlock(&xv_mutex);

  if (xv_image)
  {
    free(xv_image);
    xv_image = NULL;
  }

  xv_initialized = 0;
}

/* ---------------------------------------------------------------------------
 */
bool cXvVideoOut::Resume(void)
{
  return Reconfigure(format);
}


/* ---------------------------------------------------------------------------
 */
void cXvVideoOut::CloseOSD()
{
  cVideoOut::CloseOSD();
  osdMutex.Lock();
#if VDRVERSNUM < 10307
  for (int i = 0; i < MAXNUMWINDOWS; i++)
  {
    if (layer[i])
    {
      delete(layer[i]);
      layer[i]=0;
    }
  }
#endif
  if (videoInitialized)
  {
    memset (osd_buffer, 0, osd_image->bytes_per_line * osd_max_height);
    pthread_mutex_lock(&xv_mutex);
    XClearArea (dpy, win, 0, 0, 0, 0, False);
    ShowOSD();
    XSync(dpy, False);
    pthread_mutex_unlock(&xv_mutex);
  }
  osdMutex.Unlock();
}

#if VDRVERSNUM >= 10307
/* ---------------------------------------------------------------------------
 */
void cXvVideoOut::ClearOSD()
{
  cVideoOut::ClearOSD();
  if (videoInitialized && current_osdMode==OSDMODE_PSEUDO) {
    pthread_mutex_lock(&xv_mutex);
    memset (osd_buffer, 0, osd_image->bytes_per_line * osd_max_height);
    ShowOSD();
    XSync(dpy, False);
    pthread_mutex_unlock(&xv_mutex);
  };
};

/* ---------------------------------------------------------------------------
 */
void cXvVideoOut::AdjustOSDMode()
{
  if (xv_initialized)
          current_osdMode = setupStore->osdMode;
  else current_osdMode = OSDMODE_PSEUDO;
}

/* ---------------------------------------------------------------------------
 */
void cXvVideoOut::GetOSDDimension(int &OsdWidth,int &OsdHeight,
                                  int &xPan, int &yPan) {
   switch (current_osdMode) {
      case OSDMODE_PSEUDO :
                OsdWidth=lwidth<osd_max_width?lwidth:osd_max_width;
                OsdHeight=lheight<osd_max_height?lheight:osd_max_height;
                xPan = yPan = 0;
             break;
      case OSDMODE_SOFTWARE:
                OsdWidth=swidth;//*9/10;
                OsdHeight=sheight;//*9/10;
                xPan = sxoff;
                yPan = syoff;
             break;
    };
};

/* ---------------------------------------------------------------------------
 */
void cXvVideoOut::GetOSDMode(int &Depth, bool &HasAlpha, bool &AlphaInversed,
                  bool &IsYUV, uint8_t *&PixelMask) {
        if (current_osdMode==OSDMODE_SOFTWARE) {
                IsYUV=true;
                PixelMask=NULL;
                return;
        };

        IsYUV=false;
        Depth=osd_image->bits_per_pixel;
        HasAlpha=false;
        PixelMask=NULL;
};

/* ---------------------------------------------------------------------------
 */
void cXvVideoOut::GetLockOsdSurface(uint8_t *&osd, int &stride,
                  bool *&dirtyLines)
{
        osd = NULL;

        if (!videoInitialized)
                return;

        osd=osd_buffer;
        stride=osd_image->bytes_per_line;
        dirtyLines=NULL;
}

/* ---------------------------------------------------------------------------
 */
void cXvVideoOut::CommitUnlockOsdSurface()
{
        if (!videoInitialized)
                return;

        cVideoOut::CommitUnlockOsdSurface();
        pthread_mutex_lock(&xv_mutex);
        ShowOSD();
        XSync(dpy, False);
        pthread_mutex_unlock(&xv_mutex);
}

#else
/* ---------------------------------------------------------------------------
 */
void cXvVideoOut::Refresh()
{
  // refreshes the screen
  // copy video Data
  if (!videoInitialized)
    return;
  //if (OSDpresent)
  {
      int x0 = 0, y0 = 0, x1 = 0, y1 = 0, w = 0, h = 0,
          cx0 = 0, cy0 = 0, cx1 = 0, cy1 = 0, cw = 0, ch = 0;

    if (lastUpdate + 40 > getTimeMilis())
    {
      ;//return; // accept screen update only every 40 ms
    }
    lastUpdate = getTimeMilis();
    for (int i = 0; i < MAXNUMWINDOWS; i++)
    {
      if (layer[i] && layer[i]->visible) {
        layer[i]->Draw (osd_buffer, osd_image->bytes_per_line, NULL);
        layer[i]->Region (&cx0, &cy0, &cw, &ch);
        cx1 = cx0 + cw;
        cy1 = cy0 + ch;
        if (!x1 || !y1)
        {
          x0 = cx0; y0 = cy0; x1 = cx1; y1 = cy1;
        }
        else
        {
          if (cx0 < x0)
            x0 = cx0;
          if (cy0 < y0)
            y0 = cy0;
          if (cx1 > x1)
            x1 = cx1;
          if (cy1 > y1)
            y1 = cy1;
        }
      }
    }
    w = x1 - x0;
    h = y1 - y0;

    if (w && h)
    {
      pthread_mutex_lock(&xv_mutex);
      ShowOSD();
      XSync(dpy, False);
      pthread_mutex_unlock(&xv_mutex);

    }
  }
}
#endif

/* ---------------------------------------------------------------------------
 */
void cXvVideoOut::ShowOSD ()
{
  if (current_osdMode==OSDMODE_PSEUDO ) {
#if VDRVERSNUM >= 10307
    int x= lwidth > osd_max_width ?(lwidth - osd_max_width)/2+lxoff:lxoff;
    int y= lheight > osd_max_height ? (lheight - osd_max_height) / 2+lyoff:lyoff;
#else
    int x=lwidth > OSD_FULL_WIDTH ?(lwidth - OSD_FULL_WIDTH)/2+lxoff:lxoff;
    int y=lheight > OSD_FULL_HEIGHT?(lheight - OSD_FULL_HEIGHT) / 2+lyoff:lyoff;
#endif
    if (useShm)
      XShmPutImage (dpy, win, gc, osd_image,
          1,1,
          x,y,
          lwidth-1,lheight-1,
          False);
    else
      XPutImage (dpy, win, gc, osd_image,
          1,
          1,
          x,y,
          lwidth-1,lheight-1);
  }
}

/* ---------------------------------------------------------------------------
 */
void cXvVideoOut::YUV(sPicBuffer *buf)
{
  if (!videoInitialized || !xv_initialized)
    return;

  pthread_mutex_lock(&xv_mutex);
  if (aspect_changed ||
      cutTop != setupStore->cropTopLines ||
      cutBottom != setupStore->cropBottomLines ||
      cutLeft != setupStore->cropLeftCols ||
      cutRight != setupStore->cropRightCols)
  {
    XClearArea (dpy, win, 0, 0, 0, 0, False);
    ShowOSD();
    aspect_changed = 0;
    cutTop = setupStore->cropTopLines;
    cutBottom = setupStore->cropBottomLines;
    cutLeft = setupStore->cropLeftCols;
    cutRight = setupStore->cropRightCols;
    ClearXvArea (0, 128, 128);
  }
  uint8_t *Py=buf->pixel[0]
                +(buf->edge_height)*buf->stride[0]
                +buf->edge_width;
  uint8_t *Pu=buf->pixel[1]+(buf->edge_height/2)*buf->stride[1]
                +buf->edge_width/2;
  uint8_t *Pv=buf->pixel[2]+(buf->edge_height/2)*buf->stride[2]
                +buf->edge_width/2;
  int Ystride=buf->stride[0];
  int UVstride=buf->stride[1];
  int Width=buf->width;
  int Height=buf->height;

  if ( Py && Pu && Pv ) {
#if VDRVERSNUM >= 10307
  /* -------------------------------------------------------------------------
   * don't know where those funny stride values (752,376) come from.
   * therefor  we have to copy line by line :-( .
   * Hmm .. for HDTV they should be larger anyway and for some other
   * unusual resolutions they should be configurable swidth/sheight ?
   */

  // if (0) {
  if (OSDpresent && current_osdMode==OSDMODE_SOFTWARE) {
        for (int i = cutTop * 2; i < fheight - cutBottom * 2; i++)
        {
          AlphaBlend(pixels[0]+i*xvWidth+2*cutLeft,
                     OsdPy+i*OSD_FULL_WIDTH+2*cutLeft,
                     Py + i * Ystride+2*cutLeft,
                     OsdPAlphaY+i*OSD_FULL_WIDTH+2*cutLeft,
                     fwidth-2*(cutLeft+cutRight));
        }

        for (int i = cutTop; i < fheight / 2 - cutBottom; i++)
        {
          AlphaBlend(pixels[1]+i*xvWidth/2+cutLeft,
                     OsdPv+i*OSD_FULL_WIDTH/2+cutLeft,
                     Pv+ i * UVstride+cutLeft,
                     OsdPAlphaUV+i*OSD_FULL_WIDTH/2+cutLeft,
                     fwidth/2-(cutLeft+cutRight));
        }

        for (int i = cutTop; i < fheight / 2 - cutBottom; i++)
        {
          AlphaBlend(pixels[2]+i*xvWidth/2+cutLeft,
                     OsdPu+i*OSD_FULL_WIDTH/2+cutLeft,
                     Pu+i*UVstride+cutLeft,
                     OsdPAlphaUV+i*OSD_FULL_WIDTH/2+cutLeft,
                     fwidth/2-(cutLeft+cutRight));
        }

  }
    else
#endif
    {

      switch (format) {
        case FOURCC_YV12:
        case FOURCC_I420:
          for (int i = cutTop * 2; i < fheight - cutBottom * 2; i++)
             fast_memcpy(pixels [0] + i * xvWidth + 2 * cutLeft,
                         Py + i * Ystride + 2 * cutLeft,
                         fwidth - 2 * (cutLeft + cutRight));
          for (int i = cutTop; i < fheight / 2 - cutBottom; i++)
             fast_memcpy (pixels [1] + i * xvWidth / 2 + cutLeft,
                          Pv + i * UVstride + cutLeft,
                          fwidth / 2 - (cutLeft + cutRight));
          for (int i = cutTop; i < fheight / 2 - cutBottom; i++)
             fast_memcpy (pixels [2] + i * xvWidth / 2 + cutLeft,
                          Pu + i * UVstride + cutLeft,
                          fwidth / 2 - (cutLeft + cutRight));
          break;
        case FOURCC_YUY2:
      if (interlaceMode) {
          yv12_to_yuy2_il_mmx2(Py + Ystride  * cutTop * 2 + cutLeft * 2,
                               Pu + UVstride * cutTop + cutLeft,
                               Pv + UVstride * cutTop + cutLeft,
                               pixels[0] + 2*xvWidth * cutTop * 2 + cutLeft * 4,
                               Width - 2 * (cutLeft + cutRight),
                               Height - 2 * (cutTop + cutBottom),
                               Ystride, UVstride, 2*xvWidth);
      } else {
          yv12_to_yuy2_fr_mmx2(Py + Ystride  * cutTop * 2 + cutLeft * 2,
                               Pu + UVstride * cutTop + cutLeft,
                               Pv + UVstride * cutTop + cutLeft,
                               pixels[0] + 2*xvWidth * cutTop * 2 + cutLeft * 4,
                               Width - 2 * (cutLeft + cutRight),
                               Height - 2 * (cutTop + cutBottom),
                               Ystride, UVstride, 2*xvWidth);
      }
          break;
      }
    }
  }
  PutXvImage();
  XSync(dpy, False);
  pthread_mutex_unlock(&xv_mutex);
}

cXvVideoOut::~cXvVideoOut()
{
  if (!videoInitialized)
    return;

  if (xv_initialized)
  {
    pthread_mutex_lock(&xv_mutex);

    XvStopVideo(dpy, port, win);

    pthread_mutex_unlock(&xv_mutex);

    if(shminfo.shmaddr)
    {
      shmdt(shminfo.shmaddr);
      shminfo.shmaddr = NULL;
      shminfo.shmid = -1;
    }

    if (xv_image)
    {
      free(xv_image);
      xv_image = NULL;
    }
  }


  if(osd_shminfo.shmaddr)
  {
    shmdt(osd_shminfo.shmaddr);
    osd_shminfo.shmaddr = NULL;
  }


#if XINERAMA_SUPPORT
  XFree(xin_screen_info);
#endif
  if (osd_image)
  {
    free(osd_image);
    osd_image = NULL;
  }
  videoInitialized = false;
  pthread_mutex_unlock(&xv_mutex);

#if VDRVERSNUM < 10307
  for (int i = 0; i < MAXNUMWINDOWS; i++)
  {
    if (layer[i])
    {
      delete(layer[i]);
    }
  }
#endif
}

#ifdef USE_SUBPLUGINS
/* ---------------------------------------------------------------------------
 */
extern "C" void *
SubPluginCreator(cSetupStore *s)
{
  return new cXvVideoOut(s);
}
#endif
