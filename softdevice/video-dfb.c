/*
 * video.c: A plugin for the Video Disk Recorder
 *
 * See the README file for copyright information and how to reach the author.
 *
 * $Id: video-dfb.c,v 1.9 2004/11/09 21:48:53 lucke Exp $
 */

#include <sys/mman.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <vdr/plugin.h>
#include "video-dfb.h"
#include "utils.h"
#include "setup-softdevice.h"

#define HAVE_SetSourceLocation 0

//#define COLORKEY 17,8,79
#define COLORKEY 0,0,0

/* macro for a safe call to DirectFB functions */
#define DFBCHECK(x...) \
     {                                                               \
           DFBResult  err = x;                                       \
           if (err != DFB_OK) {                                      \
              fprintf( stderr, "%s <%d>:\n\t", __FILE__, __LINE__ ); \
              DirectFBErrorFatal( #x, err );                         \
           }                                                         \
     }

static int              events_not_done = 0;

// --- cDFBRemote ---------------------------------------------------
/* ---------------------------------------------------------------------------
 */
cDFBRemote::cDFBRemote(const char *Name, cDFBVideoOut *vout) : cRemote(Name)
{
  video_out = vout;
}

/* ---------------------------------------------------------------------------
 */
cDFBRemote::~cDFBRemote()
{
  active = false;
  Cancel(2);
}

/* ---------------------------------------------------------------------------
 */
void cDFBRemote::PutKey(DFBInputDeviceKeySymbol key)
{
  Put ((int) key);
}

/* ---------------------------------------------------------------------------
 */
void cDFBRemote::Action(void)
{
  dsyslog("DFB remote control thread started (pid=%d)", getpid());
  active = true;
  while (active)
  {
    usleep (25000);
    if (events_not_done > 1) {
      video_out->ProcessEvents ();
      video_out->ShowOSD ();
      events_not_done = 0;
    } else {
      events_not_done++;
    }
  }
  dsyslog("DFB remote control thread ended (pid=%d)", getpid());
}

/* ---------------------------------------------------------------------------
 */
void cDFBRemote::DFBRemoteStart(void)
{
  Start();
}

// --- cDFBVideoOut -------------------------------------------------
IDirectFB *DFBWrapper;

/* ---------------------------------------------------------------------------
 */
static DFBEnumerationResult EnumCallBack( unsigned int id, DFBDisplayLayerDescription desc, void *data )
{
    IDirectFBDisplayLayer **layer = (IDirectFBDisplayLayer **)data;

  fprintf(stderr,"Layer %d %s",id, desc.name);

  fprintf(stderr,"  Type: ");
  if (desc.type & DLTF_BACKGROUND) fprintf(stderr,"background " );
  if (desc.type & DLTF_GRAPHICS) fprintf(stderr,"graphics " );
  if (desc.type & DLTF_STILL_PICTURE) fprintf(stderr,"picture " );
  if (desc.type & DLTF_VIDEO) fprintf(stderr,"video " );
  fprintf(stderr,"\n" );

  fprintf(stderr,"  Caps: " );
  if (desc.caps & DLCAPS_ALPHACHANNEL) fprintf(stderr,"alphachannel " );
  if (desc.caps & DLCAPS_BRIGHTNESS) fprintf(stderr,"brightness " );
  if (desc.caps & DLCAPS_CONTRAST) fprintf(stderr,"contrast " );
  if (desc.caps & DLCAPS_DEINTERLACING) fprintf(stderr,"deinterlacing " );
  if (desc.caps & DLCAPS_DST_COLORKEY) fprintf(stderr,"dst_colorkey " );
  if (desc.caps & DLCAPS_FLICKER_FILTERING) fprintf(stderr,"flicker_filtering " );
  if (desc.caps & DLCAPS_HUE) fprintf(stderr,"hue " );
  if (desc.caps & DLCAPS_LEVELS) fprintf(stderr,"levels " );
  if (desc.caps & DLCAPS_OPACITY) fprintf(stderr,"opacity " );
  if (desc.caps & DLCAPS_SATURATION) fprintf(stderr,"saturation " );
  if (desc.caps & DLCAPS_SCREEN_LOCATION) fprintf(stderr,"screen_location " );
  if (desc.caps & DLCAPS_SRC_COLORKEY) fprintf(stderr,"src_colorkey " );
  if (desc.caps & DLCAPS_SURFACE) fprintf(stderr,"surface " );
  fprintf(stderr,"\n");

  /* We take the first layer not being the primary */
  if (id != DLID_PRIMARY && desc.caps & DLCAPS_SURFACE && !*layer) {
    fprintf(stderr,"  This is our videoLayer\n");
    try {
      *layer = DFBWrapper->GetDisplayLayer(id);
    } catch (DFBException *ex) {
      fprintf(stderr,"Caught: %s", ex);
      return DFENUM_CANCEL;
    }
  }

  return DFENUM_OK;
}

/* ---------------------------------------------------------------------------
 */
static void reportCardInfo (IDirectFB *dfb)
{
    DFBCardCapabilities           caps;

  dfb->GetCardCapabilities(&caps);

  fprintf(stderr,"[dfb] RAM: %d bytes\n",caps.video_memory);

  fprintf(stderr,"[dfb] Accellerated Functions: ");
  if (caps.acceleration_mask == DFXL_NONE) fprintf(stderr,"none ");
  if (caps.acceleration_mask & DFXL_FILLRECTANGLE) fprintf(stderr,"FillRectange ");
  if (caps.acceleration_mask & DFXL_DRAWRECTANGLE) fprintf(stderr,"DrawRectange ");
  if (caps.acceleration_mask & DFXL_DRAWLINE) fprintf(stderr,"DrawLine ");
  if (caps.acceleration_mask & DFXL_FILLTRIANGLE) fprintf(stderr,"FillTriangle ");
  if (caps.acceleration_mask & DFXL_BLIT) fprintf(stderr,"Blit ");
  if (caps.acceleration_mask & DFXL_STRETCHBLIT) fprintf(stderr,"StretchBlit ");
  if (caps.acceleration_mask & DFXL_ALL) fprintf(stderr,"All ");
  fprintf(stderr,"\n");

  fprintf(stderr,"[dfb] Drawing Flags: ");
  if (caps.drawing_flags == DSDRAW_NOFX ) fprintf(stderr,"none ");
  if (caps.drawing_flags & DSDRAW_BLEND ) fprintf(stderr,"Blend ");
  if (caps.drawing_flags & DSDRAW_DST_COLORKEY ) fprintf(stderr,"Colorkeying ");
  if (caps.drawing_flags & DSDRAW_SRC_PREMULTIPLY ) fprintf(stderr,"Src.premultiply ");
  if (caps.drawing_flags & DSDRAW_DST_PREMULTIPLY ) fprintf(stderr,"Dst.premultiply ");
  if (caps.drawing_flags & DSDRAW_DEMULTIPLY ) fprintf(stderr,"Demultiply ");
  if (caps.drawing_flags & DSDRAW_XOR ) fprintf(stderr,"Xor ");
  fprintf(stderr,"\n");

  fprintf(stderr,"[dfb] Surface Blitting Flags: ");
  if (caps.blitting_flags == DSBLIT_NOFX ) fprintf(stderr,"none ");
  if (caps.blitting_flags & DSBLIT_BLEND_ALPHACHANNEL ) fprintf(stderr,"BlendAlpha ");
  if (caps.blitting_flags & DSBLIT_BLEND_COLORALPHA ) fprintf(stderr,"BlendColorAlpha ");
  if (caps.blitting_flags & DSBLIT_COLORIZE ) fprintf(stderr,"Colorize ");
  if (caps.blitting_flags & DSBLIT_SRC_COLORKEY ) fprintf(stderr,"SrcColorkey ");
  if (caps.blitting_flags & DSBLIT_DST_COLORKEY ) fprintf(stderr,"DstColorkey ");
  if (caps.blitting_flags & DSBLIT_SRC_PREMULTIPLY ) fprintf(stderr,"SrcPremultiply ");
  if (caps.blitting_flags & DSBLIT_DST_PREMULTIPLY ) fprintf(stderr,"DstPremultiply ");
  if (caps.blitting_flags & DSBLIT_DEMULTIPLY ) fprintf(stderr,"Demultiply ");
  if (caps.blitting_flags & DSBLIT_DEINTERLACE ) fprintf(stderr,"Deinterlace ");
  fprintf(stderr,"\n");
}

/* ---------------------------------------------------------------------------
 */
static void reportSurfaceCapabilities (char *name, IDirectFBSurface *surf)
{
    DFBSurfaceCapabilities        scaps;

  scaps = surf->GetCapabilities();
  fprintf(stderr,"%s:\n", name);

  if (scaps & DSCAPS_NONE) fprintf(stderr," - none\n");
  if (scaps & DSCAPS_PRIMARY) fprintf(stderr," - primary\n");
  if (scaps & DSCAPS_SYSTEMONLY) fprintf(stderr," - systemonly\n");
  if (scaps & DSCAPS_VIDEOONLY) fprintf(stderr," - videoonly\n");
  if (scaps & DSCAPS_FLIPPING) fprintf(stderr," - flipping\n");
  if (scaps & DSCAPS_SUBSURFACE) fprintf(stderr," - subsurface\n");
  if (scaps & DSCAPS_INTERLACED) fprintf(stderr," - interlaced\n");
  if (scaps & DSCAPS_SEPARATED) fprintf(stderr," - separated\n");
  if (scaps & DSCAPS_STATIC_ALLOC) fprintf(stderr," - static alloc\n");
  if (scaps & DSCAPS_TRIPLE) fprintf(stderr," - triple buffered\n");
}

/* ---------------------------------------------------------------------------
 */
// List Video Modes
static DFBEnumerationResult EnumVideoModeCallback(int x, int y, int bpp, void *data)
{
  fprintf(stderr,"%dx%d@%d ",x,y,bpp);
  return DFENUM_OK;
}

/* ---------------------------------------------------------------------------
 */
cDFBVideoOut::cDFBVideoOut()
{
    DFBDisplayLayerDescription    desc;

  fprintf(stderr,"[dfb] init\n");
  /* --------------------------------------------------------------------------
   * default settings: source, destination and logical widht/height
   * are set to our well known dimensions (except fwidth+fheight).
   */
  swidth  = fwidth  = 720;
  sheight = fheight = 576;

  sxoff = syoff = lxoff = lyoff = 0;

  currentPixelFormat = setupStore.pixelFormat;

  OSDpresent = false;

  DirectFB::Init();
  dfb = DirectFB::Create();
  dfb->SetCooperativeLevel(DFSCL_FULLSCREEN);
  reportCardInfo (dfb);

  fprintf(stderr,"[dfb] Supported video Modes are: ");
  dfb->EnumVideoModes(EnumVideoModeCallback, NULL);
  fprintf(stderr,"\n");

  /* --------------------------------------------------------------------------
   * Thats for dfb access during layer enumeration
   */
  DFBWrapper=dfb;
  fprintf(stderr,"[dfb] Enumeratig display Layers\n");

  osdLayer=dfb->GetDisplayLayer(DLID_PRIMARY);
  if (!osdLayer) {
    fprintf(stderr,"[dfb] no OSD layer exiting\n");
    exit(1);
  }

  videoLayer = NULL;
  dfb->EnumDisplayLayers(EnumCallBack, &videoLayer);
  if (videoLayer) {
    videoLayer->SetCooperativeLevel(DLSCL_ADMINISTRATIVE);
    //videoLayer->SetDstColorKey(COLORKEY); // no need to do that now

    videoSurface=videoLayer->GetSurface();
    videoSurface->Clear(COLORKEY,0); //clear and
    videoSurface->Flip(); // Flip the field
    videoSurface->Clear(COLORKEY,0); //clear and
    videoSurface->Flip(); // Flip the field
  } else {
    fprintf(stderr,"could not find suitable videolayer\n");
    exit(1);
  }

  scrDsc.flags = (DFBSurfaceDescriptionFlags) (DSDESC_CAPS |
                                               DSDESC_PIXELFORMAT);
  scrDsc.caps = DSCAPS_NONE;
  DFB_ADD_SURFACE_CAPS(scrDsc.caps, DSCAPS_FLIPPING);
  DFB_ADD_SURFACE_CAPS(scrDsc.caps, DSCAPS_PRIMARY);
  DFB_ADD_SURFACE_CAPS(scrDsc.caps, DSCAPS_VIDEOONLY);
  DFB_ADD_SURFACE_CAPS(scrDsc.caps, DSCAPS_DOUBLE);
  scrDsc.pixelformat = DSPF_ARGB;

  scrSurface   = dfb->CreateSurface (scrDsc);

  if (scrSurface)
  {
      DFBSurfacePixelFormat fmt;

    scrSurface->GetSize(&Xres,&Yres);
    fprintf (stderr,
             "[dfb] width = %d, height = %d\n",
             Xres, Yres);
    lwidth  = dwidth  = Xres;
    lheight = dheight = Yres;

    fmt = scrSurface->GetPixelFormat();
    fprintf (stderr,
             "[dfb] got fmt = 0x%08x bpp = %d\n",
             fmt, DFB_BITS_PER_PIXEL(fmt));
    Bpp = DFB_BITS_PER_PIXEL(fmt);
#if 1
    if (Xres > 768)
      Xres = 768;
    if (Yres > 576)
      Yres = 576;
#endif

    /* ------------------------------------------------------------------------
     * clear screen surface at startup
     */
    scrSurface->Clear(0,0,0,0);
    scrSurface->Flip();
    scrSurface->Clear(0,0,0,0);

    osdDsc.flags = (DFBSurfaceDescriptionFlags) (DSDESC_CAPS |
                                                 DSDESC_WIDTH |
                                                 DSDESC_HEIGHT |
                                                 DSDESC_PIXELFORMAT);
    osdDsc.caps = DSCAPS_NONE;
    DFB_ADD_SURFACE_CAPS(osdDsc.caps, DSCAPS_FLIPPING);
    DFB_ADD_SURFACE_CAPS(osdDsc.caps, DSCAPS_VIDEOONLY);
    osdDsc.width  = Xres;
    osdDsc.height = Yres;
    osdDsc.pixelformat = DSPF_ARGB;

    useStretchBlit = false;
    OSDpseudo_alpha = true;
    if (setupStore.pixelFormat == 0)
      vidDsc.pixelformat = DSPF_I420;
    else if (setupStore.pixelFormat == 1)
      vidDsc.pixelformat = DSPF_YV12;
    else if (setupStore.pixelFormat == 2)
    {
      vidDsc.pixelformat = DSPF_YUY2;
      useStretchBlit = true;
      OSDpseudo_alpha = false;
    }

    osdSurface   = dfb->CreateSurface (osdDsc);
    osdSurface->Clear(0,0,0,0); //clear and
    osdSurface->Flip(); // Flip the field
    osdSurface->Clear(0,0,0,0); //clear and

    desc = osdLayer->GetDescription();
    fprintf(stderr,
            "[dfb] Using this layer for OSD: (%s - [%dx%d])\n",
            desc.name, osdSurface->GetWidth(), osdSurface->GetHeight());

    reportSurfaceCapabilities ("osdSurface:", osdSurface);

    vidDsc.flags = (DFBSurfaceDescriptionFlags) (DSDESC_CAPS |
                                                 DSDESC_WIDTH |
                                                 DSDESC_HEIGHT |
                                                 DSDESC_PIXELFORMAT);
    vidDsc.caps   = DSCAPS_VIDEOONLY;
    vidDsc.width  = fwidth;
    vidDsc.height = fheight;


    if (useStretchBlit)
    {
      videoSurface = dfb->CreateSurface (vidDsc);
      videoSurface->Clear(0,0,0,0); //clear and
    }
    else
    {
      videoSurface=videoLayer->GetSurface();
      videoSurface->Clear(COLORKEY,0); //clear and
      videoSurface->Flip(); // Flip the field
      videoSurface->Clear(COLORKEY,0); //clear and
      videoSurface->Flip(); // Flip the field
    }

    reportSurfaceCapabilities ("videoSurface:", videoSurface);

    fprintf(stderr,"[dfb] Configuring CooperativeLevel for Overlay\n");
    videoLayer->SetCooperativeLevel(DLSCL_ADMINISTRATIVE);

    fprintf(stderr,"[dfb] Configuring ColorKeying\n");
    videoLayer->SetDstColorKey(COLORKEY);

    fprintf(stderr,"[dfb] Configuring CooperativeLevel for OSD\n");
    osdLayer->SetCooperativeLevel(DLSCL_ADMINISTRATIVE);

    desc = osdLayer->GetDescription();
    fprintf(stderr,"[dfb] Using this layer for OSD: %s\n", desc.name);

    desc = videoLayer->GetDescription();
    fprintf(stderr,"[dfb] Using this layer for Video out: %s\n", desc.name);

  }
  
  /* create an event buffer with all devices attached */
  events = dfb->CreateInputEventBuffer( DICAPS_ALL );
}

/* ---------------------------------------------------------------------------
 */
bool cDFBVideoOut::Initialize (void)
{
  /* -------------------------------------------------------------------------
   * force reconfiguring of video layer so cle266 color key works
   */
  aspect_changed = 1;
  SetParams ();
  aspect_changed = 0;
  /* -------------------------------------------------------------------------
   * get up our remote running
   */
  if (!dfbRemote)
  {
    dfbRemote = new cDFBRemote ("softdevice-dfb", this);
    dfbRemote->DFBRemoteStart();
  }
  return true;
}

/* ---------------------------------------------------------------------------
 */
void cDFBVideoOut::ProcessEvents ()
{
    DFBInputEvent event;

  while (events->GetEvent( DFB_EVENT(&event) ))
  {
    switch (event.type)
    {
      case DIET_KEYPRESS:
        if (dfbRemote)
        {
          switch (event.key_symbol)
          {
            case DIKS_SHIFT: case DIKS_CONTROL: case DIKS_ALT:
            case DIKS_ALTGR: case DIKS_META: case DIKS_SUPER: case DIKS_HYPER:
              break;
            default:
              dfbRemote->PutKey(event.key_symbol);
              break;
          }
        }
        break;
      default:
        break;
    }
  }
}

/* ---------------------------------------------------------------------------
 */
void cDFBVideoOut::SetParams()
{
    DFBDisplayLayerConfig       dlc;
    DFBDisplayLayerDescription  desc;

  if (videoLayer || useStretchBlit)
  {
    if (!videoSurface ||
        aspect_changed ||
        currentPixelFormat != setupStore.pixelFormat)
    {
      fprintf(stderr,
              "[dfb] (re)configuring Videolayer to %d x %d (%dx%d)\n",
              fwidth,fheight,swidth,sheight);
      dlc.flags   = (DFBDisplayLayerConfigFlags)(DLCONF_WIDTH | DLCONF_HEIGHT | DLCONF_PIXELFORMAT | DLCONF_OPTIONS);

      useStretchBlit = false;
      OSDpseudo_alpha = true;
      if (setupStore.pixelFormat == 0)
        dlc.pixelformat = DSPF_I420;
      else if (setupStore.pixelFormat == 1)
        dlc.pixelformat = DSPF_YV12;
      else if (setupStore.pixelFormat == 2)
      {
        dlc.pixelformat = DSPF_YUY2;
        useStretchBlit = true;
        OSDpseudo_alpha = false;
      }

#if HAVE_SetSourceLocation
      /* ----------------------------------------------------------------------
       * this should be the settings if SetSourceRectangle is working for
       * all drivers.
       * SetSourceRectangle() moved, after layer is reconfigured
       */
      dlc.width   = fwidth;
      dlc.height  = fheight;
#else
      /* ----------------------------------------------------------------------
       * for now we have to do another trick
       */
      if (useStretchBlit)
      {
        dlc.width   = fwidth;
        dlc.height  = fheight;
      }
      else
      {
        dlc.width   = swidth;
        dlc.height  = sheight;
      }
#endif

      vidDsc.flags = (DFBSurfaceDescriptionFlags) (DSDESC_CAPS |
                                                 DSDESC_WIDTH |
                                                 DSDESC_HEIGHT |
                                                 DSDESC_PIXELFORMAT);
      vidDsc.caps        = DSCAPS_VIDEOONLY;
      vidDsc.width       = dlc.width;
      vidDsc.height      = dlc.height;
      vidDsc.pixelformat = dlc.pixelformat;

      currentPixelFormat = setupStore.pixelFormat;

      pixelformat = dlc.pixelformat;

      if (!useStretchBlit)
      {
        desc = videoLayer->GetDescription();
        if (videoSurface)
          videoSurface->Release();
        videoSurface = NULL;

        /* --------------------------------------------------------------------
         * set the default options to none
         */
        dlc.options = (DFBDisplayLayerOptions)( DLOP_NONE );

        if (desc.caps & DLCAPS_DEINTERLACING)
        {
//        fprintf (stderr, "deinterlacing\n");
          /* ------------------------------------------------------------------
           * if deinterlacing is supported we'll use that
           */
          dlc.options = (DFBDisplayLayerOptions)( DLOP_DEINTERLACING );
        }

        if (desc.caps & DLCAPS_ALPHACHANNEL)
        {
          /* ------------------------------------------------------------------
           * use alpha channel if it is supported and disable pseudo alpha mode
           */
          dlc.options = (DFBDisplayLayerOptions)((int)dlc.options|DLOP_ALPHACHANNEL);
          OSDpseudo_alpha = false;
        }
        else
        {
//          fprintf (stderr, "no alpha channel\n");
          if (desc.caps & DLCAPS_DST_COLORKEY)
          {
//            fprintf (stderr, "dest color key\n");
            /* ----------------------------------------------------------------
             * no alpha channel but destination color keying is supported
             */
            dlc.options = (DFBDisplayLayerOptions)((int)dlc.options|DLOP_DST_COLORKEY);
          }
        }

        /* --------------------------------------------------------------------
         * OK, try to set the video layer configuration
         */
        try
        {
          videoLayer->SetConfiguration(dlc);
        }
        catch (DFBException *ex)
        {
          fprintf (stderr,"Caught: action=%s, result=%s\n",
                   ex->GetAction(), ex->GetResult());
          exit(1);
        }

#if HAVE_SetSourceLocation
        try
        {
          videoLayer->SetSourceRectangle (sxoff, syoff, swidth, sheight);
        }
        catch (DFBException *ex)
        {
          fprintf (stderr,"Caught: action=%s, result=%s\n",
                   ex->GetAction(), ex->GetResult());
        }
#endif

        if (desc.caps & DLCAPS_SCREEN_LOCATION)
        {
          videoLayer->SetScreenLocation((float) lxoff / (float) dwidth,
                                        (float) lyoff / (float) dheight,
                                        (float) lwidth / (float) dwidth,
                                        (float) lheight / (float) dheight);
        }
        else
        {
          fprintf(stderr,"Can't configure ScreenLocation. Hope it is Fullscreen\n");
        }
        /* --------------------------------------------------------------------
         * set colorkey now. some driver accepct that _after_ configuration
         * has been set ! But check that color keying is supported.
         */
        if (desc.caps & DLCAPS_DST_COLORKEY)
          videoLayer->SetDstColorKey(COLORKEY);

        videoSurface=videoLayer->GetSurface();
        videoSurface->Clear(COLORKEY,0xff); //clear and
      }
      else
      {
        fprintf (stderr, "[dfb] creating new surface\n");
        if (videoSurface) {
          /* ------------------------------------------------------------------
           * clear previous used surface. there where some troubles when we
           * started in I420 mode, switched to YUY2 and video format changed
           * from 4:3 to 16:9 .
           */
          videoSurface->Clear(COLORKEY,0); //clear and
          videoSurface->Release();
        }
        videoSurface=NULL;
        videoSurface=dfb->CreateSurface(vidDsc);
      }

      fprintf(stderr,"[dfb] (re)configured 0x%08x\n",pixelformat);
    }
  }
  else
  {
    fprintf(stderr,"No Videolayer avaiable. Exiting...\n");
    exit(1);
  }
}

/* ---------------------------------------------------------------------------
 */
void cDFBVideoOut::Pause(void)
{
}

#if VDRVERSNUM >= 10307

/* ---------------------------------------------------------------------------
 */
void cDFBVideoOut::Refresh(cBitmap *Bitmap)
{
    int pitch;
    uint8_t *dst;
    IDirectFBSurface  *tmpSurface;

  tmpSurface = (useStretchBlit) ? osdSurface : scrSurface;

  /* --------------------------------------------------------------------------
   * ?? if that clear is removed, radeon OSD does not flicker
   * any more. but it has some other negative effects on mga.
   * ??
   */
  tmpSurface->Clear(0,0,0,0);
  tmpSurface->Lock(DSLF_WRITE, (void **)&dst, &pitch) ;
  Draw(Bitmap,dst,pitch);
  tmpSurface->Unlock();

  if (useStretchBlit)
  {
    OSDpresent = true;
    //tmpSurface->Flip();
  }
  tmpSurface->Flip();
}

#else

/* ---------------------------------------------------------------------------
 */
void cDFBVideoOut::Refresh()
{
    int               pitch;
    uint8_t           *dst;
    IDirectFBSurface  *tmpSurface;

  tmpSurface = (useStretchBlit) ? osdSurface : scrSurface;

  tmpSurface->Clear(0,0,0,0);
  tmpSurface->Lock(DSLF_WRITE, (void **)&dst, &pitch) ;
  for (int i = 0; i < MAXNUMWINDOWS; i++)
  {
    if (layer[i] && layer[i]->visible)
      layer[i]->Draw(dst, pitch, NULL);
  }
  tmpSurface->Unlock();

  if (useStretchBlit)
    OSDpresent = true;

  tmpSurface->Flip();
}
#endif

/* ---------------------------------------------------------------------------
 */
void cDFBVideoOut::CloseOSD()
{
    IDirectFBSurface  *tmpSurface;

  tmpSurface = (useStretchBlit) ? osdSurface : scrSurface;

  if (useStretchBlit)
  {
    osdMutex.Lock();
    OSDpresent  = false;
    osdClrBack = true;
    tmpSurface->Clear(COLORKEY,0); //clear and
    osdMutex.Unlock();
  }
  else
  {
    tmpSurface->Clear(COLORKEY,0); //clear and
    tmpSurface->Flip(); // Flip the field
    tmpSurface->Clear(COLORKEY,0); //clear and
    tmpSurface->Flip(); // Flip the field
  }

}

/* ---------------------------------------------------------------------------
 */
void cDFBVideoOut::ShowOSD ()
{
  if (useStretchBlit && (OSDpresent || osdClrBack)) {
    // do image and OSD mix here
      DFBRectangle  src, dst, osdsrc;

    src.x = sxoff;
    src.y = syoff;
    src.w = swidth;
    src.h = sheight;
    dst.x = lxoff;
    dst.y = lyoff;
    dst.w = lwidth;
    dst.h = lheight;

    osdMutex.Lock();
    try {
      if (osdClrBack) {
        scrSurface->Clear(0,0,0,0);
        osdSurface->Clear(0,0,0,0);
        osdSurface->Flip();
      }

      scrSurface->SetBlittingFlags(DSBLIT_NOFX);
      scrSurface->StretchBlit(videoSurface, &src, &dst);

      osdsrc.x = osdsrc.y = 0;
      osdsrc.w = Xres;osdsrc.h=Yres;
      scrSurface->SetBlittingFlags(DSBLIT_BLEND_ALPHACHANNEL);
      scrSurface->Blit(osdSurface, &osdsrc, 0, 0);

      scrSurface->Flip(NULL, DSFLIP_ONSYNC);

      if (osdClrBack) {
        scrSurface->Clear(0,0,0,0);
        osdSurface->Clear(0,0,0,0);
        osdSurface->Flip();
      }

    } catch (DFBException *ex){
      fprintf(stderr,"--- OSD refresh failed failed\n");
    }
    osdClrBack = false;
    osdMutex.Unlock();
  }
}

/* ---------------------------------------------------------------------------
 */
void cDFBVideoOut::YUV(uint8_t *Py, uint8_t *Pu, uint8_t *Pv,
                       int Width, int Height, int Ystride, int UVstride)
{
    uint8_t *dst;
    int pitch;
    int hi;

  SetParams();

  //fprintf(stderr,"[dfb] draw frame (%d x %d) Y: %d UV: %d\n", Width, Height, Ystride, UVstride);

  videoSurface->Lock(DSLF_WRITE, (void **)&dst, &pitch);
  if (pixelformat == DSPF_I420 || pixelformat == DSPF_YV12)
  {
#if HAVE_SetSourceLocation
    for(hi=0; hi < Height; hi++){
      memcpy(dst, Py, Width);
      Py  += Ystride;
      dst += pitch;
    }
    for(hi=0; hi < Height/2; hi++) {
      memcpy(dst, Pu, Width/2);
      Pu  += UVstride;
      dst += pitch / 2;
    }

    for(hi=0; hi < Height/2; hi++) {
      memcpy(dst, Pv, Width/2);
      Pv  += UVstride;
      dst += pitch / 2;
    }
#else

    Py += (Ystride * syoff);
    Pv += (UVstride * syoff/2);
    Pu += (UVstride * syoff/2);

    for(hi=0; hi < sheight; hi++){
      memcpy(dst, Py+sxoff, swidth);
      Py  += Ystride;
      dst += pitch;
    }
    for(hi=0; hi < sheight/2; hi++) {
      memcpy(dst, Pu+sxoff/2, swidth/2);
      Pu  += UVstride;
      dst += pitch / 2;
    }

    for(hi=0; hi < sheight/2; hi++) {
      memcpy(dst, Pv+sxoff/2, swidth/2);
      Pv  += UVstride;
      dst += pitch / 2;
    }
#endif
  } else if (pixelformat == DSPF_YUY2) {

#ifndef USE_MMX
//#if 1
    /* reference implementation */
      int p = pitch - Width*2;
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
      dst +=p;
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
      dst += pitch;
    }
#endif
  }
  videoSurface->Unlock();

  try {
    if (useStretchBlit)
    {
        DFBRectangle  src, dst;

      src.x = sxoff;
      src.y = syoff;
      src.w = swidth;
      src.h = sheight;
      dst.x = lxoff;
      dst.y = lyoff;
      dst.w = lwidth;
      dst.h = lheight;
      //fprintf (stderr, "src (%d,%d %dx%d)\n", sxoff,syoff,swidth,sheight);
      //fprintf (stderr, "dst (%d,%d %dx%d)\n", lxoff,lyoff,lwidth,lheight);

      osdMutex.Lock();
      if (aspect_changed || osdClrBack)
        scrSurface->Clear(0,0,0,0);

      scrSurface->SetBlittingFlags(DSBLIT_NOFX);
      scrSurface->StretchBlit(videoSurface, &src, &dst);

      if (OSDpresent)
      {
          DFBRectangle  osdsrc;
        osdsrc.x = osdsrc.y = 0;
        osdsrc.w = Xres;osdsrc.h=Yres;
        scrSurface->SetBlittingFlags(DSBLIT_BLEND_ALPHACHANNEL);
        scrSurface->Blit(osdSurface, &osdsrc, 0, 0);
      }

      scrSurface->Flip(NULL, DSFLIP_ONSYNC);

      if (aspect_changed || osdClrBack)
        scrSurface->Clear(0,0,0,0);

      osdClrBack = false;
      osdMutex.Unlock();
    }
    else
    {
      videoSurface->Flip();
    }
  } catch (DFBException *ex){
    fprintf(stderr,"Flip failed\n");
  }
  ProcessEvents ();
  events_not_done = 0;
}

/* ---------------------------------------------------------------------------
 */
cDFBVideoOut::~cDFBVideoOut()
{
    fprintf(stderr,"Releasing DFB\n");
    if (dfb) dfb->Release();
}
