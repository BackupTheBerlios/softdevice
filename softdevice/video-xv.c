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
 * $Id: video-xv.c,v 1.17 2005/03/04 20:04:20 lucke Exp $
 */

#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <vdr/plugin.h>
#include "video-xv.h"
#include "xscreensaver.h"
#include "utils.h"
#include "setup-softdevice.h"

#define PATCH_VERSION "010_pre_1"

static pthread_mutex_t  xv_mutex = PTHREAD_MUTEX_INITIALIZER;
static cXvRemote        *xvRemote = NULL;
static cScreensaver     *xScreensaver = NULL;
static int              events_not_done = 0;

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
void cXvPortAttributeStore::SetXInfo(Display *dpy, XvPortID port)
{
  this->dpy = dpy;
  this->port = port;
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
  XSync(dpy,False);
}

/* ---------------------------------------------------------------------------
 */
Bool chk_event (Display *d, XEvent *e, char *a)
{
  if (e->type == KeyPress)
    return True;
  return False;
}

/* ---------------------------------------------------------------------------
 */
cXvRemote::cXvRemote(const char *Name, cXvVideoOut *vout) : cRemote(Name)
{
  video_out = vout;
}

/* ---------------------------------------------------------------------------
 */
cXvRemote::~cXvRemote()
{
  active = false;
  Cancel(2);
}

/* ---------------------------------------------------------------------------
 */
void cXvRemote::PutKey(KeySym key)
{
  Put (key);
}

/* ---------------------------------------------------------------------------
 */
void cXvRemote::Action(void)
{
  dsyslog("Xv remote control thread started (pid=%d)", getpid());
  active = true;
  while (active)
  {
    usleep (25000);
    pthread_mutex_lock(&xv_mutex);
    if (events_not_done > 2) {
      video_out->ProcessEvents ();
      video_out->ShowOSD (0, True);
      events_not_done = 0;
    } else {
      events_not_done++;
    }
    pthread_mutex_unlock(&xv_mutex);
  }
  dsyslog("Xv remote control thread ended (pid=%d)", getpid());
}

/* ---------------------------------------------------------------------------
 */
void cXvRemote::SetX11Info(Display *d, Window w)
{
  dpy = d;
  win = w;
}

/* ---------------------------------------------------------------------------
 */
void cXvRemote::XvRemoteStart(void)
{
  Start();
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

    x = y = 0;
    w = DisplayWidth(dpy,DefaultScreen(dpy));
    h = DisplayHeight(dpy,DefaultScreen(dpy));
  }
  else
  {
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
      int         mwmHints [5];

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
  else if (net_wm_STATE_FULLSCREEN)
    e.xclient.data.l[1] = net_wm_STATE_FULLSCREEN;
  else //if (net_wm_STATE_STAYS_ON_TOP)
    e.xclient.data.l[1] = net_wm_STATE_STAYS_ON_TOP;
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
void cXvVideoOut::ProcessEvents ()
{

    float           old_aspect;
    char            buffer [80];
    int             len,
                    map_count = 0;
    XComposeStatus  compose;
    KeySym          keysym;
    struct timeval  current_time;

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
        map_count++;
        dx = event.xconfigure.x;
        dy = event.xconfigure.y;
        dwidth = event.xconfigure.width;
        dheight = event.xconfigure.height;
        /* --------------------------------------------------------------------
         * set current picture format to unknown, so that .._check_format
         * does some work.
         */
        old_aspect = (current_aspect == DV_FORMAT_WIDE) ?
                      16.0 / 9.0 : 4.0 / 3.0;
        current_aspect = -1;
        CheckAspect (current_afd, old_aspect);
        break;
      case KeyPress:
        if(cursor_visible == False) {
          XUndefineCursor(dpy, win);
          cursor_visible = True;
        }
        gettimeofday(&current_time, NULL);
        button_time = current_time.tv_sec;

        len = XLookupString (&event. xkey, buffer, 80, &keysym, &compose);
        switch (keysym)
        {
          case XK_Shift_L: case XK_Shift_R: case XK_Control_L: case XK_Control_R:
          case XK_Caps_Lock: case XK_Shift_Lock: case XK_Meta_L:
          case XK_Meta_R: case XK_Alt_L: case XK_Alt_R:
            break;
          case 'f':
            toggleFullScreen();
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
#if 1
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
#endif
#ifdef SUSPEND_BY_KEY 
          case 'r':
          case 'R':
            setupStore->shouldSuspend=!setupStore->shouldSuspend;
#endif
          default:
            if (xvRemote) {
              xvRemote->PutKey (keysym);
            }
            break;
        }
        break;

      case MapNotify:
        map_count++;
        if (initialized) {
          XSetInputFocus(dpy,
                   win,
                   RevertToParent,
                   CurrentTime);
          if (map_count > 2 && xv_initialized) {
            XvShmPutImage(dpy, port,
                          win, gc,
                          xv_image,
                          sxoff, syoff,      /* sx, sy */
                          swidth, sheight,   /* sw, sh */
                          lxoff,  lyoff,     /* dx, dy */
                          lwidth, lheight,   /* dw, dh */
                          False);
            XSync(dpy, False);
            map_count = 0;
          }
        }
        break;
      case UnmapNotify:
        if (xv_initialized)
          XvStopVideo (dpy,port,win);
        if(cursor_visible == False) {
          XUndefineCursor(dpy, win);
          cursor_visible = True;
        }
        break;
      default:
        break;
    }
  }

  if (xv_initialized && map_count) {
    XvShmPutImage(dpy, port,
                  win, gc,
                  xv_image,
                  sxoff, syoff,      /* sx, sy */
                  swidth, sheight,   /* sw, sh */
                  lxoff,  lyoff,     /* dx, dy */
                  lwidth, lheight,   /* dw, dh */
                  False);
    XSync(dpy, False);
  }

  if(cursor_visible == True) {
    gettimeofday(&current_time, NULL);
    if(current_time.tv_sec - motion_time >= 2) {
      XDefineCursor(dpy, win, hidden_cursor);
      cursor_visible = False;
    }
  }
  xScreensaver->MaybeSendDeactivate();
}

/* ---------------------------------------------------------------------------
 */
cXvVideoOut::cXvVideoOut(cSetupStore *setupStore)
              : cVideoOut(setupStore)
{
  OSDpresent = false;
  OSDpseudo_alpha = true;
  initialized = 0;
  /* -------------------------------------------------------------------------
   * could be specified by argv ! TODO
   */
  display_aspect = current_aspect = setupStore->xvAspect;
  scale_size = 0;
  screenPixelAspect = -1;
  width = XV_SRC_WIDTH;
  height = XV_SRC_HEIGHT;

  format = FOURCC_YV12;
  use_xv_port = 0;
  len = width * height * 4;
  outbuffer = NULL;
  w_name = "vdr";
  i_name = "vdr";

  Xres=width;
  Yres=height;

  /*
   * default settings: source, destination and logical widht/height
   * are set to our well known dimensions.
   */
  fwidth = lwidth = dwidth = swidth = width;
  fheight = lheight = dheight = sheight = height;

  if (current_aspect == DV_FORMAT_NORMAL) {
    lwidth = dwidth = XV_DEST_WIDTH_4_3;
    lheight = dheight = XV_DEST_HEIGHT;
  } else {
    lwidth = dwidth = XV_DEST_WIDTH_16_9;
  }
  //start osd refresh thread
  active=true;
  Start();
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

  dsyslog("[XvVideoOut]: patch version (%s)", PATCH_VERSION);

  if(!(dpy = XOpenDisplay(NULL))) {
    dsyslog("[XvVideoOut]: Could not connect to X-server");
    return false;
  } /* if */

  rwin = DefaultRootWindow(dpy);
  scn_id = DefaultScreen(dpy);


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


  if (scale_size) {
    lwidth  = (int)(((double)lwidth  * (double)scale_size)/100.0);
    lheight = (int)(((double)lheight * (double)scale_size)/100.0);
    dwidth  = (int)(((double)dwidth  * (double)scale_size)/100.0);
    dheight = (int)(((double)dheight * (double)scale_size)/100.0);
  }
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
  XNextEvent(dpy, &event);

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
  osd_image = XShmCreateImage (dpy,
                               XDefaultVisual (dpy, scn_id),
                               XDefaultDepth (dpy, scn_id),
                               ZPixmap,NULL,
                               &osd_shminfo,
                               width, height);
  if (osd_image) {
    dsyslog("[XvVideoOut]: Initialize XShmCreateImage Successful (%p)", osd_image);
  } else {
    dsyslog("[XvVideoOut]: Initialize ERROR: XShmCreateImage FAILED !");
  }

  Bpp=osd_image->bits_per_pixel;

  osd_shminfo.shmid = shmget (IPC_PRIVATE,
                              osd_image->bytes_per_line*height,
                              IPC_CREAT | 0777);
  if (osd_shminfo.shmid == -1) {
    dsyslog("[XvVideoOut]: Initialize ERROR: shmget FAILED !");
  } else {
    dsyslog("[XvVideoOut]: Initialize shmget Successful (%d bytes)",
            osd_image->bytes_per_line*height);
  }

  /* compile fix for gcc 3.4.3
   */
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

  rc = XClearArea (dpy, win, 0, 0, 0, 0, True);

  rc = XSync(dpy, False);

  if (osd_shminfo. shmid > 0)
    shmctl (osd_shminfo. shmid, IPC_RMID, 0);

  if (!xScreensaver)
  {
    xScreensaver = new cScreensaver(dpy);
  }

  /* -------------------------------------------------------------------------
   * get up our remote running
   */
  if (!xvRemote)
  {
    xvRemote = new cXvRemote ("softdevice-xv", this);
    xvRemote->SetX11Info(dpy,win);
    xvRemote->XvRemoteStart();
  }

  dsyslog("[XvVideoOut]: initialized OK");

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
          dsyslog("[XvVideoOut]: grabbed port %ld", port);
          got_port = True;
          break;
        } /* if */
      } /* for */
      if(got_port)
        break;
    } /* for */

  } else {
    /* Xv extension probably not present */
    return false;
  } /* else */

  if(!ad_cnt) {
    dsyslog("[XvVideoOut]: (ERROR) no adaptor found!");
    return false;
  }
  if(!got_port) {
    dsyslog("[XvVideoOut]: (ERROR) could not grab any port!");
    return false;
  }

  attributeStore.SetXInfo(dpy,port);
  attributeStore.Save();
  attributeStore.SetColorkey(0x00000000);
  attributeStore.SetValue("XV_AUTOPAINT_COLORKEY",1);

  /*
   * Now we do shared memory allocation etc..
   */
  xv_image = XvShmCreateImage(dpy, port,
                              format, (char *) outbuffer,
                              width, height,
                              &shminfo);

  if (xv_image) {
    dsyslog("[XvVideoOut]: XvShmCreateImage Successful (%p)", xv_image);
  } else {
    dsyslog("[XvVideoOut]: ERROR: XvShmCreateImage FAILED !");
  }

  shminfo.shmid = shmget(IPC_PRIVATE,
                         len,
                         IPC_CREAT | 0777);

  if (shminfo.shmid == -1) {
    dsyslog("[XvVideoOut]: ERROR: shmget FAILED !");
  } else {
    dsyslog("[XvVideoOut]: shmget Successful (%d bytes)",len);
  }

  /* compile fix for gcc 3.4.3
   */
  shminfo.shmaddr = (char *) shmat(shminfo.shmid, NULL, 0);
  outbuffer = (unsigned char *) shminfo.shmaddr;
  xv_image->data = (char *) outbuffer;

  if (xv_image->data == (char *) -1L) {
    dsyslog("[XvVideoOut]: ERROR: shmat FAILED !");
  } else {
    dsyslog("[XvVideoOut]: shmat Successful");
  }
  shminfo.readOnly = False;

  pixels [0] = outbuffer;
  pixels [1] = outbuffer + width * height;
  pixels [2] = pixels [1] + width * height / 4;
  rc = XShmAttach(dpy, &shminfo);
  dsyslog("[XvVideoOut]: XShmAttach    rc = %d %s",
          rc,(rc == 1) ? "(should be OK)":"(thats NOT OK!)");

  if (shminfo. shmid > 0)
    shmctl (shminfo. shmid, IPC_RMID, 0);

  memset (pixels [0], 0, width*height);
  memset (pixels [1], 128, width*height/4);
  memset (pixels [2], 128, width*height/4);
//    dv_display_event(dv_dpy);
  rc = XvShmPutImage(dpy, port,
                     win, gc,
                     xv_image,
                     0, 0,              /* sx, sy */
                     swidth, sheight,   /* sw, sh */
                     lxoff,  lyoff,     /* dx, dy */
                     lwidth, lheight,   /* dw, dh */
                     False);
  rc = XClearArea (dpy, win, 0, 0, 0, 0, True);
  
  rc = XSync(dpy, False);
  
  pthread_mutex_unlock(&xv_mutex);

  this->format = format;
  initialized = 1;
  xv_initialized = 1;
  return true;
}

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
  
  pthread_mutex_unlock(&xv_mutex);

  if(shminfo.shmaddr)
  {
    shmdt(shminfo.shmaddr);
    shminfo.shmaddr = NULL;
  }

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
bool cXvVideoOut::GetInfo(int *fmt, unsigned char **dest, int *w, int *h)
{
  *fmt = format;
  *dest = (unsigned char *) xv_image->data;
  *w = width;
  *h = height;
  return true;
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
  if (initialized)
  {
    memset (osd_buffer, 0, osd_image->bytes_per_line * height);
    pthread_mutex_lock(&xv_mutex);
    osd_refresh_counter = osd_skip_counter = 0;
    XClearArea (dpy, win, 0, 0, 0, 0, True);
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
  if (initialized && current_osdMode==OSDMODE_PSEUDO)
    memset (osd_buffer, 0, osd_image->bytes_per_line * height);
};

/* ---------------------------------------------------------------------------
 */

void cXvVideoOut::GetOSDDimension(int &OsdWidth,int &OsdHeight) {
   switch (current_osdMode) {
      case OSDMODE_PSEUDO :
                OsdWidth=lwidth;//*9/10;
                OsdHeight=lheight;//*9/10;
             break;
      case OSDMODE_SOFTWARE:
                OsdWidth=swidth;//*9/10;
                OsdHeight=sheight;//*9/10;
             break;
    };
};

/* ---------------------------------------------------------------------------
 */
void cXvVideoOut::Refresh(cBitmap *Bitmap)
{
  // refreshes the screen
  // copy video Data
  if (!initialized)
    return;
//  if (OSDpresent)
  {
    switch (current_osdMode) {
      case OSDMODE_PSEUDO :
              Draw(Bitmap, osd_buffer, osd_image->bytes_per_line);
            break;
      case OSDMODE_SOFTWARE:
              ToYUV(Bitmap);
            break;
    };
    
    pthread_mutex_lock(&xv_mutex);
    ++osd_refresh_counter;
    osd_x = OSDxOfs;
    osd_y = OSDyOfs;
    osd_w = OSDw;
    osd_h = OSDh;
    //OSDpresent=true;
    pthread_mutex_unlock(&xv_mutex);
  }
}

#else
/* ---------------------------------------------------------------------------
 */
void cXvVideoOut::Refresh()
{
  // refreshes the screen
  // copy video Data
  if (!initialized)
    return;
  if (OSDpresent)
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
      ++osd_refresh_counter;
      osd_x = x0;
      osd_y = y0;
      osd_w = w;
      osd_h = h;
      pthread_mutex_unlock(&xv_mutex);

    }
  }
}
#endif

/* ---------------------------------------------------------------------------
 */
void cXvVideoOut::ShowOSD (int skip, int do_sync)
{
  if (OSDpresent) {
    if (osd_refresh_counter) {
      if (current_osdMode==OSDMODE_PSEUDO && osd_skip_counter > skip) {
      
        int x= lwidth > OSD_FULL_WIDTH ? osd_x + (dwidth - width) / 2:
                osd_x * lwidth/OSD_FULL_WIDTH *9/10;
        int y= lheight > OSD_FULL_HEIGHT ? osd_y + (dheight - height) / 2:
                osd_y * lheight/OSD_FULL_HEIGHT *9/10;

        XShmPutImage (dpy, win, gc, osd_image,
                      osd_x, 
                      osd_y,
                      x,y,
                      osd_w, osd_h,
                      False);

        if (do_sync)
          XSync(dpy, False);
        osd_skip_counter = 0;
        //osd_refresh_counter--;
      } else {
        osd_skip_counter++;
      }
    }
  }
}

/* ---------------------------------------------------------------------------
 */
void cXvVideoOut::YUV(uint8_t *Py, uint8_t *Pu, uint8_t *Pv,
                      int Width, int Height,
                      int Ystride, int UVstride)
{
  if (!initialized || !xv_initialized)
    return;
#if VDRVERSNUM >= 10307
  OsdRefreshCounter=0;
  
  /* -------------------------------------------------------------------------
   * don't know where those funny stride values (752,376) come from.
   * therefor  we have to copy line by line :-( .
   * Hmm .. for HDTV they should be larger anyway and for some other
   * unusual resolutions they should be configurable swidth/sheight ?
   */

  // if (0) {
  if (OSDpresent && current_osdMode==OSDMODE_SOFTWARE) {
        for (int i = 0; i < fheight; i++)
        {
          AlphaBlend(pixels[0]+i*width,OsdPy+i*OSD_FULL_WIDTH,
            Py + i * Ystride,
            OsdPAlphaY+i*OSD_FULL_WIDTH,fwidth);
        }
 
        for (int i = 0; i < fheight / 2; i++)
        {
          AlphaBlend(pixels[1]+i*width/2,
            OsdPv+i*OSD_FULL_WIDTH/2,Pv+ i * UVstride,
            OsdPAlphaUV+i*OSD_FULL_WIDTH/2,fwidth/2);
        }

        for (int i = 0; i < fheight / 2; i++)
        {
          AlphaBlend(pixels[2]+i*width/2,
            OsdPu+i*OSD_FULL_WIDTH/2,Pu+i*UVstride,
            OsdPAlphaUV+i*OSD_FULL_WIDTH/2,fwidth/2);
        }
#ifdef USE_MMX
     EMMS;
#endif

        pthread_mutex_lock(&xv_mutex);
        XvShmPutImage(dpy, port,
                win, gc,
                xv_image,
                sxoff, syoff,      /* sx, sy */
                swidth, sheight,   /* sw, sh */
                lxoff,  lyoff,     /* dx, dy */
                lwidth, lheight,   /* dw, dh */
                False);

  }
  else 
#endif
 {
          for (int i = 0; i < fheight; i++)
             memcpy (pixels [0] + i * width, Py + i * Ystride, fwidth);
          for (int i = 0; i < fheight / 2; i++) 
             memcpy (pixels [1] + i * width / 2, Pv + i * UVstride, fwidth / 2);
          for (int i = 0; i < fheight / 2; i++)
             memcpy (pixels [2] + i * width / 2, Pu + i * UVstride, fwidth / 2);
   
          pthread_mutex_lock(&xv_mutex);
          XvShmPutImage(dpy, port,
                win, gc,
                xv_image,
                sxoff, syoff,      /* sx, sy */
                swidth, sheight,   /* sw, sh */
                lxoff,  lyoff,     /* dx, dy */
                lwidth, lheight,   /* dw, dh */
                False);
          ShowOSD (1, False);
  }
  ProcessEvents ();
  events_not_done = 0;
  XSync(dpy, False);
  pthread_mutex_unlock(&xv_mutex);
}

cXvVideoOut::~cXvVideoOut()
{
  if (!initialized)
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


  if (osd_image)
  {
    free(osd_image);
    osd_image = NULL;
  }
  initialized = 0;
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
