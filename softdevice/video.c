/*
 * video.c: A plugin for the Video Disk Recorder
 *
 * See the README file for copyright information and how to reach the author.
 *
 * $Id: video.c,v 1.70 2006/11/16 21:03:18 wachm Exp $
 */

#include <fcntl.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <math.h>

//#include <vdr/plugin.h>
#include "video.h"
#include "utils.h"
#include "setup-softdevice.h"
#include "sync-timer.h"

//#define OSDDEB(out...) {printf("vout_osd[%04d]:",(int)(getTimeMilis() % 10000));printf(out);}

#ifndef OSDDEB
#define OSDDEB(out...)
#endif

cVideoOut::cVideoOut(cSetupStore *setupStore)
{
  OsdWidth=OSD_FULL_WIDTH;
  OsdHeight=OSD_FULL_HEIGHT;
  // set some reasonable defaults
  fwidth = lwidth = old_dwidth = dwidth = swidth = 720;
  fheight = lheight = old_dheight = dheight = sheight = 536;
  sxoff = syoff = lxoff = lyoff = 0;
  cutTop = cutBottom = cutLeft = cutRight = 0;
  OsdPy = OsdPu = OsdPv = OsdPAlphaY = OsdPAlphaUV = NULL;
  Osd_changed = 0;
  aspect_F = 4.1 / 3.0;
  aspect_I = 0;
  current_aspect = -1;
  interlaceMode = -1;
  prevZoomFactor = 0;
  realZoomFactor = 1.0;
  PixelMask=NULL;
  OsdRefreshCounter=0;
  displayTimeUS = 0;
  this->setupStore=setupStore;
  freezeMode=false;
  videoInitialized = false;
  oldPicture = NULL;

  for (int i = 0; i < SETUP_VIDEOASPECTNAMES_COUNT; ++i)
    parValues [i] = 1.0;

  init_OsdBuffers();

  //start osd thread
  active=true;
  //Start();
}

cVideoOut::~cVideoOut()
{
  active=false;
  Cancel(3);
  dsyslog("[VideoOut]: Good bye");
}

/*----------------------------------------------------------------------------*/
void cVideoOut::init_OsdBuffers()
{
    int Ysize=(OSD_FULL_WIDTH*OSD_FULL_HEIGHT);
    if (!OsdPy)
       OsdPy=(uint8_t*)malloc(Ysize+8);
    if (!OsdPAlphaY)
    {
       OsdPAlphaY=(uint8_t*)malloc(Ysize+8);
       memset(OsdPAlphaY,0,Ysize);
    };
    if (!OsdPu)
       OsdPu=(uint8_t*)malloc(Ysize/4+8);
    if (!OsdPv)
       OsdPv=(uint8_t*)malloc(Ysize/4+8);
    if (!OsdPAlphaUV)
    {
       OsdPAlphaUV=(uint8_t*)malloc(Ysize/4+8);
       memset(OsdPAlphaUV,0,Ysize/4);
    }
}

/*----------------------------------------------------------------------------*/
void cVideoOut::Action()
{
  ClearOSD();
  while(active)
  {
    OsdRefreshCounter++;
    usleep(20000);

    if (OsdRefreshCounter > 2)
      ProcessEvents();

    if (
        OsdRefreshCounter > 120 || // blanks the screen after inactivity (4s)
        (IsSoftOSDMode() &&
         OsdRefreshCounter>5 && Osd_changed))
    {
      oldPictureMutex.Lock();
      if (oldPicture)
      {
        OSDDEB("redrawing oldPicture osd_changed %d\n",Osd_changed);
        DrawStill_420pl(oldPicture);
      }
      else
      {
        sPicBuffer tmpBuf;

        memset (&tmpBuf, 0, sizeof (sPicBuffer));
        tmpBuf.pixel[0]=OsdPy;
        tmpBuf.pixel[1]=OsdPu;
        tmpBuf.pixel[2]=OsdPv;
        tmpBuf.stride[0]=OSD_FULL_WIDTH;
        tmpBuf.stride[1]=tmpBuf.stride[2]=OSD_FULL_WIDTH/2;
        tmpBuf.max_width=tmpBuf.width=OSD_FULL_WIDTH;
        tmpBuf.max_height=tmpBuf.height=OSD_FULL_HEIGHT;
        tmpBuf.aspect_ratio=((float)OSD_FULL_HEIGHT)/((float)OSD_FULL_WIDTH);
        tmpBuf.aspect_ratio=((float)OSD_FULL_WIDTH)/((float)OSD_FULL_HEIGHT);
        tmpBuf.dtg_active_format=0;
        OSDDEB("drawing osd_layer osd_changed %d\n",Osd_changed);
        DrawStill_420pl(&tmpBuf);
      }
      oldPictureMutex.Unlock();
    }
  }
}

/* ---------------------------------------------------------------------------
 */
void cVideoOut::SetOldPicture(sPicBuffer *picture)
{
  if (oldPicture)
    UnlockBuffer(oldPicture);
  if (picture && picture->owner==this) {
    LockBuffer(picture);
    oldPicture=picture;
  } else
    oldPicture = NULL;
}

/* ---------------------------------------------------------------------------
 */
void cVideoOut::SetParValues(double displayAspect, double displayRatio)
{
  parValues [0] = displayAspect / displayRatio;
  parValues [1] = ( 5.0 /  4.0) / displayRatio;
  parValues [2] = ( 4.0 /  3.0) / displayRatio;
  parValues [3] = (16.0 /  9.0) / displayRatio;
  parValues [4] = (16.0 / 10.0) / displayRatio;
  parValues [5] = (15.0 /  9.0) / displayRatio;
}

/* ---------------------------------------------------------------------------
 */
void cVideoOut::AdjustToZoomFactor(int *tw, int *th)
{
  if (prevZoomFactor != setupStore->zoomFactor ||
      zoomCenterX != setupStore->zoomCenterX ||
      zoomCenterY != setupStore->zoomCenterY ||
      expandTopBottom != setupStore->expandTopBottomLines ||
      expandLeftRight != setupStore->expandLeftRightCols)
  {
    prevZoomFactor = setupStore->zoomFactor;
    zoomCenterX = setupStore->zoomCenterX;
    zoomCenterY = setupStore->zoomCenterY;
    realZoomFactor = (1.0 +
                      (((double) prevZoomFactor * (double) prevZoomFactor) / 2)
                        / 512.0);
    expandTopBottom = setupStore->expandTopBottomLines;
    expandLeftRight = setupStore->expandLeftRightCols;
    /* -----------------------------------------------------------------------
     * force recalculation for aspect ration handling
     */
    current_aspect = -1;
  }

  *th = (int) ((double) fheight / realZoomFactor);
  *tw = (int) ((double) fwidth / realZoomFactor);
}

/* ---------------------------------------------------------------------------
 */
void cVideoOut::AdjustSourceArea(int tw, int th)
{
    int dx, dy;

  sxoff = (fwidth - swidth) / 2;
  syoff = (fheight - sheight) / 2;

  sxoff += (int) ((double) sxoff * ((double) zoomCenterX / 100.0));
  syoff += (int) ((double) syoff * ((double) zoomCenterY / 100.0));

  /* -------------------------------------------------------------------------
   * handle user selected row/coloumn expansion
   */
  if ((dy = expandTopBottom * 2 - syoff) > 0) {
    syoff += dy;
    sheight -= 2 * dy;
  }
  if ((dx = expandLeftRight * 2 - sxoff) > 0) {
    sxoff += dx;
    swidth -= 2 * dx;
  }

  /* --------------------------------------------------------------------------
   * adjust possible rounding errors. as we are in YV12 or similar mode,
   * line offsets and coloumn offsets must be even.
   */
  if (syoff & 1) {
    syoff--;
    if ((th - sheight) > 1)
      sheight += 2;
  }

  if (sxoff & 1) {
    sxoff--;
    if ((tw - swidth) > 1)
      swidth += 2;
  }
}

/* ---------------------------------------------------------------------------
 */
void cVideoOut::AdjustToDisplayGeometry(double afd_aspect)
{
    double        d_asp, p_asp;

  /* --------------------------------------------------------------------------
   * handle screen aspect support now
   */
  p_asp = parValues [screenPixelAspect];
  d_asp = (double) dwidth / (double) dheight;

  if ((d_asp * p_asp) > afd_aspect) {
    /* ------------------------------------------------------------------------
     * display aspect is wider than frame aspect
     * so we have to pillar-box
     */
    lheight = dheight;
    lwidth = (int) (0.5 + ((double) dheight * afd_aspect / p_asp));
  } else {
    /* ------------------------------------------------------------------------
     * display aspect is taller or equal than frame aspect
     * so we have to letter-box
     */
    lwidth = dwidth;
    lheight = (int) (0.5 + ((double) dwidth * p_asp / afd_aspect));
  }

  /* -------------------------------------------------------------------------
   * center result on display, and force offsets to be even.
   */
  lxoff = ((dwidth - lwidth) / 2) & ~1;
  lyoff = ((dheight - lheight) / 2) & ~1;
}

/* ---------------------------------------------------------------------------
 */
void cVideoOut::CheckAspect(int new_afd, double new_asp)
{
    int           new_aspect,
                  tmpWidth,
                  tmpHeight;
    double        afd_asp;

  /* -------------------------------------------------------------------------
   * check if there are some aspect ratio constraints
   * flags & XV_FORMAT_.. check if our ouput aspect is
   *                        NORMAL 4:3 or WIDE 16:9
   *
   */
  new_aspect = (new_asp > 1.4) ? DV_FORMAT_WIDE : DV_FORMAT_NORMAL;
  new_afd &= 0x07;
  /* -------------------------------------------------------------------------
   * override afd value with crop value from setup
   */
  new_afd = (setupStore->cropMode) ? setupStore->cropMode : new_afd;

  /* -------------------------------------------------------------------------
   * check for changes of screen width/height change
   */

  if (screenPixelAspect != setupStore->screenPixelAspect)
  {
    screenPixelAspect = setupStore->screenPixelAspect;
    /* -----------------------------------------------------------------------
     * force recalculation for aspect ration handling
     */
    current_aspect = -1;
  }

  AdjustToZoomFactor(&tmpWidth, &tmpHeight);

  if (new_aspect == current_aspect && new_afd == current_afd )
  {
    aspect_changed = 0;
    return;
  }

  aspect_changed = 1;

  switch (new_afd) {
  /* --------------------------------------------------------------------------
   * these are still TODOs. in general I focus on 2 cases where there
   * is a mismatch (4:3 frame encoded as 16:9, and 16:9 enc as 4:3
   */
    case 0: afd_asp = new_asp; break;
    case 1: afd_asp = 4.0 / 3.0; break;
    case 2: afd_asp = 16.0 / 9.0; break;
    case 3: afd_asp = 14.0 / 9.0; break;
    case 4: afd_asp = new_asp; break;
    case 5: afd_asp = 4.0 / 3.0; break;
    case 6: afd_asp = 16.0 / 9.0; break;
    case 7: afd_asp = 16.0 / 9.0; break;
    default: afd_asp = new_asp; break;
  }

  sheight = tmpHeight;
  swidth = tmpWidth;

  if (afd_asp <= new_asp) {
    swidth = (int) (0.5 + ((double) tmpWidth * afd_asp / new_asp));
  } else {
    sheight = (int) (0.5 + ((double) tmpHeight * new_asp / afd_asp));
  }

  AdjustSourceArea (tmpWidth, tmpHeight);
  AdjustToDisplayGeometry (afd_asp);

  dsyslog("[VideoOut]: %dx%d [%d,%d %dx%d] -> %dx%d [%d,%d %dx%d]",
          fwidth, fheight, sxoff, syoff, swidth, sheight,
          dwidth, dheight, lxoff, lyoff, lwidth, lheight);

  current_aspect = new_aspect;
  current_afd = new_afd;
}

/* ---------------------------------------------------------------------------
 */
void cVideoOut::RecalculateAspect(void)
{
  current_aspect = -1;
  //printf("aspect_F %f\n",aspect_F);
  CheckAspect (current_afd, aspect_F);
}

/* ---------------------------------------------------------------------------
 */
void cVideoOut::CheckAspectDimensions(sPicBuffer *pic)
{
  /* --------------------------------------------------------------------------
   * check and handle changes of dimensions first
   */
  if (fwidth != pic->width || fheight != pic->height)
  {
    dsyslog("[VideoOut]: resolution changed: W(%d -> %d); H(%d ->%d)\n",
             fwidth, pic->width, fheight, pic->height);
    fwidth = pic->width; fheight = pic->height;
    current_aspect = -1;
  }
  if (interlaceMode != pic->interlaced_frame)
  {
    //dsyslog("[VideoOut]: interlaced mode now: %sinterlaced",
      //      (picture->interlaced_frame) ? "" : "non-");
    interlaceMode = pic->interlaced_frame;
  }

  if (aspect_I != pic->dtg_active_format ||
      fabs(aspect_F - pic->aspect_ratio ) > 0.0001 )
  {
    dsyslog("[VideoOut]: aspect changed (%d -> %d ; %f -> %f)",
             aspect_I,pic->dtg_active_format,
             aspect_F,pic->aspect_ratio);
#if 0
#if LIBAVCODEC_BUILD > 4686
    if (picture->pan_scan && picture->pan_scan->width) {
      dsyslog("[VideoOut]: PAN/SCAN info present ([%d] %d - %d, %d %d)",
               picture->pan_scan->id,
               picture->pan_scan->width,
               picture->pan_scan->height,
               context->sample_aspect_ratio.num,
               context->sample_aspect_ratio.den);
      for (int i = 0; i < 3; i++) {
        dsyslog("[VideoOut]: PAN/SCAN  position  %d (%d %d)",
                 i,
                 picture->pan_scan->position[i] [0],
                 picture->pan_scan->position[i] [1]);
      }
    }
#endif
#endif

    aspect_I = pic->dtg_active_format;
    aspect_F = pic->aspect_ratio;
  }

  CheckAspect (aspect_I, aspect_F);
}

/* ---------------------------------------------------------------------------
 */
void cVideoOut::CheckArea(int w, int h)
{
  if (fwidth != w || fheight != h)
  {
    dsyslog("[VideoOut]: resolution changed: W(%d -> %d); H(%d ->%d)\n",
             fwidth, w, fheight, h);
    fwidth = w;
    fheight = h;
    aspect_changed = 1;
    current_aspect = -1;
  }
}

/* ---------------------------------------------------------------------------
 */
void cVideoOut::Sync(cSyncTimer *syncTimer, int *delay)
{
  *delay-=syncTimer->GetRelTime();
  syncTimer->Sleep(delay,displayTimeUS);
  *delay -= syncTimer->GetRelTime();
}

/* ---------------------------------------------------------------------------
 */
void cVideoOut::DrawVideo_420pl(cSyncTimer *syncTimer, int *delay,
                                sPicBuffer *pic)
{
  Sync(syncTimer, delay);
  oldPictureMutex.Lock();

  OsdRefreshCounter=0;
  Osd_changed=0;
  CheckAspectDimensions(pic);

  // display picture
  YUV(pic);
  SetOldPicture(pic);

  oldPictureMutex.Unlock();
  ProcessEvents();
}

/* ---------------------------------------------------------------------------
 */
void cVideoOut::DrawStill_420pl(sPicBuffer *buf)
{
  oldPictureMutex.Lock();

  OsdRefreshCounter=0;
  Osd_changed=0;
  CheckArea(buf->width, buf->height);
  CheckAspect(buf->dtg_active_format,buf->aspect_ratio);
  // display picture
  YUV (buf);
  oldPictureMutex.Unlock();

  ProcessEvents();
}

/* ---------------------------------------------------------------------------
 */
void cVideoOut::ClearOSD()
{
  OSDDEB("ClearOSD\n");
  OSDpresent=false; // will automaticly be set to true on redraw ;-)
  if (OsdPy)
    memset(OsdPy,0,OSD_FULL_WIDTH*OSD_FULL_HEIGHT);
  if (OsdPu)
    memset(OsdPu,127,OSD_FULL_WIDTH*OSD_FULL_HEIGHT/4);
  if (OsdPv)
    memset(OsdPv,127,OSD_FULL_WIDTH*OSD_FULL_HEIGHT/4);
  if (OsdPAlphaY)
    memset(OsdPAlphaY,0,OSD_FULL_WIDTH*OSD_FULL_HEIGHT);
  if (OsdPAlphaUV)
    memset(OsdPAlphaUV,0,OSD_FULL_WIDTH*OSD_FULL_HEIGHT/4);
  Osd_changed=1;
}

/* ---------------------------------------------------------------------------
 */
bool cVideoOut::IsSoftOSDMode()
{
  return current_osdMode == OSDMODE_SOFTWARE;
}

void cVideoOut::OpenOSD()
{
  OSDDEB("OpenOSD\n");
}

int cVideoOut::GetOSDColorkey()
{
  OSDDEB("GetOSDColorKey\n");
  return 0x000000;
}

void cVideoOut::CloseOSD()
{
  ClearOSD();
  OSDpresent=false;
  OSDDEB("CloseOSD\n");
}

/* ---------------------------------------------------------------------------
 */
void cVideoOut::AdjustOSDMode()
{
  current_osdMode = OSDMODE_PSEUDO;
}
