/*
 * video-vidix.c: A plugin for the Video Disk Recorder
 *
 * See the README file for copyright information and how to reach the author.
 *
 * $Id: video-vidix.c,v 1.4 2004/12/21 05:55:42 lucke Exp $
 */

#include <sys/mman.h>
#include <sys/ioctl.h>
#include <fcntl.h> 
#include <vdr/plugin.h>
#include "video-vidix.h"
#include "utils.h"
#include "setup-softdevice.h"

cVidixVideoOut::cVidixVideoOut()
{
    int err;
    if ((fbdev = open(FBDEV, O_RDWR)) == -1) {
	printf("cVidixVideoOut: Can't open framebuffer\n");
        exit(1);
    }
    
    if (ioctl(fbdev, FBIOGET_VSCREENINFO, &fb_vinfo)) {
	printf("cVidixVideoOut: Can't get VSCREENINFO\n");
        exit(1);
    }
    if (ioctl(fbdev, FBIOGET_FSCREENINFO, &fb_finfo)) {
        printf("cVidixVideoOut: Can't get FSCREENINFO\n");
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
               printf("cVidixVideoOut: Can't alloc memory for cmap\n");
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
    sxoff = syoff = lxoff = lyoff = 0;

    printf("cVidixVideoOut: xres = %d yres= %d \n", fb_vinfo.xres, fb_vinfo.yres);
    printf("cVidixVideoOut: line length = %d \n", fb_line_len);
    printf("cVidixVideoOut: bpp = %d\n", fb_vinfo.bits_per_pixel);

    fb = (uint8_t *) mmap(0, fb_finfo.smem_len, PROT_READ | PROT_WRITE, MAP_SHARED, fbdev, 0);

    if ( fb == (uint8_t *) -1 ) {
       printf("cVidixVideoOut: Can't mmap framebuffer memory\n");
       exit(1);
     }

    osd = (uint8_t *) malloc(fb_line_len * Yres);

    if ( osd == NULL ) {
       printf("cVidixVideoOut: Can't alloc osd memory\n");
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
       printf("cVidixVideoOut: Couldn't find working VIDIX driver\n");
       exit(1);
    }

    if( (err = vdlGetCapability(vidix_handler,&vidix_cap)) != 0)
    {
       printf("cVidixVideoOut: Couldn't get capability: %s\n", strerror(err) );
       exit(1);
    }

    OSDpseudo_alpha = true;

    if (setupStore.pixelFormat == 0)
      vidix_fourcc.fourcc = IMGFMT_I420;
    else if(setupStore.pixelFormat == 1)
      vidix_fourcc.fourcc = IMGFMT_YV12;

    currentPixelFormat = setupStore.pixelFormat;
    screenPixelAspect = -1;

    vdlQueryFourcc(vidix_handler, &vidix_fourcc);

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
    vidix_play.num_frames   = 1;


    if( vidix_fourcc.flags & VID_CAP_COLORKEY )
    {
       vdlGetGrKeys(vidix_handler, &gr_key);

       gr_key.key_op = KEYS_PUT;

       gr_key.ckey.op = CKEY_TRUE;
       gr_key.ckey.red = gr_key.ckey.green = gr_key.ckey.blue = 0;

       vdlSetGrKeys(vidix_handler, &gr_key);
    }
}

void cVidixVideoOut::Pause(void)
{
}

void cVidixVideoOut::YUV(uint8_t *Py, uint8_t *Pu, uint8_t *Pv, int Width, int Height, int Ystride, int UVstride)
{
    int err;
    uint8_t *dst;
    uint32_t apitch;
    int hi, wi;

    if (aspect_changed || currentPixelFormat != setupStore.pixelFormat)
    {
       printf("cVidixVideoOut: Video changed format to %dx%d\n", Width, Height);

       if((Xres > Width || Yres > Height) && (vidix_cap.flags & FLAG_UPSCALER) != FLAG_UPSCALER)
       {
           printf("cVidixVideoOut : vidix driver can't upscale image (%dx%d -> %dx%d)\n", Width, Height, Xres, Yres);
           exit(1);
       }

       if((Xres < Width || Yres < Height) && (vidix_cap.flags & FLAG_DOWNSCALER) != FLAG_DOWNSCALER)
       {
           printf("cVidixVideoOut : vidix driver can't downscale image (%dx%d -> %dx%d)\n", Width, Height, Xres, Yres);
           exit(1);
       }

       if (currentPixelFormat != setupStore.pixelFormat)
       {
         if (setupStore.pixelFormat == 0)
           vidix_play.fourcc = IMGFMT_I420;
         else if(setupStore.pixelFormat == 1)
           vidix_play.fourcc = IMGFMT_YV12;

         currentPixelFormat = setupStore.pixelFormat;
       }

       vidix_play.src.w  = swidth;
       vidix_play.src.h  = sheight;

       vidix_play.dest.x = lxoff;
       vidix_play.dest.y = lyoff;
       vidix_play.dest.w = lwidth;
       vidix_play.dest.h = lheight;

       vidix_play.src.pitch.y=Ystride;
       vidix_play.src.pitch.u=UVstride;
       vidix_play.src.pitch.v=UVstride;

       if((err = vdlPlaybackOff(vidix_handler)) != 0) {
           printf("cVidixVideoOut : Can't stop playback: %s\n", strerror(err));
           exit(1);
       }

       if((err = vdlConfigPlayback(vidix_handler, &vidix_play)) != 0) {
           printf("cVidixVideoOut : Can't configure playback: %s\n", strerror(err));
           exit(1);
       }

       if((err = vdlPlaybackOn(vidix_handler)) != 0) {
           printf("cVidixVideoOut : Can't start playback: %s\n", strerror(err));
           exit(1);
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
    }

    // Plane Y
    dst = (uint8_t *) vidix_play.dga_addr + vidix_play.offsets[next_frame] + vidix_play.offset.y;

    Py += (Ystride * syoff);
    Pv += (UVstride * syoff/2);
    Pu += (UVstride * syoff/2);

    for(hi=0; hi < sheight; hi++){
       memcpy(dst, Py+sxoff, swidth);
        Py  += Ystride;
        dst += dstrides.y;
    }

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

    vdlPlaybackFrameSelect(vidix_handler, next_frame);
    next_frame = (next_frame+1) % vidix_play.num_frames;
}

#if VDRVERSNUM >= 10307

void cVidixVideoOut::Refresh(cBitmap *Bitmap)
{
  Draw(Bitmap,fb,fb_line_len);
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
