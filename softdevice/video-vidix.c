/*
 * video-vidix.c: A plugin for the Video Disk Recorder
 *
 * See the README file for copyright information and how to reach the author.
 *
 * $Id: video-vidix.c,v 1.27 2007/05/10 21:57:26 wachm Exp $
 */

#include <sys/mman.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <vdr/plugin.h>
#include "video-vidix.h"
#include "utils.h"
#include "setup-softdevice.h"

//#define TIMINGS

#ifdef TIMINGS
uint64_t startTime;
#define START startTime=getTimeMilis()
#define TIMINGS(out...)  {printf("time %d: ",getTimeMilis()-startTime);printf(out);}
#else
#define TIMINGS(out...)
#define START
#endif

/* ---------------------------------------------------------------------------
 */
cVidixVideoOut::cVidixVideoOut(cSetupStore *setupStore, cSetupSoftlog *Softlog)
                  : cVideoOut(setupStore,Softlog)
{
    int     err;
    double  displayRatio;
    char    *fbName = getFBName();

    if ((fbdev = open(fbName, O_RDWR)) == -1) {
        softlog->Log(SOFT_LOG_ERROR, 0,
                  "[cVidixVideoOut] Can't open framebuffer exiting\n");
        free(fbName);
        exit(1);
    }
    free(fbName);

    if (ioctl(fbdev, FBIOGET_VSCREENINFO, &fb_vinfo)) {
        softlog->Log(SOFT_LOG_ERROR, 0,
                  "[cVidixVideoOut] Can't get VSCREENINFO exiting\n");
        exit(1);
    }
    if (ioctl(fbdev, FBIOGET_FSCREENINFO, &fb_finfo)) {
        softlog->Log(SOFT_LOG_ERROR, 0,
                  "[cVidixVideoOut] Can't get FSCREENINFO exiting\n");
        exit(1);
    }

    switch (fb_finfo.visual) {

       case FB_VISUAL_TRUECOLOR:
           softlog->Log(SOFT_LOG_INFO, 0,
                     "[cVidixVideoOut] Truecolor FB found\n");
           break;

       case FB_VISUAL_DIRECTCOLOR:

           struct fb_cmap cmap;
           __u16 red[256], green[256], blue[256];

           softlog->Log(SOFT_LOG_INFO, 0,
                     "[cVidixVideoOut] DirectColor FB found\n");

           orig_cmaplen = 256;
           orig_cmap = (__u16 *) malloc ( 3 * orig_cmaplen * sizeof(*orig_cmap) );

           if ( orig_cmap == NULL ) {
               softlog->Log(SOFT_LOG_ERROR, 0,
                         "[cVidixVideoOut] Can't alloc memory for cmap exiting\n");
               exit(1);
           }
           cmap.start  = 0;
           cmap.len    = orig_cmaplen;
           cmap.red    = &orig_cmap[0*orig_cmaplen];
           cmap.green  = &orig_cmap[1*orig_cmaplen];
           cmap.blue   = &orig_cmap[2*orig_cmaplen];
           cmap.transp = NULL;

           if ( ioctl(fbdev, FBIOGETCMAP, &cmap)) {
               softlog->Log(SOFT_LOG_ERROR, 0,
                         "[cVidixVideoOut] Can't get cmap\n");
           }

           for ( int i=0; i < orig_cmaplen; ++i ) {
               red[i] = green[i] = blue[i] = (i<<8)|i;
           }

           cmap.start  = 0;
           cmap.len    = orig_cmaplen;
           cmap.red    = red;
           cmap.green  = green;
           cmap.blue   = blue;
           cmap.transp = NULL;

           if ( ioctl(fbdev, FBIOPUTCMAP, &cmap)) {
               softlog->Log(SOFT_LOG_ERROR, 0,
                         "[cVidixVideoOut] Can't put cmap\n");
           }

           break;

       default:
           softlog->Log(SOFT_LOG_ERROR, 0,
                     "[cVidixVideoOut] Unsupported FB. Don't know if it will work.\n");
    }

    fb_line_len = fb_finfo.line_length;
    Xres        = fb_vinfo.xres;
    Yres        = fb_vinfo.yres;
    Bpp         = fb_vinfo.bits_per_pixel;

    /*
     * default settings: source, destination and logical widht/height
     * are set to our well known dimensions.
     */
    fwidth = lwidth = dwidth = Xres;
    fheight = lheight = dheight = Yres;
    useVidixAlpha = false;
    swidth = 720;
    sheight = 576;

    displayRatio = (double) Xres / (double) Yres;
    SetParValues(displayRatio, displayRatio);

    softlog->Log(SOFT_LOG_INFO, 0,
              "[cVidixVideoOut] xres = %d yres= %d \n", fb_vinfo.xres, fb_vinfo.yres);
    softlog->Log(SOFT_LOG_INFO, 0,
              "[cVidixVideoOut] line length = %d \n", fb_line_len);
    softlog->Log(SOFT_LOG_INFO, 0,
              "[cVidixVideoOut] bpp = %d\n", fb_vinfo.bits_per_pixel);

    fb = (uint8_t *) mmap(0, fb_finfo.smem_len, PROT_READ | PROT_WRITE, MAP_SHARED, fbdev, 0);

    if ( fb == (uint8_t *) -1 ) {
       softlog->Log(SOFT_LOG_ERROR, 0,
                 "[cVidixVideoOut] Can't mmap framebuffer memory exiting\n");
       exit(1);
     }

    memset(fb,  0, fb_line_len * Yres );

    vidix_version = vdlGetVersion();

    softlog->Log(SOFT_LOG_INFO, 0,
              "[cVidixVideoOut] vidix version: %i\n", vidix_version);

    vidix_handler = vdlOpen(VIDIX_DIR, NULL, TYPE_OUTPUT, 0);

    if( !vidix_handler )
    {
       softlog->Log(SOFT_LOG_ERROR, 0,
                 "[cVidixVideoOut] Couldn't find working VIDIX driver exiting\n");
       exit(1);
    }

    if( (err = vdlGetCapability(vidix_handler,&vidix_cap)) != 0)
    {
       softlog->Log(SOFT_LOG_ERROR, 0,
                 "[cVidixVideoOut] Couldn't get capability: %s exiting\n",
                 strerror(err) );
       exit(1);
    }
    softlog->Log(SOFT_LOG_INFO, 0,
              "[cVidixVideoOut] capabilities:  0x%0x\n", vidix_cap.flags );

    OSDpseudo_alpha = true;

    if (setupStore->pixelFormat == 0)
      vidix_fourcc.fourcc = IMGFMT_I420;
    else if(setupStore->pixelFormat == 1)
      vidix_fourcc.fourcc = IMGFMT_YV12;
    else if (setupStore->pixelFormat == 2)
      vidix_fourcc.fourcc = IMGFMT_YUY2;

    currentPixelFormat = setupStore->pixelFormat;
    screenPixelAspect = -1;

    if (vdlQueryFourcc(vidix_handler, &vidix_fourcc))
    {
      if (!MatchPixelFormat())
      {
        softlog->Log(SOFT_LOG_ERROR, 0,
                  "[cVidixVideoOut] no matching pixel format found exiting\n");
        exit(1);
      }
    }

    memset(&vidix_play, 0, sizeof(vidix_playback_t));

    vidix_play.fourcc       = vidix_fourcc.fourcc;
    vidix_play.capability   = vidix_cap.flags;
    vidix_play.blend_factor = 0;
    vidix_play.src.x        = 0;
    vidix_play.src.y        = 0;
    vidix_play.src.w  = swidth;
    vidix_play.src.h  = sheight;
    vidix_play.src.pitch.y  = swidth;
    vidix_play.src.pitch.u  = swidth/2;
    vidix_play.src.pitch.v  = swidth/2;
    vidix_play.dest.x       = 0;
    vidix_play.dest.y       = 0;
    vidix_play.dest.w       = Xres;
    vidix_play.dest.h       = Yres;
    vidix_play.num_frames   = 2;

    softlog->Log(SOFT_LOG_INFO, 0,
              "[cVidixVideoOut] fourcc.flags:  0x%0x\n",vidix_fourcc.flags);
    AllocLayer();

    /* -----------------------------------------------------------------------
     * check presence of equalizer capability
     */
    vidix_video_eq_t tmpeq;
    softlog->Log(SOFT_LOG_INFO, 0,
              "[cVidixVideoOut] capabilities\n");

    if(!vdlPlaybackGetEq(vidix_handler, &tmpeq)) {
        vidix_curr_eq=tmpeq;
        softlog->Log(SOFT_LOG_INFO, 0,
                  "[cVidixVideoOut] EQ cap: %x:\n",
                  vidix_curr_eq.cap);
        if (vidix_curr_eq.cap & VEQ_CAP_BRIGHTNESS) {
            softlog->Log(SOFT_LOG_INFO, 0,
                      "[cVidixVideoOut]  brightness (%d)\n",
                      vidix_curr_eq.brightness);
            setupStore->vidCaps |= CAP_BRIGHTNESS;
        }
        if (vidix_curr_eq.cap & VEQ_CAP_CONTRAST) {
            softlog->Log(SOFT_LOG_INFO, 0,
                      "[cVidixVideoOut]  contrast (%d)\n",
                      vidix_curr_eq.contrast);
            setupStore->vidCaps |= CAP_CONTRAST;
        }
        if (vidix_curr_eq.cap & VEQ_CAP_SATURATION) {
            softlog->Log(SOFT_LOG_INFO, 0,
                      "[cVidixVideoOut]  saturation (%d)\n",
                      vidix_curr_eq.saturation);
            setupStore->vidCaps |= CAP_SATURATION;
        }
        if (vidix_curr_eq.cap & VEQ_CAP_HUE) {
            softlog->Log(SOFT_LOG_INFO, 0,
                      "[cVidixVideoOut]  hue (%d)\n",
                      vidix_curr_eq.hue);
            setupStore->vidCaps |= CAP_HUE;
        }
        if (vidix_curr_eq.cap & VEQ_CAP_RGB_INTENSITY) {
            softlog->Log(SOFT_LOG_INFO, 0,
                      "[cVidixVideoOut]  RGB-intensity (%d, %d, %d)\n",
                      vidix_curr_eq.red_intensity,
                      vidix_curr_eq.green_intensity,
                      vidix_curr_eq.blue_intensity );
        }
    } else {
       softlog->Log(SOFT_LOG_INFO, 0,
                 "[cVidixVideoOut] Couldn't get EQ capability\n");
    }

    /* -----------------------------------------------------------------------
     * check presence of hw deinterlace capability
     */
    vidix_deinterlace_t vidix_deint;

    if (!vdlPlaybackGetDeint(vidix_handler, &vidix_deint) &&
        !vdlPlaybackSetDeint(vidix_handler, &vidix_deint)) {
        softlog->Log(SOFT_LOG_INFO, 0,
                  "[cVidixVideoOut]  HW-deinterlace (%x,%x)\n",
                  vidix_deint.flags,
                  vidix_deint.deinterlace_pattern);

        setupStore->vidCaps |= CAP_HWDEINTERLACE;
    }
}

/* ----------------------------------------------------------------------------
 */
bool cVidixVideoOut::MatchPixelFormat()
{
  for (int i = 0; i < 3; ++i)
  {
    switch (i)
    {
      case 0: vidix_fourcc.fourcc = IMGFMT_I420; break;
      case 1: vidix_fourcc.fourcc = IMGFMT_YV12; break;
      case 2: vidix_fourcc.fourcc = IMGFMT_YUY2; break;
    }
    if (!vdlQueryFourcc(vidix_handler, &vidix_fourcc))
    {
      currentPixelFormat = setupStore->pixelFormat = i;
      return true;
    }
  }
  return false;
}

/* ----------------------------------------------------------------------------
 */
void cVidixVideoOut::SetParams(int Ystride, int UVstride)
{
    vidix_video_eq_t  vidix_video_eq;
    int               err;

  if (aspect_changed || currentPixelFormat != setupStore->pixelFormat ||
      cutTop != setupStore->cropTopLines ||
      cutBottom != setupStore->cropBottomLines )
  {
    cutTop    = setupStore->cropTopLines;
    cutBottom = setupStore->cropBottomLines;

    softlog->Log(SOFT_LOG_INFO, 0,
              "[cVidixVideoOut] Video changed format to %dx%d\n",
              fwidth, fheight);

    if((Xres > fwidth || Yres > fheight) &&
       (vidix_cap.flags & FLAG_UPSCALER) != FLAG_UPSCALER)
    {
      softlog->Log(SOFT_LOG_ERROR, 0,
                "[cVidixVideoOut] vidix driver can't "
                "upscale image (%dx%d -> %dx%d) exiting\n",
                fwidth, fheight, Xres, Yres);
      exit(1);
    }

    if((Xres < fwidth || Yres < fheight) &&
       (vidix_cap.flags & FLAG_DOWNSCALER) != FLAG_DOWNSCALER)
    {
      softlog->Log(SOFT_LOG_ERROR, 0,
                "[cVidixVideoOut] vidix driver can't "
                "downscale image (%dx%d -> %dx%d) exiting\n",
                fwidth, fheight, Xres, Yres);
      exit(1);
    }

    if (currentPixelFormat != setupStore->pixelFormat)
    {
      if (setupStore->pixelFormat == 0)
        vidix_fourcc.fourcc = IMGFMT_I420;
      else if(setupStore->pixelFormat == 1)
        vidix_fourcc.fourcc = IMGFMT_YV12;
      else if(setupStore->pixelFormat == 2)
        vidix_fourcc.fourcc = IMGFMT_YUY2;

      currentPixelFormat = setupStore->pixelFormat;
      if (vdlQueryFourcc(vidix_handler, &vidix_fourcc))
      {
        if (!MatchPixelFormat())
        {
          softlog->Log(SOFT_LOG_ERROR, 0,
                    "[cVidixVideoOut]: no matching pixel "
                    "format found exiting\n");
          exit(1);
        }
      }
    }

    vidix_play.fourcc       = vidix_fourcc.fourcc;

    vidix_play.src.w  = swidth;
    vidix_play.src.h  = sheight;

    vidix_play.dest.x = lxoff;
    vidix_play.dest.y = lyoff;
    vidix_play.dest.w = lwidth;
    vidix_play.dest.h = lheight;

    vidix_play.src.pitch.y=Ystride;
    vidix_play.src.pitch.u=UVstride;
    vidix_play.src.pitch.v=UVstride;

    AllocLayer();
#if 0
    printf("cVidixVideoOut : num_frames=%d \n", vidix_play.num_frames);
    printf("cVidixVideoOut : frame_size=%d\n",  vidix_play.frame_size);

    printf("cVidixVideoOut : src pitches y=%d\n", vidix_play.src.pitch.y);
    printf("cVidixVideoOut : src pitches u=%d\n", vidix_play.src.pitch.u);
    printf("cVidixVideoOut : src pitches v=%d\n", vidix_play.src.pitch.v);

    printf("cVidixVideoOut : dst pitches y=%d\n", vidix_play.dest.pitch.y);
    printf("cVidixVideoOut : dst pitches u=%d\n", vidix_play.dest.pitch.u);
    printf("cVidixVideoOut : dst pitches v=%d\n", vidix_play.dest.pitch.v);

    printf("cVidixVideoOut : dstrides.y=%d\n", dstrides.y);
    printf("cVidixVideoOut : dstrides.u=%d\n", dstrides.u);
    printf("cVidixVideoOut : dstrides.v=%d\n", dstrides.v);
#endif
  }

  vidix_video_eq=vidix_curr_eq;

  vidix_video_eq.brightness = (setupStore->vidBrightness - 50)*20;
  vidix_video_eq.contrast = (setupStore->vidContrast - 50)*20;
  vidix_video_eq.saturation = (setupStore->vidSaturation - 50)*20;
  vidix_video_eq.hue = (setupStore->vidHue - 50)*20;

  if (vidix_curr_eq.brightness != vidix_video_eq.brightness ||
      vidix_curr_eq.contrast != vidix_video_eq.contrast ||
      vidix_curr_eq.saturation != vidix_video_eq.saturation ||
      vidix_curr_eq.hue != vidix_video_eq.hue ) {

      softlog->Log(SOFT_LOG_INFO, 0,
                "[cVidixVideoOut] set eq values\n");
      vidix_curr_eq = vidix_video_eq;
      if( (err = vdlPlaybackSetEq(vidix_handler, &vidix_curr_eq)) != 0) {
          softlog->Log(SOFT_LOG_ERROR, 0,
                    "[cVidixVideoOut] Couldn't set EQ "
                    "capability: %s (FAILED)\n",
                    strerror(err) );
      }
  }

  if (setupStore->vidDeinterlace > 10)
      setupStore->vidDeinterlace = 10;

  if (setupStore->vidDeinterlace != vidix_curr_deinterlace) {

      vidix_deinterlace_t vidix_deint;

      vidix_curr_deinterlace = setupStore->vidDeinterlace;
      switch (vidix_curr_deinterlace)
      {
          case 0: // CFG_NON_INTERLACED
              vidix_deint.flags = CFG_NON_INTERLACED;
              break;
          case 1: // CFG_INTERLACED
          case 2: // CFG_EVEN_ODD_INTERLACING
          case 3:
              vidix_deint. flags = CFG_EVEN_ODD_INTERLACING;
              break;
          case 4: // CFG_ODD_EVEN_INTERLACING
          case 5:
          case 6:
          case 7:
              vidix_deint. flags = CFG_ODD_EVEN_INTERLACING;
              break;
          case 8: // CFG_UNIQUE_INTERLACING
                  // xorg METHOD_BOB
              vidix_deint. flags = CFG_UNIQUE_INTERLACING;
              vidix_deint. deinterlace_pattern = 0xAAAAA;
              break;
          case 9: // CFG_UNIQUE_INTERLACING
                  // xorg METHOD_SINGLE
              vidix_deint. flags = CFG_UNIQUE_INTERLACING;
              vidix_deint. deinterlace_pattern = 0xEEEEE | (9<<28);
              break;
          case 10: // xorg METHOD_WEAVE
              vidix_deint. flags = CFG_UNIQUE_INTERLACING;
              vidix_deint. deinterlace_pattern = 0;
              break;
          default:
              vidix_deint.flags = CFG_NON_INTERLACED;
              break;
      }
      vdlPlaybackSetDeint(vidix_handler, &vidix_deint);
      vdlPlaybackGetDeint(vidix_handler, &vidix_deint);
      softlog->Log(SOFT_LOG_INFO, 0,
                "[cVidixVideoOut] Deinterlacer: %x %x\n",
                vidix_deint.flags,
                vidix_deint.deinterlace_pattern);
  }
}

/* ----------------------------------------------------------------------------
 */
void cVidixVideoOut::AllocLayer(void)
{
    int       err;
    uint8_t   *dst;
    uint32_t  apitch;

  if (currentPixelFormat == 2)
    vidix_play.src.pitch.y *= 2;

  if ((err = vdlPlaybackOff(vidix_handler)) != 0)
  {
    softlog->Log(SOFT_LOG_ERROR, 0,
              "[cVidixVideoOut] Can't stop playback: %s exiting\n",
              strerror(err));
    exit(1);
  }

  if ((err = vdlConfigPlayback(vidix_handler, &vidix_play)) != 0)
  {
    softlog->Log(SOFT_LOG_ERROR, 0,
               "[cVidixVideoOut] Can't configure playback: %s exiting\n",
               strerror(err));
    exit(1);
  }

  if ((err = vdlPlaybackOn(vidix_handler)) != 0)
  {
    softlog->Log(SOFT_LOG_ERROR, 0,
              "[cVidixVideoOut] Can't start playback: %s exiting\n",
              strerror(err));
    exit(1);
  }

  if (vidix_fourcc.flags & VID_CAP_COLORKEY)
  {
    int res = 0;

    softlog->Log(SOFT_LOG_INFO, 0,
              "[cVidixVideoOut] set colorkey\n");
    vdlGetGrKeys(vidix_handler, &gr_key);

    gr_key.key_op = KEYS_PUT;
#ifdef CKEY_ALPHA
    if (vidix_fourcc.flags & VID_CAP_BLEND) {
      useVidixAlpha = true;
      gr_key.ckey.op = CKEY_ALPHA;
    } else {
      gr_key.ckey.op = CKEY_TRUE;
    }
#else
    gr_key.ckey.op = CKEY_TRUE;
#endif
    gr_key.ckey.red = gr_key.ckey.green = gr_key.ckey.blue = 0;
    res = vdlSetGrKeys(vidix_handler, &gr_key);
    softlog->Log(SOFT_LOG_INFO, 0,
              "[cVidixVideoOut] vdlSetGrKeys() = %d\n", res);

    if (res) {
      gr_key.ckey.op = CKEY_TRUE;
      res = vdlSetGrKeys(vidix_handler, &gr_key);
      softlog->Log(SOFT_LOG_INFO, 0,
                "[cVidixVideoOut] vdlSetGrKeys() = %d (noAlpha)\n", res);
      useVidixAlpha = false;
    }
  }

  next_frame = 0;

  apitch     = vidix_play.dest.pitch.y-1;
  dstrides.y = (swidth + apitch) & ~apitch;

  apitch     = vidix_play.dest.pitch.v-1;
  dstrides.v = (swidth + apitch) & ~apitch;

  apitch     = vidix_play.dest.pitch.u-1;
  dstrides.u = (swidth + apitch) & ~apitch;

  // clear every frame
  for (uint8_t i = 0; i < vidix_play.num_frames; i++)
  {
    dst = (uint8_t *) vidix_play.dga_addr + vidix_play.offsets[i] +
                      vidix_play.offset.y;
    if (currentPixelFormat == 2)
    {
        int *ldst = (int *) dst;

      for (unsigned int j = 0; j < dstrides.y * sheight/2; j++)
      {
        *ldst++ = 0x80008000;
      }
    }
    else
    {
      memset(dst, 0x00, dstrides.y * sheight);

      dst = (uint8_t *) vidix_play.dga_addr + vidix_play.offsets[i] +
                        vidix_play.offset.u;
      memset(dst, 0x80, (dstrides.u/2) * (sheight/2));

      dst = (uint8_t *) vidix_play.dga_addr + vidix_play.offsets[i] +
                        vidix_play.offset.v;
      memset(dst, 0x80, (dstrides.v/2) * (sheight/2));
    }
  }

}

/* ----------------------------------------------------------------------------
 */
void cVidixVideoOut::Pause(void)
{
}

/* ----------------------------------------------------------------------------
 */
void cVidixVideoOut::YUV(sPicBuffer *buf)
{
  uint8_t *dst;
  int hi, wi;
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

  if (!videoInitialized)
    return;

  START;
  TIMINGS("start...\n");
  SetParams(Ystride, UVstride);
  TIMINGS("after if, before Y\n");

  // Plane Y
  dst = (uint8_t *) vidix_play.dga_addr +
        vidix_play.offsets[next_frame] +
        vidix_play.offset.y;

  Py += (Ystride * syoff);
  Pv += (UVstride * syoff/2);
  Pu += (UVstride * syoff/2);

  if (currentPixelFormat == 0 || currentPixelFormat == 1)
  {
    if (OSDpresent && current_osdMode==OSDMODE_SOFTWARE)
    {
      for (hi=0; hi < sheight; hi++){
        AlphaBlend(dst,
                   OsdPy+hi*OSD_FULL_WIDTH,
                   Py + sxoff,
                   OsdPAlphaY+hi*OSD_FULL_WIDTH,
                   swidth);
        Py  += Ystride;
        dst += dstrides.y;
      }

      // Plane U
      dst = (uint8_t *)vidix_play.dga_addr +
            vidix_play.offsets[next_frame] +
            vidix_play.offset.u;
      for (hi=0; hi < sheight/2; hi++) {
        AlphaBlend(dst,
                   OsdPu+hi*OSD_FULL_WIDTH/2,
                   Pu + sxoff/2,
                   OsdPAlphaUV+hi*OSD_FULL_WIDTH/2,
                   swidth/2);
        Pu  += UVstride;
        dst += dstrides.y / 2;
      }

      // Plane V
      dst = (uint8_t *)vidix_play.dga_addr +
            vidix_play.offsets[next_frame] +
            vidix_play.offset.v;
      for (hi=0; hi < sheight/2; hi++) {
        AlphaBlend(dst,
                   OsdPv+hi*OSD_FULL_WIDTH/2,
                   Pv + sxoff/2,
                   OsdPAlphaUV+hi*OSD_FULL_WIDTH/2,
                   swidth/2);
        Pv  += UVstride;
        dst += dstrides.v / 2;
      }
    } else
    {
        int chromaWidth  = swidth >> 1;
        int chromaOffset = sxoff >> 1;

      Py += Ystride  * cutTop * 2;
      Pv += UVstride * cutTop;
      Pu += UVstride * cutTop;

      dst += dstrides.y * cutTop * 2;

      for(hi=cutTop*2; hi < sheight-cutBottom*2; hi++)
      {
        memcpy(dst, Py+sxoff, swidth);
        Py  += Ystride;
        dst += dstrides.y;
      }

      TIMINGS("Before YUV\n");

      if (vidix_play.flags & VID_PLAY_INTERLEAVED_UV)
      {
          int dstStride = dstrides.v << 1;

        dst = (uint8_t *)vidix_play.dga_addr +
              vidix_play.offsets[next_frame] +
              vidix_play.offset.v +
              dstStride * cutTop;

        for(hi = cutTop; hi < sheight/2; hi++)
        {
            uint16_t  *idst = (uint16_t *) dst;
            uint8_t   *usrc = Pu + chromaOffset,
                      *vsrc = Pv + chromaOffset;

          for(wi = 0; wi < chromaWidth; wi++)
          {
            *idst++ = ( usrc[0] << 8 ) + vsrc[0];
            usrc++;
            vsrc++;
          }

          dst += dstStride;
          Pu += UVstride;
          Pv += UVstride;
        }
      } else {
          int dstStride;

        // Plane U
        dstStride = dstrides.u >> 1;
        dst = (uint8_t *)vidix_play.dga_addr +
              vidix_play.offsets[next_frame] +
              vidix_play.offset.u +
              dstStride * cutTop;

        for(hi=cutTop; hi < sheight/2 - cutBottom; hi++)
        {
          memcpy(dst, Pu+chromaOffset, chromaWidth);
          Pu  += UVstride;
          dst += dstStride;
        }

        // Plane V
        dstStride = dstrides.v >> 1;
        dst = (uint8_t *)vidix_play.dga_addr +
              vidix_play.offsets[next_frame] +
              vidix_play.offset.v +
              dstStride * cutTop;

        for(hi=cutTop; hi < sheight/2 - cutBottom; hi++)
        {
          memcpy(dst, Pv+chromaOffset, chromaWidth);
          Pv   += UVstride;
          dst += dstStride;
        }
      }
    }
  } else if (currentPixelFormat == 2) {
    current_osdMode = setupStore->osdMode = OSDMODE_PSEUDO;
    if (interlaceMode) {
      yv12_to_yuy2_il_mmx2(Py + Ystride  * cutTop * 2,
                        Pu + UVstride * cutTop,
                        Pv + UVstride * cutTop,
                        dst + dstrides.y*2 * cutTop * 2,
                        Width, Height - 2 * (cutTop + cutBottom),
                        Ystride, UVstride, dstrides.y*2);
    } else {
      yv12_to_yuy2_fr_mmx2(Py + Ystride  * cutTop * 2,
                           Pu + UVstride * cutTop,
                           Pv + UVstride * cutTop,
                           dst + dstrides.y*2 * cutTop * 2,
                           Width, Height - 2 * (cutTop + cutBottom),
                           Ystride, UVstride, dstrides.y*2);
    }
  }

  TIMINGS("After UV\n");
  vdlPlaybackFrameSelect(vidix_handler, next_frame);
  next_frame = (next_frame+1) % vidix_play.num_frames;
  TIMINGS("End\n");
}

/* ---------------------------------------------------------------------------
 */
void cVidixVideoOut::ClearOSD()
{
    if (!videoInitialized)
        return;

    cVideoOut::ClearOSD();
    if (current_osdMode==OSDMODE_PSEUDO)
        memset(fb, 0, fb_line_len * Yres);
}

/* ---------------------------------------------------------------------------
 */
void cVidixVideoOut::AdjustOSDMode()
{
  current_osdMode = setupStore->osdMode;
}

/* ---------------------------------------------------------------------------
 */
void cVidixVideoOut::GetOSDMode(int &Depth, bool &HasAlpha, bool &AlphaInversed,
                              bool &IsYUV)
{
  Depth         = Bpp;
  HasAlpha      = useVidixAlpha;
  AlphaInversed = false;
  IsYUV         = (current_osdMode == OSDMODE_SOFTWARE);
}

/* ---------------------------------------------------------------------------
 */
void cVidixVideoOut::GetOSDDimension(int &OsdWidth,int &OsdHeight,
                                     int &xPan, int &yPan)
{
   switch (current_osdMode) {
      case OSDMODE_PSEUDO :
                OsdWidth=Xres;//*9/10;
                OsdHeight=Yres;//*9/10;
                xPan = yPan = 0;
             break;
      case OSDMODE_SOFTWARE:
                OsdWidth=swidth;//*9/10;
                OsdHeight=sheight;//*9/10;
                xPan = sxoff;
                yPan = syoff;
             break;
    }
}


/* ---------------------------------------------------------------------------
 */
void cVidixVideoOut::GetLockOsdSurface(uint8_t *&osd, int &stride,
                bool *&dirtyLines)
{
    osd = NULL;
    stride = 0;

    if (!videoInitialized)
        return;

    osd=fb;
    stride=fb_line_len;
    dirtyLines=NULL;
}

/* ---------------------------------------------------------------------------
 */
void cVidixVideoOut::CloseOSD()
{
    if (!videoInitialized)
        return;

    cVideoOut::CloseOSD();
    memset(fb, 0, fb_line_len * Yres);
}

/* ---------------------------------------------------------------------------
 */
cVidixVideoOut::~cVidixVideoOut()
{
    int err;

    softlog->Log(SOFT_LOG_DEBUG, 0,
              "[cVidixVideoOut] destructor\n");

    if(vidix_handler) {
        if((err = vdlPlaybackOff(vidix_handler)) != 0) {
           softlog->Log(SOFT_LOG_ERROR, 0,
                     "[cVidixVideoOut] Can't stop playback: %s\n",
                     strerror(err));
        }
        vdlClose(vidix_handler);
    }
    switch (fb_finfo.visual) {
       case FB_VISUAL_DIRECTCOLOR:
       {
           struct fb_cmap cmap;

           if ( orig_cmap ) {

               cmap.start  = 0;
               cmap.len    = orig_cmaplen;
               cmap.red    = &orig_cmap[0*orig_cmaplen];
               cmap.green  = &orig_cmap[1*orig_cmaplen];
               cmap.blue   = &orig_cmap[2*orig_cmaplen];
               cmap.transp = NULL;

               if ( ioctl(fbdev, FBIOPUTCMAP, &cmap)) {
                   softlog->Log(SOFT_LOG_ERROR, 0,
                             "[cVidixVideoOut] Can't put cmap\n");
               }

               free(orig_cmap);
               orig_cmap = NULL;
           }
           break;
       }
    }

    if (fbdev) close(fbdev);
}

#ifdef USE_SUBPLUGINS
/* ---------------------------------------------------------------------------
 */
extern "C" void *
SubPluginCreator(cSetupStore *s, cSetupSoftlog *log)
{
  return new cVidixVideoOut(s,log);
}
#endif
