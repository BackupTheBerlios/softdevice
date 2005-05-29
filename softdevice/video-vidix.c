/*
 * video-vidix.c: A plugin for the Video Disk Recorder
 *
 * See the README file for copyright information and how to reach the author.
 *
 * $Id: video-vidix.c,v 1.8 2005/05/29 19:50:44 lucke Exp $
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

cVidixVideoOut::cVidixVideoOut(cSetupStore *setupStore)
                  : cVideoOut(setupStore)
{
    int     err;
    double  displayRatio;

    if ((fbdev = open(FBDEV, O_RDWR)) == -1) {
        esyslog("[cVidixVideoOut] Can't open framebuffer exiting\n");
        exit(1);
    }

    if (ioctl(fbdev, FBIOGET_VSCREENINFO, &fb_vinfo)) {
        esyslog("[cVidixVideoOut] Can't get VSCREENINFO exiting\n");
        exit(1);
    }
    if (ioctl(fbdev, FBIOGET_FSCREENINFO, &fb_finfo)) {
        esyslog("[cVidixVideoOut] Can't get FSCREENINFO exiting\n");
        exit(1);
    }

    switch (fb_finfo.visual) {

       case FB_VISUAL_TRUECOLOR:
           printf("cVidixVideoOut: Truecolor FB found\n");
           break;

       case FB_VISUAL_DIRECTCOLOR:

           struct fb_cmap cmap;
           __u16 red[256], green[256], blue[256];

           printf("cVidixVideoOut: DirectColor FB found\n");

           orig_cmaplen = 256;
           orig_cmap = (__u16 *) malloc ( 3 * orig_cmaplen * sizeof(*orig_cmap) );

           if ( orig_cmap == NULL ) {
               esyslog("[cVidixVideoOut] Can't alloc memory for cmap exiting\n");
               exit(1);
           }
           cmap.start  = 0;
           cmap.len    = orig_cmaplen;
           cmap.red    = &orig_cmap[0*orig_cmaplen];
           cmap.green  = &orig_cmap[1*orig_cmaplen];
           cmap.blue   = &orig_cmap[2*orig_cmaplen];
           cmap.transp = NULL;

           if ( ioctl(fbdev, FBIOGETCMAP, &cmap)) {
               printf("cVidixVideoOut: Can't get cmap\n");
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
               printf("cVidixVideoOut: Can't put cmap\n");
           }

           break;

       default:
           printf("cVidixVideoOut: Unsupported FB. Don't know if it will work.\n");
    }

    fb_line_len = fb_finfo.line_length;
    Xres        = fb_vinfo.xres;
    Yres        = fb_vinfo.yres;
    Bpp         = fb_vinfo.bits_per_pixel;

    /*
     * default settings: source, destination and logical widht/height
     * are set to our well known dimensions.
     */
    fwidth = lwidth = dwidth = swidth = Xres;
    fheight = lheight = dheight = sheight = Yres;

    displayRatio = (double) Xres / (double) Yres;
    SetParValues(displayRatio, displayRatio);

    printf("cVidixVideoOut: xres = %d yres= %d \n", fb_vinfo.xres, fb_vinfo.yres);
    printf("cVidixVideoOut: line length = %d \n", fb_line_len);
    printf("cVidixVideoOut: bpp = %d\n", fb_vinfo.bits_per_pixel);

    fb = (uint8_t *) mmap(0, fb_finfo.smem_len, PROT_READ | PROT_WRITE, MAP_SHARED, fbdev, 0);

    if ( fb == (uint8_t *) -1 ) {
       esyslog("[cVidixVideoOut] Can't mmap framebuffer memory exiting\n");
       exit(1);
     }

    osd = (uint8_t *) malloc(fb_line_len * Yres);

    if ( osd == NULL ) {
       esyslog("[cVidixVideoOut] Can't alloc osd memory exiting\n");
       exit(1);
    }

    memset(osd, 0, fb_line_len * Yres );
    memset(fb,  0, fb_line_len * Yres );


    vidix_version = vdlGetVersion();

    printf("cVidixVideoOut: vidix version: %i\n", vidix_version);


//    printf("cVidixVideoOut: looking for driver: %s in %s\n", VIDIX_DRIVER, VIDIX_DIR);
//    vidix_handler = vdlOpen(VIDIX_DIR, VIDIX_DRIVER, TYPE_OUTPUT, 1);
    vidix_handler = vdlOpen(VIDIX_DIR, NULL, TYPE_OUTPUT, 1);

    if( !vidix_handler )
    {
       esyslog("[cVidixVideoOut] Couldn't find working VIDIX driver exiting\n");
       exit(1);
    }

    if( (err = vdlGetCapability(vidix_handler,&vidix_cap)) != 0)
    {
       esyslog("[cVidixVideoOut] Couldn't get capability: %s exiting\n",
               strerror(err) );
       exit(1);
    }
    printf("cVidixVideoOut: capabilities:  0x%0x\n", vidix_cap.flags );

    OSDpseudo_alpha = true;

    if (setupStore->pixelFormat == 0)
      vidix_fourcc.fourcc = IMGFMT_I420;
    else if(setupStore->pixelFormat == 1)
      vidix_fourcc.fourcc = IMGFMT_YV12;
    else if (setupStore->pixelFormat == 2)
      vidix_fourcc.fourcc = IMGFMT_YUY2;

    currentPixelFormat = setupStore->pixelFormat;
    screenPixelAspect = -1;

    vdlQueryFourcc(vidix_handler, &vidix_fourcc);
    if (vdlQueryFourcc(vidix_handler, &vidix_fourcc))
    {
      if (!matchPixelFormat())
      {
        esyslog("[cVidixVideoOut]: no matching pixel format found exiting\n");
        exit(1);
      }
    }

    memset(&vidix_play, 0, sizeof(vidix_playback_t));

    vidix_play.fourcc       = vidix_fourcc.fourcc;
    vidix_play.capability   = vidix_cap.flags;
    vidix_play.blend_factor = 0;
    vidix_play.src.x        = 0;
    vidix_play.src.y        = 0;
    vidix_play.src.pitch.y  = 0;
    vidix_play.src.pitch.u  = 0;
    vidix_play.src.pitch.v  = 0;
    vidix_play.dest.x       = 0;
    vidix_play.dest.y       = 0;
    vidix_play.dest.w       = Xres;
    vidix_play.dest.h       = Yres;
    vidix_play.num_frames   = 2;
    //vidix_play.num_frames   = 1;

    printf("cVidixVideoOut: fourcc.flags:  0x%0x\n",vidix_fourcc.flags);
    if( vidix_fourcc.flags & VID_CAP_COLORKEY )
    {
       printf("cVidixVideoOut: set colorkey\n");
       vdlGetGrKeys(vidix_handler, &gr_key);

       gr_key.key_op = KEYS_PUT;

       gr_key.ckey.op = CKEY_TRUE;
       //gr_key.ckey.red = gr_key.ckey.green = gr_key.ckey.blue = 32;
       gr_key.ckey.red = gr_key.ckey.green = gr_key.ckey.blue = 0;

       vdlSetGrKeys(vidix_handler, &gr_key);
    }
}

/* ----------------------------------------------------------------------------
 */
bool cVidixVideoOut::matchPixelFormat()
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
void cVidixVideoOut::Pause(void)
{
}

/* ----------------------------------------------------------------------------
 */
void cVidixVideoOut::YUV(uint8_t *Py, uint8_t *Pu, uint8_t *Pv, int Width, int Height, int Ystride, int UVstride)
{
    int err;
    uint8_t *dst;
    uint32_t apitch;
    int hi, wi;
    START;
    TIMINGS("start...\n");
    
    if (aspect_changed || currentPixelFormat != setupStore->pixelFormat)
    {
       printf("cVidixVideoOut: Video changed format to %dx%d\n", Width, Height);

       if((Xres > Width || Yres > Height) &&
          (vidix_cap.flags & FLAG_UPSCALER) != FLAG_UPSCALER)
       {
           esyslog("[cVidixVideoOut] vidix driver can't upscale image (%dx%d -> %dx%d) exiting\n",
                   Width, Height, Xres, Yres);
           exit(1);
       }

       if((Xres < Width || Yres < Height) &&
          (vidix_cap.flags & FLAG_DOWNSCALER) != FLAG_DOWNSCALER)
       {
           esyslog("[cVidixVideoOut] vidix driver can't downscale image (%dx%d -> %dx%d) exiting\n",
                   Width, Height, Xres, Yres);
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
          if (!matchPixelFormat())
          {
            esyslog("[cVidixVideoOut]: no matching pixel format found exiting\n");
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

       if (currentPixelFormat == 2)
         vidix_play.src.pitch.y *= 2;

       if((err = vdlPlaybackOff(vidix_handler)) != 0) {
           esyslog("[cVidixVideoOut] Can't stop playback: %s exiting\n",
                   strerror(err));
           exit(1);
       }

       if((err = vdlConfigPlayback(vidix_handler, &vidix_play)) != 0) {
           esyslog("[cVidixVideoOut] Can't configure playback: %s exiting\n",
                   strerror(err));
           exit(1);
       }

       if((err = vdlPlaybackOn(vidix_handler)) != 0) {
           esyslog("[cVidixVideoOut] Can't start playback: %s exiting\n",
                   strerror(err));
           exit(1);
       }

       if( vidix_fourcc.flags & VID_CAP_COLORKEY )
       {
         printf("cVidixVideoOut: set colorkey\n");
         vdlGetGrKeys(vidix_handler, &gr_key);

         gr_key.key_op = KEYS_PUT;

         gr_key.ckey.op = CKEY_TRUE;
         //gr_key.ckey.red = gr_key.ckey.green = gr_key.ckey.blue = 0xff;
         gr_key.ckey.red = gr_key.ckey.green = gr_key.ckey.blue = 0;

         vdlSetGrKeys(vidix_handler, &gr_key);
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
           dst = (uint8_t *) vidix_play.dga_addr + vidix_play.offsets[i] + vidix_play.offset.y;
           memset(dst, 0x00, dstrides.y * Height);

           dst = (uint8_t *)vidix_play.dga_addr + vidix_play.offsets[i] + vidix_play.offset.u;
           memset(dst, 0x80, (dstrides.u/2) * (Height/2));

           dst = (uint8_t *)vidix_play.dga_addr + vidix_play.offsets[i] + vidix_play.offset.v;
           memset(dst, 0x80, (dstrides.v/2) * (Height/2));
       }

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
    TIMINGS("after if, before Y\n");

    // Plane Y
    dst = (uint8_t *) vidix_play.dga_addr + vidix_play.offsets[next_frame] + vidix_play.offset.y;

    Py += (Ystride * syoff);
    Pv += (UVstride * syoff/2);
    Pu += (UVstride * syoff/2);

    if (currentPixelFormat == 0 || currentPixelFormat == 1)
    {
#if VDRVERSNUM >= 10307
      OsdRefreshCounter=0;
      if (OSDpresent && current_osdMode==OSDMODE_SOFTWARE) {
        for(hi=0; hi < sheight; hi++){
          AlphaBlend(dst,OsdPy+hi*OSD_FULL_WIDTH,
              Py + sxoff,
              OsdPAlphaY+hi*OSD_FULL_WIDTH,swidth);
          Py  += Ystride;
          dst += dstrides.y;

        }
        EMMS;
      } else
#endif

        for(hi=0; hi < sheight; hi++){
          memcpy(dst, Py+sxoff, swidth);
          Py  += Ystride;
          dst += dstrides.y;
        }

      TIMINGS("Before YUV\n");

      if (vidix_play.flags & VID_PLAY_INTERLEAVED_UV)
      {
        dst = (uint8_t *)vidix_play.dga_addr + vidix_play.offsets[next_frame] + vidix_play.offset.v;

        for(hi = 0; hi < sheight/2; hi++) {
          for(wi = 0; wi < swidth/2; wi++) {
            dst[2*wi+0] = Pu[wi+sxoff/2];
            dst[2*wi+1] = Pv[wi+sxoff/2];
          }

          dst += dstrides.y;
          Pu += UVstride;
          Pv += UVstride;
        }
      } else {

        // Plane U
        dst = (uint8_t *)vidix_play.dga_addr + vidix_play.offsets[next_frame] + vidix_play.offset.u;

#if VDRVERSNUM >= 10307
        if (OSDpresent && current_osdMode==OSDMODE_SOFTWARE) {
          for(hi=0; hi < sheight/2; hi++){
            AlphaBlend(dst,OsdPu+hi*OSD_FULL_WIDTH/2,
               Pu + sxoff/2,
               OsdPAlphaUV+hi*OSD_FULL_WIDTH/2,swidth/2);
            Pu  += UVstride;
            dst += dstrides.y / 2;
          }

          // Plane V
          dst = (uint8_t *)vidix_play.dga_addr + vidix_play.offsets[next_frame] + vidix_play.offset.v;
          for(hi=0; hi < sheight/2; hi++) {
            AlphaBlend(dst, OsdPv+hi*OSD_FULL_WIDTH/2,
                   Pv + sxoff/2,
                   OsdPAlphaUV+hi*OSD_FULL_WIDTH/2, swidth/2);
            Pv  += UVstride;
            dst += dstrides.v / 2;
          }

          EMMS;
        } else
#endif
        {
          for(hi=0; hi < sheight/2; hi++) {
            memcpy(dst, Pu+sxoff/2, swidth/2);
            Pu   += UVstride;
            dst += dstrides.u / 2;
          }

          // Plane V
          dst = (uint8_t *)vidix_play.dga_addr + vidix_play.offsets[next_frame] + vidix_play.offset.v;
          for(hi=0; hi < sheight/2; hi++) {
            memcpy(dst, Pv+sxoff/2, swidth/2);
            Pv   += UVstride;
            dst += dstrides.v / 2;
          }
        }
      }
    } else if (currentPixelFormat == 2) {

#ifndef USE_MMX

      /* reference implementation */
      //int p = vidix_play.src.pitch.y - Width*2;
      for (int i=0; i<Height; i++) {
        for (int j =0; j < Width/2; j++) {
          *dst = *(Py + (j*2) + i * Ystride);
          dst +=1;
          *dst = *(Pu + j + (i/2) * UVstride);
          dst +=1;
          *dst = *(Py + (j*2)+1 + i * Ystride);
          dst +=1;
          *dst = *(Pv + j + (i/2) * UVstride);
          dst +=1;
        }
        //dst +=p;
      }
#else
      /* deltas */
      for (int i=0; i<Height; i++) {
          uint8_t *pu, *pv, *py, *srfc;

        pu = Pu;
        pv = Pv;
        py = Py;
        srfc = dst;
        for (int j =0; j < Width/8; j++) {
          movd_m2r(*pu, mm1);       // mm1 = 00 00 00 00 U3 U2 U1 U0
          movd_m2r(*pv, mm2);       // mm2 = 00 00 00 00 V3 V2 V1 V0
          movq_m2r(*py, mm0);       // mm0 = Y7 Y6 Y5 Y4 Y3 Y2 Y1 Y0
          punpcklbw_r2r(mm2, mm1);  // mm1 = V3 U3 V2 U2 V1 U1 V0 U0
          movq_r2r(mm0,mm3);        // mm3 = Y7 Y6 Y5 Y4 Y3 Y2 Y1 Y0
          movq_r2r(mm1,mm4);        // mm4 = V3 U3 V2 U2 V1 U1 V0 U0
          punpcklbw_r2r(mm1, mm0);  // mm0 = V1 Y3 U1 Y2 V0 Y1 U0 Y0
          punpckhbw_r2r(mm4, mm3);  // mm3 = V3 Y7 U3 Y6 V2 Y5 U2 Y4

          movntq(mm0,*srfc);        // Am Meisten brauchen die Speicherzugriffe
          srfc+=8;
          py+=8;
          pu+=4;
          pv+=4;
          movntq(mm3,*srfc);      // wenn movntq nicht geht, dann movq verwenden
          srfc+=8;
        }
        Py += Ystride;;
        if (i % 2 == 1) {
          Pu += UVstride;
          Pv += UVstride;
        }
        dst += Width*2;
      }
#endif
    }

    TIMINGS("After UV\n");
    vdlPlaybackFrameSelect(vidix_handler, next_frame);
    next_frame = (next_frame+1) % vidix_play.num_frames;
    TIMINGS("End\n");
}

#if VDRVERSNUM >= 10307

/* ---------------------------------------------------------------------------
 */
void cVidixVideoOut::ClearOSD()
{
  cVideoOut::ClearOSD();
  if (current_osdMode==OSDMODE_PSEUDO)
    memset(fb, 0, fb_line_len * Yres);
};

/* ---------------------------------------------------------------------------
 */
void cVidixVideoOut::GetOSDDimension(int &OsdWidth,int &OsdHeight) {
   switch (current_osdMode) {
      case OSDMODE_PSEUDO :
                OsdWidth=Xres;//*9/10;
                OsdHeight=Yres;//*9/10;
             break;
      case OSDMODE_SOFTWARE:
                OsdWidth=swidth;//*9/10;
                OsdHeight=sheight;//*9/10;
             break;
    };
};


void cVidixVideoOut::Refresh(cBitmap *Bitmap)
{
    switch (current_osdMode) {
      case OSDMODE_PSEUDO :
              Draw(Bitmap, fb,fb_line_len);
            break;
      case OSDMODE_SOFTWARE:
              ToYUV(Bitmap);
            break;
    }
}

#else

void cVidixVideoOut::Refresh()
{

    for (int i = 0; i < MAXNUMWINDOWS; i++)
    {
        if (layer[i] && layer[i]->visible) layer[i]->Draw(fb, fb_line_len, NULL);
    }
}

#endif

void cVidixVideoOut::CloseOSD()
{
    cVideoOut::CloseOSD();
    memset(fb, 0, fb_line_len * Yres);
}

cVidixVideoOut::~cVidixVideoOut()
{
    int err;

    printf("cVidixVideoOut : destructor\n");

    if(vidix_handler) {
        if((err = vdlPlaybackOff(vidix_handler)) != 0) {
           printf("cVidixVideoOut : Can't stop playback: %s\n", strerror(err));
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
                   printf("cVidixVideoOut : Can't put cmap\n");
               }

               free(orig_cmap);
               orig_cmap = NULL;
           }
           break;
       }
    }

    if (osd) free(osd);
    if (fbdev) close(fbdev);
}

#ifdef USE_SUBPLUGINS
/* ---------------------------------------------------------------------------
 */
extern "C" void *
SubPluginCreator(cSetupStore *s)
{
  return new cVidixVideoOut(s);
}
#endif
