/*
 * video.c: A plugin for the Video Disk Recorder
 *
 * See the README file for copyright information and how to reach the author.
 *
 * $Id: video-fb.c,v 1.12 2006/02/03 22:34:54 wachm Exp $
 *
 * This is a software output driver.
 * It scales the image more or less perfect in sw and put it into the framebuffer
 * This should work with every 16 bit FB (15 bit mode not tested)
 */

#include <sys/mman.h>
#include <sys/ioctl.h>
#include <fcntl.h> 
#include <vdr/plugin.h>
#include "video-fb.h"
#include "utils.h"
#include "setup-softdevice.h"

static  pthread_mutex_t fb_mutex = PTHREAD_MUTEX_INITIALIZER;
// --- cFrameBuffer --------------------------------------------------------
cFBVideoOut::cFBVideoOut(cSetupStore *setupStore)
              : cVideoOut(setupStore)
{
    printf("[video-fb] Initializing Driver\n");

    if ((fbdev = open(FBDEV, O_RDWR)) == -1) {
        printf("[video-fb] cant open framebuffer %s\n", FBDEV);
        exit(1);
    }

    if (ioctl(fbdev, FBIOGET_VSCREENINFO, &fb_vinfo)) {
        printf("[video-fb] Can't get VSCREENINFO\n");
        exit(1);
    }
    if (ioctl(fbdev, FBIOGET_FSCREENINFO, &fb_finfo)) {
        printf("[video-fb] Can't get FSCREENINFO\n");
        exit(1);
    }

    fb_orig_vinfo = fb_vinfo;

    switch (fb_finfo.visual) {

       case FB_VISUAL_TRUECOLOR:
           printf("[video-fb] Truecolor FB found\n");
           break;

       case FB_VISUAL_DIRECTCOLOR:

           struct fb_cmap cmap;
           __u16 red[256], green[256], blue[256];

           printf("[video-fb] DirectColor FB found\n");

           orig_cmaplen = 32;
           orig_cmap = (__u16 *) malloc ( 3 * orig_cmaplen * sizeof(*orig_cmap) );

           if ( orig_cmap == NULL ) {
               printf("cFBVideoOut: Can't alloc memory for cmap\n");
               exit(1);
           }
           cmap.start  = 0;
           cmap.len    = orig_cmaplen;
           cmap.red    = &orig_cmap[0*orig_cmaplen];
           cmap.green  = &orig_cmap[1*orig_cmaplen];
           cmap.blue   = &orig_cmap[2*orig_cmaplen];
           cmap.transp = NULL;

           if ( ioctl(fbdev, FBIOGETCMAP, &cmap)) {
               printf("cFBVideoOut: Can't get cmap\n");
               exit(-1);
           }

           for ( int i=0; i < orig_cmaplen; ++i ) {
               red[i]   = (65535/(orig_cmaplen+1))*i;
               green[i] = (65535/(orig_cmaplen+1))*i;
               blue[i] =  (65535/(orig_cmaplen+1))*i;
               //(i<<8)|i;
           }

           cmap.start  = 0;
           cmap.len    = orig_cmaplen;
           cmap.red    = red;
           cmap.green  = green;
           cmap.blue   = blue;
           cmap.transp = NULL;

           if ( ioctl(fbdev, FBIOPUTCMAP, &cmap)) {
               printf("cVidixVideoOut: Can't put cmap\n");
               exit(-1);
           }

           break;

       default:
           printf("cFBVideoOut: Unsupported FB. Don't know if it will work.\n");
    }


    // currently we support only 16 bit FB's

    size = fb_finfo.smem_len;
    line_len = fb_finfo.line_length;
    if ((fb = (unsigned char *) mmap(0, size, PROT_READ | PROT_WRITE, MAP_SHARED, fbdev, 0)) == (unsigned char *) -1) {
        printf("[video-fb] Can't mmap\n");
        exit(1);
    }

    Xres=fb_vinfo.xres;
    Yres=fb_vinfo.yres;
    Bpp = fb_vinfo.bits_per_pixel;
    if (Bpp != 16 && Bpp != 15) {
        printf ("[video-fb] In software-mode only 15/16 bit Framebuffer supported\n");
        exit(1);
    }

    // set rgb unpack method
    mmx_unpack=mmx_unpack_16rgb;
    if (fb_vinfo.red.length==5 && fb_vinfo.green.length==5 
         && fb_vinfo.blue.length==5 ) {
      mmx_unpack=mmx_unpack_15rgb;
      printf("[video-fb] Using mmx_unpack_15rgb\n");
    };

    int OsdYres=Yres>OSD_FULL_HEIGHT?Yres:OSD_FULL_HEIGHT;
#if VDRVERSNUM < 10307
    int OsdXres=Xres>OSD_FULL_WIDTH?Xres:OSD_FULL_WIDTH;
    PixelMask = (unsigned char *)malloc(OsdXres*OsdYres/8 ); // where the Video window should be transparent
#else
    PixelMask = (unsigned char *)malloc(OsdYres*line_len / ((Bpp+7) / 8) / 8); // where the Video window should be transparent
#endif

    OSDpresent=false;

    printf("[video-fb] init %d x %d (Size: %zu Bytes LineLen %d) Bpp: %d\n",fb_vinfo.xres, fb_vinfo.yres, size, line_len, fb_vinfo.bits_per_pixel);
    // clear screens
    printf("[video-fb] Clearing the FB\n");
    unsigned char * fbinit;
    fbinit=fb;
    for (int i = 0; i <Yres*line_len; i++) {
        *fbinit=0;
        fbinit++;
    }
    screenPixelAspect = -1;
}

void cFBVideoOut::Pause(void) 
{
}

#if VDRVERSNUM >= 10307
/* ---------------------------------------------------------------------------
 */
void cFBVideoOut::OpenOSD()
{
  cVideoOut::OpenOSD();
  //OSDxOfs = X & ~7;
  //OSDyOfs = Y & ~1;
}

/* ---------------------------------------------------------------------------
 */
void cFBVideoOut::ClearOSD()
{
  cVideoOut::ClearOSD();
  if (PixelMask)
    memset(PixelMask, 0, Xres * Yres/8);
};

/* ---------------------------------------------------------------------------
 */
void cFBVideoOut::GetOSDDimension(int &OsdWidth,int &OsdHeight) {
   switch (current_osdMode) {
      case OSDMODE_PSEUDO :
                OsdWidth=Xres;
                OsdHeight=Yres;
             break;
    }
}

void cFBVideoOut::GetLockOsdSurface(uint8_t *&osd, int &stride, 
                  bool *&dirtyLines) {
  pthread_mutex_lock(&fb_mutex);
  osd=fb; 
  stride=line_len;
  dirtyLines=NULL;
};

void cFBVideoOut::CommitUnlockOsdSurface() {      
  pthread_mutex_unlock(&fb_mutex);
  cVideoOut::CommitUnlockOsdSurface();
};

#else

void cFBVideoOut::Refresh()
{
  // refreshes the OSD screen
  osdMutex.Lock();
  if (OSDpresent)
  {
    pthread_mutex_lock(&fb_mutex);
    for (int i = 0; i <(Yres*line_len / ((Bpp + 7) / 8) / 8); i++)
    {
      PixelMask[i]= 0;
    }
    for (int i = 0; i < MAXNUMWINDOWS; i++)
    {
      if (layer[i] && layer[i]->visible)
        layer[i]->Draw(fb, line_len, PixelMask);
    }
    pthread_mutex_unlock(&fb_mutex);
  }
  osdMutex.Unlock();
}
#endif

void cFBVideoOut::YUV(uint8_t *Py, uint8_t *Pu, uint8_t *Pv, int Width, int Height, int Ystride, int UVstride)
{
  pthread_mutex_lock(&fb_mutex);
  if (OSDpresent) {
    yuv_to_rgb (fb, Py, Pu, Pv,
                Width, Height, line_len,
                Ystride,UVstride,
                Xres,Yres,
                Bpp, PixelMask, setupStore->deintMethod);
  } else {
    yuv_to_rgb (fb, Py, Pu, Pv,
                Width, Height, line_len,
                Ystride,UVstride,
                Xres,Yres,
                Bpp, NULL, setupStore->deintMethod);
  }
  pthread_mutex_unlock(&fb_mutex);
}

cFBVideoOut::~cFBVideoOut()
{
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
                   printf("cFBVideoOut : Can't put cmap\n");
               }

               free(orig_cmap);
               orig_cmap = NULL;
           }
           break;
       }
  }
  if (fbdev)
    close(fbdev);
#if VDRVERSNUM < 10307
  osdMutex.Lock();
  for (int i = 0; i < MAXNUMWINDOWS; i++)
  {
    if (layer[i])
      delete(layer[i]);
  }
  osdMutex.Unlock();
#endif
}

#ifdef USE_SUBPLUGINS
/* ---------------------------------------------------------------------------
 */
extern "C" void *
SubPluginCreator(cSetupStore *s)
{
  return new cFBVideoOut(s);
}
#endif
