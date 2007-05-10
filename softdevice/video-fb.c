/*
 * video.c: A plugin for the Video Disk Recorder
 *
 * See the README file for copyright information and how to reach the author.
 *
 * $Id: video-fb.c,v 1.19 2007/05/10 19:54:44 wachm Exp $
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
cFBVideoOut::cFBVideoOut(cSetupStore *setupStore, cSetupSoftlog *Softlog)
              : cVideoOut(setupStore, Softlog)
{
    char    *fbName = getFBName();

    printf("[video-fb] Initializing Driver\n");

    if ((fbdev = open(fbName, O_RDWR)) == -1) {
        printf("[video-fb] cant open framebuffer %s\n", fbName);
        free(fbName);
        exit(1);
    }
    free(fbName);

    if (ioctl(fbdev, FBIOGET_VSCREENINFO, &fb_vinfo)) {
        printf("[video-fb] Can't get VSCREENINFO\n");
        exit(1);
    }
    if (ioctl(fbdev, FBIOGET_FSCREENINFO, &fb_finfo)) {
        printf("[video-fb] Can't get FSCREENINFO\n");
        exit(1);
    }

    fb_orig_vinfo = fb_vinfo;

    size = fb_finfo.smem_len;
    line_len = fb_finfo.line_length;
    if ((fb = (unsigned char *) mmap(0, size, PROT_READ | PROT_WRITE, MAP_SHARED, fbdev, 0)) == (unsigned char *) -1) {
        printf("[video-fb] Can't mmap\n");
        exit(1);
    }

    // set up picture buffer;
    privBuf.pixel[0]=fb;
    privBuf.stride[0]=line_len;
    if (fb_vinfo.red.length==5 && fb_vinfo.green.length==5
         && fb_vinfo.blue.length==5 ) 
            Bpp=15;
    else Bpp=fb_vinfo.bits_per_pixel;
    switch (Bpp) {
            case 32 : privBuf.format=PIX_FMT_RGBA32;
                      break;
            case 24 : privBuf.format=PIX_FMT_RGB24;
                      break;
            case 16 : 
                      privBuf.format=PIX_FMT_RGB565;
                      break;
            default:
                      privBuf.format=PIX_FMT_RGB555;
    };
    privBuf.max_width=dwidth=fb_vinfo.xres;
    privBuf.max_height=dheight=fb_vinfo.yres;

    switch (fb_finfo.visual) {
       case FB_VISUAL_TRUECOLOR:
           printf("[video-fb] Truecolor FB found\n");
           break;

       case FB_VISUAL_DIRECTCOLOR:

           struct fb_cmap cmap;
           __u16 red[256], green[256], blue[256];

           printf("[video-fb] DirectColor FB found\n");

           orig_cmaplen = 1<<fb_vinfo.green.length;
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
           printf("cFBVideoOut: Unsupported FB(%d). Don't know if it will work.\n",fb_finfo.visual);
    }

    printf("[video-fb] init %d x %d (Size: %zu Bytes LineLen %d) Bpp: %d\n",fb_vinfo.xres, fb_vinfo.yres, size, line_len, fb_vinfo.bits_per_pixel);
    // clear screens
    printf("[video-fb] Clearing the FB\n");

    ClearPicBuffer(&privBuf);

    screenPixelAspect = -1;
}

/* ---------------------------------------------------------------------------
 */
void cFBVideoOut::GetOSDDimension(int &OsdWidth,int &OsdHeight,
                int &xPan, int &yPan) {
  OsdWidth=swidth;
  OsdHeight=sheight;
  xPan = sxoff;
  yPan = syoff;
};

/* ---------------------------------------------------------------------------
 */
void cFBVideoOut::YUV(sPicBuffer *buf)
{

  if (!videoInitialized)
    return;

  pthread_mutex_lock(&fb_mutex);
  if (aspect_changed ||
      cutTop != setupStore->cropTopLines ||
      cutBottom != setupStore->cropBottomLines ||
      cutLeft != setupStore->cropLeftCols ||
      cutRight != setupStore->cropRightCols)
  {
    aspect_changed = 0;
    cutTop = setupStore->cropTopLines;
    cutBottom = setupStore->cropBottomLines;
    cutLeft = setupStore->cropLeftCols;
    cutRight = setupStore->cropRightCols;
    ClearPicBuffer(&privBuf);
  }
  if ( OSDpresent ) {
    CopyScalePicBufAlphaBlend(&privBuf,buf,
        sxoff, syoff,
        swidth, sheight,
        lxoff,  lyoff,
        lwidth, lheight,
        OsdPy,OsdPu,OsdPv,
        OsdPAlphaY,OsdPAlphaUV,OSD_FULL_WIDTH,
        cutTop,cutBottom,cutLeft,cutRight);
  } else {
    CopyScalePicBuf(&privBuf, buf,                   
        sxoff, syoff,
        swidth, sheight,
        lxoff,  lyoff,    
        lwidth, lheight,
        cutTop,cutBottom,cutLeft,cutRight);
  };
  pthread_mutex_unlock(&fb_mutex);
}

/* ---------------------------------------------------------------------------
 */
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
}

#ifdef USE_SUBPLUGINS
/* ---------------------------------------------------------------------------
 */
extern "C" void *
SubPluginCreator(cSetupStore *s, cSetupSoftlog *log)
{
  return new cFBVideoOut(s,log);
}
#endif
