/*
 * video.c: A plugin for the Video Disk Recorder
 *
 * See the README file for copyright information and how to reach the author.
 *
 * $Id: video.c,v 1.49 2006/04/21 06:47:10 lucke Exp $
 */

#include <sys/mman.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <vdr/plugin.h>
#include "video.h"
#include "utils.h"
#include "setup-softdevice.h"
#include "sync-timer.h"
#include "SoftOsd.h"

//#define OSDDEB(out...) {printf("vout_osd[%04d]:",(int)(getTimeMilis() % 10000));printf(out);}

#ifndef OSDDEB
#define OSDDEB(out...)
#endif

cVideoOut::cVideoOut(cSetupStore *setupStore)
{
#if VDRVERSNUM >= 10307
  OsdWidth=OSD_FULL_WIDTH;
  OsdHeight=OSD_FULL_HEIGHT;
#endif
  // set some reasonable defaults
  old_width = fwidth = lwidth = old_dwidth = dwidth = swidth = 720;
  old_height = fheight = lheight = old_dheight = dheight = sheight = 536;
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
  old_picture = NULL;

  for (int i = 0; i < MAX_PAR; ++i)
    parValues [i] = 1.0;

  init_OsdBuffers();

  //start osd thread
  active=true;
  Start();
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
#if VDRVERSNUM >= 10307
  while(active)
  {
    OsdRefreshCounter++;
    usleep(50000);
    if (
        OsdRefreshCounter > 80 || // blanks the screen after inactivity (4s)
        (setupStore->osdMode == OSDMODE_SOFTWARE &&
         OsdRefreshCounter>2 && Osd_changed))
    {
      osdMutex.Lock();
      if (old_picture)
      {
        OSDDEB("redrawing old_picture\n");
        DrawStill_420pl (old_picture->data[0],
                         old_picture->data[1],
                         old_picture->data[2],
                         old_width, old_height,
                         old_picture->linesize[0],
                         old_picture->linesize[1]);
      }
      else
      {
        OSDDEB("drawing osd_layer\n");
        DrawStill_420pl (OsdPy,OsdPu, OsdPv,
                        OsdWidth,OsdHeight,
                        //OSD_FULL_WIDTH, OSD_FULL_HEIGHT,
                         OSD_FULL_WIDTH, OSD_FULL_WIDTH/2);
      }
      osdMutex.Unlock();
    }
  }
#endif
}

/* ---------------------------------------------------------------------------
 */
void cVideoOut::InvalidateOldPicture(void)
{
  areaMutex.Lock();
  old_picture = NULL;
  areaMutex.Unlock();
}

/* ---------------------------------------------------------------------------
 */
void cVideoOut::SetOldPicture(AVFrame *picture, int width, int height)
{
  //osdMutex.Lock(); //protected by areaMutex osdMutex will cause deadlocks!
  old_picture = picture;
  old_width = width;
  old_height = height;
  //osdMutex.Unlock();
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
   * center result on display
   */
  lxoff = (dwidth - lwidth) / 2;
  lyoff = (dheight - lheight) / 2;
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
void cVideoOut::CheckAspectDimensions(AVFrame *picture,
                                        AVCodecContext *context)
{
    volatile double new_asp;

  /* --------------------------------------------------------------------------
   * check and handle changes of dimensions first
   */
  if (fwidth != context->width || fheight != context->height)
  {
    dsyslog("[VideoOut]: resolution changed: W(%d -> %d); H(%d ->%d)\n",
             fwidth, context->width, fheight, context->height);
    fwidth = context->width; fheight = context->height;
    current_aspect = -1;
  }

  if (interlaceMode != picture->interlaced_frame)
  {
    dsyslog("[VideoOut]: interlaced mode now: %sinterlaced",
            (picture->interlaced_frame) ? "" : "non-");
    interlaceMode = picture->interlaced_frame;
  }
#if LIBAVCODEC_BUILD > 4686
  /* --------------------------------------------------------------------------
   * removed aspect ratio calculation based on picture->pan_scan->width
   * as this value seems to be wrong on some dvds.
   * reverted this removal as effect from above it is not reproducable.
   */
  if (!context->sample_aspect_ratio.num)
  {
    new_asp = (double) (context->width) / (double) (context->height);
  }
  else if (picture->pan_scan && picture->pan_scan->width)
  {
    new_asp = (double) (picture->pan_scan->width *
                         context->sample_aspect_ratio.num) /
                (double) (picture->pan_scan->height *
                           context->sample_aspect_ratio.den);
  }
  else
  {
    new_asp = (double) (context->width * context->sample_aspect_ratio.num) /
               (double) (context->height * context->sample_aspect_ratio.den);
  }
#else
  new_asp = context->aspect_ratio;
#endif

  /* --------------------------------------------------------------------------
   * aspect_F and new_asp are now static volatile float. Due to above
   * code removal, gcc-3.3.1 from suse compiles comparison wrong.
   * it compares the 32bit float value with it's temprary new_asp value
   * from above calculation which has even a higher precision than double :-( ,
   * and would result not_equal every time.
   */
  if (aspect_I != context->dtg_active_format ||
      aspect_F != new_asp)
  {
    dsyslog("[VideoOut]: aspect changed (%d -> %d ; %f -> %f)",
             aspect_I,context->dtg_active_format,
             aspect_F,new_asp);
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

    aspect_I = context->dtg_active_format;
    aspect_F = new_asp;
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
                                AVFrame *picture, AVCodecContext *context)
{
  areaMutex. Lock();
  OsdRefreshCounter=0;
  Osd_changed=0;
  CheckAspectDimensions(picture,context);
  SetOldPicture(picture,context->width,context->height);
  Sync(syncTimer, delay);
  // display picture
  YUV(picture->data[0], picture->data[1],picture->data[2],
      context->width,context->height,
      picture->linesize[0],picture->linesize[1]);

  /* --------------------------------------------------------------------------
   * Unlocking could be done a bit earlier in YUV(), after video is displayed
   * and before event processing starts. For now it is easier to do it here.
   * Same applies for DrawStill_420pl() below.
   */
  areaMutex. Unlock();
}

/* ---------------------------------------------------------------------------
 */
void cVideoOut::DrawStill_420pl(uint8_t *pY, uint8_t *pU, uint8_t *pV,
                                int w, int h, int yPitch, int uvPitch)
{
  areaMutex. Lock();
  OsdRefreshCounter=0;
  Osd_changed=0;
  RecalculateAspect();
  CheckArea(w, h);
  // display picture
  YUV (pY, pU, pV, w, h, yPitch, uvPitch);
  areaMutex. Unlock();
}

/* ---------------------------------------------------------------------------
 */
void cVideoOut::ClearOSD()
{
  OSDDEB("ClearOSD\n");
  OSDpresent=false; // will automaticly be set to true on redraw ;-)
  //if (current_osdMode==OSDMODE_SOFTWARE)
  {
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
   };
  Osd_changed=1;
};

#if VDRVERSNUM >= 10307

void cVideoOut::OpenOSD()
{
  OSDDEB("OpenOSD\n");
}

void cVideoOut::CloseOSD()
{
  osdMutex.Lock();
  ClearOSD();
  OSDpresent=false;
  osdMutex.Unlock();
  OSDDEB("CloseOSD\n");
}

/* ---------------------------------------------------------------------------
 */
void cVideoOut::AdjustOSDMode()
{
  current_osdMode = OSDMODE_PSEUDO;
}

void cVideoOut::AlphaBlend(uint8_t *dest,uint8_t *P1,uint8_t *P2,
          uint8_t *alpha,uint16_t count) {
     // printf("%x %x %x \n",P1,P2,alpha);

#ifdef USE_MMX
        __asm__(" pxor %%mm3,%%mm3\n"
#ifdef USE_MMX2
                PREFETCH"(%0)\n"
                PREFETCH"(%1)\n"
                PREFETCH"(%2)\n"
                PREFETCH"64(%0)\n"
                PREFETCH"64(%1)\n"
                PREFETCH"64(%2)\n"
                PREFETCH"128(%0)\n"
                PREFETCH"128(%1)\n"
                PREFETCH"128(%2)\n"
#endif //USE_MMX2
                : : "r" (P1), "r" (P2), "r" (alpha) : "memory");

        // I guess this can be further improved...
	// Useing prefetch makes it slower on Athlon64,
	// but faster on the Athlon Tbird...
	// Why is prefetching slower on Athlon64????
        while (count>8 ) {
#ifdef USE_MMX2
          if (! (count%8 ) )
               __asm__(
                  PREFETCH" 192(%0)\n"
                  PREFETCH" 192(%1)\n"
                  PREFETCH" 192(%2)\n"
                  : : "r" (P1), "r" (P2), "r" (alpha) : "memory");

#endif //USE_MMX2
         __asm__(
                "  movq  (%0),%%mm0\n"
                "  movq  (%1),%%mm1\n"
                "  movq  (%2),%%mm2\n"
                "  movq  %%mm0,%%mm4\n"
                "  movq  %%mm1,%%mm5\n"
                "  movq  %%mm2,%%mm6\n"
                "  punpcklbw %%mm3, %%mm0\n"
                "  punpcklbw %%mm3, %%mm1\n"
                "  punpcklbw %%mm3, %%mm2\n"
                "  punpckhbw %%mm3, %%mm4\n"
                "  punpckhbw %%mm3, %%mm5\n"
                "  punpckhbw %%mm3, %%mm6\n"
                "  psubw %%mm1, %%mm0 \n"
                "  psubw %%mm5, %%mm4 \n"
                "  psraw $1,%%mm0\n"
                "  psraw $1,%%mm4\n"
                "  pmullw %%mm2, %%mm0 \n"
                "  pmullw %%mm6, %%mm4 \n"
                "  psraw $7,%%mm0\n"
                "  psraw $7,%%mm4\n"
                "  paddw %%mm1, %%mm0 \n"
                "  paddw %%mm5, %%mm4 \n"
                "  packuswb %%mm4, %%mm0 \n"
                MOVQ " %%mm0,(%3)\n"
                : : "r" (P1), "r" (P2), "r" (alpha),"r"(dest) : "memory");
                count-=8;
                P1+=8;
                P2+=8;
                alpha+=8;
                dest+=8;
       }
       EMMS;
#endif //USE_MMX

       //fallback version and the last missing bytes...
       for (int i=0; i < count; i++){
          dest[i]=(((uint16_t) P1[i] *(uint16_t) alpha[i]) +
             ((uint16_t) P2[i] *(256-(uint16_t) alpha[i])))  >>8 ;
       }
}


#else

bool cVideoOut::OpenWindow(cWindow *Window) {
    layer[Window->Handle()]= new cWindowLayer(Window->X0()+OSDxOfs,
                                              Window->Y0()+OSDyOfs,
                                              Window->Width(),
                                              Window->Height(),
                                              Bpp,
                                              Xres, Yres, OSDpseudo_alpha);
    return true;
}

void cVideoOut::OpenOSD(int X, int Y)
{
  // initialize Layers.
  OSDxOfs = (Xres - 720) / 2 + X;
  OSDyOfs = (Yres - 576) / 2 + Y;
  osdMutex.Lock();
  for (int i = 0; i < MAXNUMWINDOWS; i++)
  {
    layer[i]=0;
  }
  OSDpresent=true;
  osdMutex.Unlock();
}

void cVideoOut::CloseOSD()
{
  osdMutex.Lock();
  OSDpresent=false;
  for (int i = 0; i < MAXNUMWINDOWS; i++)
  {
    if (layer[i])
    {
      delete(layer[i]);
      layer[i]=0;
    }
  }
  osdMutex.Unlock();
}

void cVideoOut::CommitWindow(cWindow *Window) {
    layer[Window->Handle()]->Render(Window);
    Refresh();
}

void cVideoOut::ShowWindow(cWindow *Window) {
    layer[Window->Handle()]->visible=true;
    layer[Window->Handle()]->Render(Window);
    Refresh();
}
void cVideoOut::HideWindow(cWindow *Window, bool Hide) {
    layer[Window->Handle()]->visible= ! Hide ;
    Refresh();
}

void cVideoOut::MoveWindow(cWindow *Window, int x, int y) {
    layer[Window->Handle()]->Move(x,y);
    layer[Window->Handle()]->Render(Window);
    Refresh();
}

void cVideoOut::CloseWindow(cWindow *Window) {
    delete (layer[Window->Handle()]);
    Refresh();
}


// --- cWindowLayer --------------------------------------------------
cWindowLayer::cWindowLayer(int X, int Y, int W, int H, int Bpp,
                           int Xres, int Yres, bool alpha) {
    left=X;
    top=Y;
    width=W;
    height=H;
    bpp=Bpp;
    xres=Xres;
    yres=Yres;
    visible=false;
    OSDpseudo_alpha = alpha;
    imagedata=(unsigned char *)malloc(W*H*4); // RGBA Screen memory
    //printf("[video] Creating WindowLayer at %d x %d, (%d x %d)\n",X,Y,W,H);
}

void cWindowLayer::Region (int *x, int *y, int *w, int *h) {
   *x = left;
   *y = top;
   *w = width;
   *h = height;
}


cWindowLayer::~cWindowLayer() {
    free(imagedata);
}


void cWindowLayer::Render(cWindow *Window) {
    unsigned char * buf;
    buf=imagedata;

  for (int yp = 0; yp < height; yp++) {
    for (int ix = 0; ix < width; ix++) {
      eDvbColor c = Window->GetColor(*Window->Data(ix,yp));
      buf[0]=c & 255; //Red
      buf[1]=(c >> 8) & 255; //Green
      buf[2]=(c >> 16) & 255; //Blue
      buf[3]=(c >> 24) & 255; //Alpha*/
      buf+=4;
    }
  }
}

void cWindowLayer::Move(int x, int y) {
    left=x;
    top=y;
}

void cWindowLayer::Draw(unsigned char * buf, int linelen, unsigned char * keymap) {
    unsigned char * im;
    im = imagedata;
    int depth = (bpp + 7) / 8;
    int dx = linelen - width * depth;
    bool          prev_pix = false, do_dither;

  buf += top * linelen + left * depth; // upper left corner
  for (int y = top; y < top+height; y++) {
    prev_pix = false;

    for (int x = left; x < left+width; x++) {
      if ( (im[3] != 0)
          && (x >= 0) && (x < xres)
          && (y >= 0) && (y < yres))  { // Alpha != 0 and in the screen
        do_dither = ((x % 2 == 1 && y % 2 == 1) ||
                      x % 2 == 0 && y % 2 == 0 || prev_pix);

        //if (keymap) keymap[(x+y*linelen / depth) / 8] |= (1 << (x % 8));
        switch (depth) {
          case 4:
            if ((do_dither && IS_BACKGROUND(im[3]) && OSDpseudo_alpha) ||
                (im[3] == 255 && im[0] == 0 && im[1] == 0 && im[2] == 0)) {
              *buf++ = 1; *buf++ = 1; *buf++ = 1; *buf++ = 255;
            } else {
              *(buf++)=im[2];
              *(buf++)=im[1];
              *(buf++)=im[0];
              *(buf++)=im[3];
            }
            //buf++;
            break;
          case 3:
            if ((do_dither && IS_BACKGROUND(im[3])) ||
                (im[3] == 255 && im[0] == 0 && im[1] == 0 && im[2] == 0)) {
              *buf++ = 1; *buf++ = 1; *buf++ = 1;
            } else {
              *(buf++)=im[2];
              *(buf++)=im[1];
              *(buf++)=im[0];
            }
            break;
          case 2: // 565 RGB
            if ((do_dither && IS_BACKGROUND(im[3])) ||
                (im[3] == 255 && im[0] == 0 && im[1] == 0 && im[2] == 0)) {
              *buf++ = 0x21; *buf++ = 0x08;
            } else {
              *(buf++)= ((im[2] >> 3)& 0x1F) | ((im[1] & 0x1C) << 3);
              *(buf++)= (im[0] & 0xF8) | (im[1] >> 5);
            }
            break;
          default:
            dsyslog("[video] Unsupported depth %d exiting",depth);
            exit(1);
        }
        prev_pix = !IS_BACKGROUND(im[3]);


      } else  {
        buf += depth; // skip this pixel
      }
      im +=4;
    }
    buf += dx;
  }
  return;
}

#endif
