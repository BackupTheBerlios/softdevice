/*
 * video.c: A plugin for the Video Disk Recorder
 *
 * See the README file for copyright information and how to reach the author.
 *
 * $Id: video.c,v 1.4 2004/12/21 05:55:43 lucke Exp $
 */

#include <sys/mman.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <vdr/plugin.h>
#include "video.h"
#include "utils.h"
#include "setup-softdevice.h"


cVideoOut::~cVideoOut()
{
  dsyslog("[VideoOut]: Good bye");
}

/* ---------------------------------------------------------------------------
 */
void cVideoOut::CheckAspect(int new_afd, float new_asp)
{
    int           new_aspect,
                  screenWidth, screenHeight;
    double        d_asp, afd_asp;

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
  new_afd = (setupStore.cropMode) ? setupStore.cropMode : new_afd;

  /* -------------------------------------------------------------------------
   * check for changes of screen width/height change
   */

  if (screenPixelAspect != setupStore.screenPixelAspect)
  {
    screenPixelAspect = setupStore.screenPixelAspect;
    /* -----------------------------------------------------------------------
     * force recalculation for aspect ration handling
     */
    current_aspect = -1;
  }

  if (new_aspect == current_aspect && new_afd == current_afd)
  {
    aspect_changed = 0;
    return;
  }

  setupStore.getScreenDimension (screenWidth, screenHeight);
  aspect_changed = 1;

  d_asp = (double) dwidth / (double) dheight;
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

  sheight = fheight;
  swidth = fwidth;
  if (afd_asp <= new_asp) {
    swidth = (int) (0.5 + ((double) fwidth * afd_asp / new_asp));
  } else {
    sheight = (int) (0.5 + ((double) fheight * new_asp / afd_asp));
  }
  sxoff = (fwidth - swidth) / 2;
  syoff = (fheight - sheight) / 2;

  /* --------------------------------------------------------------------------
   * handle screen aspect support now
   */
  afd_asp *= ((double) screenWidth / (double) screenHeight) * (3.0 / 4.0);

  if (d_asp > afd_asp) {
    /* ------------------------------------------------------------------------
     * display aspect is wider than frame aspect
     * so we have to pillar-box
     */
    lheight = dheight;
    lwidth = (int) (0.5 + ((double) dheight * afd_asp));
    lxoff = (dwidth - lwidth) / 2;
    lyoff = 0;
  } else {
    /* ------------------------------------------------------------------------
     * display aspect is taller or equal than frame aspect
     * so we have to letter-box
     */
    lwidth = dwidth;
    lheight = (int) (0.5 + ((double) dwidth / afd_asp));
    lyoff = (dheight - lheight) / 2;
    lxoff = 0;
  }

  dsyslog("[VideoOut]: %dx%d [%d,%d %dx%d] -> %dx%d [%d,%d %dx%d]",
          fwidth, fheight, sxoff, syoff, swidth, sheight,
          dwidth, dheight, lxoff, lyoff, lwidth, lheight);

  current_aspect = new_aspect;
  current_afd = new_afd;
}

/* ---------------------------------------------------------------------------
 */
void cVideoOut::CheckAspectDimensions(AVFrame *picture,
                                        AVCodecContext *context)
{
    static float  new_asp, aspect_F = -100.0;
    static int    aspect_I = -100;

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

#if LIBAVCODEC_BUILD > 4686
  if (picture->pan_scan->width) {
    new_asp = (float) (picture->pan_scan->width *
                       context->sample_aspect_ratio.num) /
               (float) (picture->pan_scan->height *
                        context->sample_aspect_ratio.den);
  } else {
    new_asp = (float) (context->width *
                       context->sample_aspect_ratio.num) /
               (float) (context->height *
                        context->sample_aspect_ratio.den);
  }
#else
  new_asp = context->aspect_ratio;
#endif


  if (aspect_I != context->dtg_active_format ||
      aspect_F != new_asp)
  {
    dsyslog("[VideoOut]: aspect changed (%d -> %d ; %f -> %f)",
             aspect_I,context->dtg_active_format,
             aspect_F,new_asp);
#if LIBAVCODEC_BUILD > 4686
    if (picture->pan_scan->width) {
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

#define OPACITY_THRESHOLD 0x8F
#define TRANSPARENT_THRESHOLD 0x0F
#define IS_BACKGROUND(a) (((a) < OPACITY_THRESHOLD) && (a > TRANSPARENT_THRESHOLD))

#if VDRVERSNUM >= 10307

void cVideoOut::OpenOSD(int X, int Y)
{
  OSDxOfs = X;
  OSDyOfs = Y;
  OSDpresent=true;
}

void cVideoOut::CloseOSD()
{
  OSDpresent=false;
}

/* ---------------------------------------------------------------------------
 */
void cVideoOut::OSDStart()
{
  //fprintf (stderr, "+");
}

/* ---------------------------------------------------------------------------
 */
void cVideoOut::OSDCommit()
{
  //fprintf (stderr, "-");
}

/* ---------------------------------------------------------------------------
 */
void cVideoOut::Draw(cBitmap *Bitmap,
                     unsigned char *osd_buf,
                     int linelen,
                     bool inverseAlpha)
{
    int           depth = (Bpp + 7) / 8;
    int           a, r, g, b;
    bool          prev_pix = false, do_dither;
    tColor        c;
    tIndex        *buf;
    const tIndex  *adr;

  for (int y = 0; y < Bitmap->Height(); y++)
  {
    buf = (tIndex *) osd_buf +
            linelen * ( OSDyOfs + y ) +
            OSDxOfs * depth;
    prev_pix = false;

    for (int x = 0; x < Bitmap->Width(); x++)
    {
      do_dither = ((x % 2 == 1 && y % 2 == 1) ||
                    x % 2 == 0 && y % 2 == 0 || prev_pix);
      adr = Bitmap->Data(x, y);
      c = Bitmap->Color(*adr);
      a = (c >> 24) & 255; //Alpha
      r = (c >> 16) & 255; //Red
      g = (c >> 8) & 255;  //Green
      b = c & 255;         //Blue
      switch (depth) {
        case 4:
          if ((do_dither && IS_BACKGROUND(a) && OSDpseudo_alpha) ||
              (a == 255 && r == 0 && g == 0 && b == 0))
          {
            buf[0] = 1; buf[1] = 1; buf[2] = 1; buf[3] = 255;
          } else {
            if (inverseAlpha)
              a = 255 - a;

            buf[0] = b;
            buf[1] = g;
            buf[2] = r;
            buf[3] = a;
          }

          buf += 4;
          break;
        case 3:
          if ((do_dither && IS_BACKGROUND(a)) ||
              (a == 255 && r == 0 && g == 0 && b == 0))
          {
            buf[0] = 1; buf[1] = 1; buf[2] = 1;
          } else {
            buf[0] = b;
            buf[1] = g;
            buf[2] = r;
          }

          buf += 3;
          break;
        case 2:
          if ((do_dither && IS_BACKGROUND(a)) ||
              (a == 255 && r == 0 && g == 0 && b == 0))
          {
              buf[0] = 0x21; buf[1] = 0x08;
          } else {
            buf[0] = ((b >> 3)& 0x1F) | ((g & 0x1C) << 3);
            buf[1] = (r & 0xF8) | (g >> 5);
          }

          buf += 2;
          break;
        default:
            dsyslog("[VideoOut] OSD: unsupported depth %d exiting",depth);
            exit(1);
          break;
      }
      prev_pix = !IS_BACKGROUND(a);
    }
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
    printf("[video] Creating WindowLayer at %d x %d, (%d x %d)\n",X,Y,W,H);
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
