/*
 * video.c: A plugin for the Video Disk Recorder
 *
 * See the README file for copyright information and how to reach the author.
 *
 * $Id: video-fb.c,v 1.1 2004/08/01 05:07:04 lucke Exp $
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


static  pthread_mutex_t fb_mutex = PTHREAD_MUTEX_INITIALIZER;
// --- cFrameBuffer --------------------------------------------------------
cFBVideoOut::cFBVideoOut()
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
#if VDRVERSNUM < 10307
    PixelMask = (unsigned char *)malloc(Yres*line_len / ((Bpp+7) / 8) / 8); // where the Video window should be transparent
#endif

    OSDpresent=false;

    printf("[video-fb] init %d x %d (Size: %d Bytes LineLen %d) Bpp: %d\n",fb_vinfo.xres, fb_vinfo.yres, size, line_len, fb_vinfo.bits_per_pixel);
    // clear screens
    printf("[video-fb] Clearing the FB\n");
    unsigned char * fbinit;
    fbinit=fb;
    for (int i = 0; i <Yres*line_len; i++) {
	*fbinit=0;
	fbinit++;
    }
}

void cFBVideoOut::Pause(void) 
{
}


#if VDRVERSNUM >= 10307
void cFBVideoOut::Refresh(cBitmap *Bitmap)
{
  Draw(Bitmap,fb,line_len);
}

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
#if VDRVERSNUM < 10307
    yuv_to_rgb (fb, Py, Pu, Pv, Width, Height, line_len,Ystride,UVstride,Xres,Yres,Bpp, PixelMask);
#else
    yuv_to_rgb (fb, Py, Pu, Pv, Width, Height, line_len,Ystride,UVstride,Xres,Yres,Bpp, NULL);
#endif
  } else {
    yuv_to_rgb (fb, Py, Pu, Pv, Width, Height, line_len,Ystride,UVstride,Xres,Yres,Bpp, NULL);
  }
  pthread_mutex_unlock(&fb_mutex);
}

cFBVideoOut::~cFBVideoOut()
{
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

