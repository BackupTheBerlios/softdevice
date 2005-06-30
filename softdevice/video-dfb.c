/*
 * video.c: A plugin for the Video Disk Recorder
 *
 * See the README file for copyright information and how to reach the author.
 *
 * $Id: video-dfb.c,v 1.31 2005/06/30 21:46:16 lucke Exp $
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

typedef struct
{
  const char            *shortName;
  const char            *longName;
  IDirectFBDisplayLayer *layer;
  DFBResult             result;
} tLayerSelectItem;

#define BES_LAYER       0
#define CRTC2_LAYER_OLD 1
#define CRTC2_LAYER_NEW 2
#define SPIC_LAYER      3
#define ANY_LAYER       4

tLayerSelectItem  layerList [5] =
  {
    {"bes",   "Matrox Backend Scaler",    NULL, DFB_UNSUPPORTED},
    {"crtc2", "Matrox CRTC2",             NULL, DFB_UNSUPPORTED},
    {"crtc2", "Matrox CRTC2 Layer",       NULL, DFB_UNSUPPORTED},
    {"spic",  "Matrox CRTC2 Sub-Picture", NULL, DFB_UNSUPPORTED},
    {NULL,    NULL,                       NULL, DFB_UNSUPPORTED}
  };

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
static DFBEnumerationResult EnumCallBack(unsigned int id,
                                         DFBDisplayLayerDescription desc,
                                         void *data)
{

    tLayerSelectItem *layer = (tLayerSelectItem *)data;

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
  if (desc.caps & DLCAPS_FIELD_PARITY) fprintf(stderr,"field_parity " );
  if (desc.caps & DLCAPS_OPACITY) fprintf(stderr,"opacity " );
  if (desc.caps & DLCAPS_SATURATION) fprintf(stderr,"saturation " );
  if (desc.caps & DLCAPS_SCREEN_LOCATION) fprintf(stderr,"screen_location " );
  if (desc.caps & DLCAPS_SRC_COLORKEY) fprintf(stderr,"src_colorkey " );
  if (desc.caps & DLCAPS_SURFACE) fprintf(stderr,"surface " );
  fprintf(stderr,"\n");

  if ((layer->shortName && !strcmp (layer->longName,desc.name)) ||
      (!layer->shortName && id != DLID_PRIMARY && desc.caps & DLCAPS_SURFACE))
  {
      layer->layer = DFBWrapper->GetDisplayLayer(id);
      return DFENUM_CANCEL;
  }

  return DFENUM_OK;
}

/* ---------------------------------------------------------------------------
 */
static void reportCardInfo (IDirectFB *dfb)
{
#if (DIRECTFB_MAJOR_VERSION == 0) && (DIRECTFB_MINOR_VERSION == 9) && (DIRECTFB_MICRO_VERSION < 23)
    DFBCardCapabilities           caps;

  dfb->GetCardCapabilities(&caps);
#else
    DFBGraphicsDeviceDescription  caps;

  dfb->GetDeviceDescription(&caps);
#endif

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
  fprintf(stderr,"[surface capabilities] %s: ", name);

  if (scaps == DSCAPS_NONE) fprintf(stderr,"none ");
  if (scaps & DSCAPS_PRIMARY) fprintf(stderr,"primary ");
  if (scaps & DSCAPS_SYSTEMONLY) fprintf(stderr,"systemonly ");
  if (scaps & DSCAPS_VIDEOONLY) fprintf(stderr,"videoonly ");
  if (scaps & DSCAPS_DOUBLE) fprintf(stderr,"double-buffered ");
  if (scaps & DSCAPS_FLIPPING) fprintf(stderr,"flipping ");
  if (scaps & DSCAPS_SUBSURFACE) fprintf(stderr,"subsurface ");
  if (scaps & DSCAPS_INTERLACED) fprintf(stderr,"interlaced ");
  if (scaps & DSCAPS_SEPARATED) fprintf(stderr,"separated ");
  if (scaps & DSCAPS_STATIC_ALLOC) fprintf(stderr,"static-alloc ");
  if (scaps & DSCAPS_TRIPLE) fprintf(stderr,"triple-buffered ");
  fprintf(stderr,"\n");
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
cDFBVideoOut::cDFBVideoOut(cSetupStore *setupStore)
              : cVideoOut(setupStore)
{
    DFBDisplayLayerDescription    desc;
    tLayerSelectItem              *layerInfo;

  fprintf(stderr,"[dfb] init\n");
  /* --------------------------------------------------------------------------
   * default settings: source, destination and logical widht/height
   * are set to our well known dimensions (except fwidth+fheight).
   */
  swidth  = fwidth  = 720;
  sheight = fheight = 576;

  screenPixelAspect = -1;
  currentPixelFormat = setupStore->pixelFormat;
  isVIAUnichrome = false;
  clearAlpha = 0x00;
  clearBackground = 0;
  clearBackCount = 2; // by default for double buffering;
  OSDpresent = false;

  DirectFB::Init();
  dfb = DirectFB::Create();
  if (!setupStore->useMGAtv)
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
  layerInfo = &layerList [ANY_LAYER];
  if (setupStore->useMGAtv) {
    layerInfo = &layerList [CRTC2_LAYER_NEW];
    currentPixelFormat = setupStore->pixelFormat = 2;
    if (!setupStore->screenPixelAspect)
      setupStore->screenPixelAspect = 1;
  }

  dfb->EnumDisplayLayers(EnumCallBack, layerInfo);
  videoLayer = layerInfo->layer;

  if (setupStore->useMGAtv && !videoLayer) {
    layerInfo = &layerList [CRTC2_LAYER_OLD];
    fprintf(stderr, "[dfb] New layer name allocation failed. Trying old (dfb-0.9.20) layer name\n");
    dfb->EnumDisplayLayers(EnumCallBack, layerInfo);
    videoLayer = layerInfo->layer;
  }

  scrDsc.flags = (DFBSurfaceDescriptionFlags) (DSDESC_CAPS |
                                               DSDESC_PIXELFORMAT);
  scrDsc.caps = DSCAPS_NONE;
  DFB_ADD_SURFACE_CAPS(scrDsc.caps, DSCAPS_FLIPPING);
  DFB_ADD_SURFACE_CAPS(scrDsc.caps, DSCAPS_PRIMARY);
  DFB_ADD_SURFACE_CAPS(scrDsc.caps, DSCAPS_VIDEOONLY);
  //DFB_ADD_SURFACE_CAPS(scrDsc.caps, DSCAPS_DOUBLE);
  scrDsc.pixelformat = DSPF_ARGB;

  if (setupStore->useMGAtv)
  {
      DFBDisplayLayerConfig   dlc;

    dlc.flags = (DFBDisplayLayerConfigFlags)
                  (DLCONF_PIXELFORMAT | DLCONF_BUFFERMODE | DLCONF_OPTIONS);
    //dlc.buffermode = DLBM_BACKVIDEO;
    dlc.buffermode = DLBM_TRIPLE;
    clearBackCount = 3;             // 3 for triple, 2 for double buffering
    dlc.pixelformat = DSPF_ARGB;
    dlc.options = DLOP_FIELD_PARITY;

    try
    {
      osdLayer->SetCooperativeLevel(DLSCL_EXCLUSIVE);
      osdLayer->SetOpacity(0xff);
    }
    catch (DFBException *ex)
    {
      fprintf (stderr,
               "Caught: action=%s, result=%s (could not set DLSCL_EXCLUSIVE)\n",
               ex->GetAction(), ex->GetResult());
    }

    try
    {
      osdLayer->SetConfiguration(dlc);
    }
    catch (DFBException *ex)
    {
      fprintf (stderr,"Caught: action=%s, result=%s\n",
               ex->GetAction(), ex->GetResult());
    }

    try
    {
      osdLayer->SetFieldParity(0);
    }
    catch (DFBException *ex)
    {
      fprintf (stderr,"Caught: action=%s, result=%s (could not set field parity)\n",
               ex->GetAction(), ex->GetResult());
    }
    scrSurface = osdLayer->GetSurface();

  }
  else
  {
    scrSurface   = dfb->CreateSurface (scrDsc);
  }

  reportSurfaceCapabilities ("scrSurface", scrSurface);

  if (!videoLayer)
  {
    fprintf(stderr,"[dfb]: could not find suitable videolayer\n");
    exit(1);
  }

  /* --------------------------------------------------------------------------
   * check for VIA Unichrome presence
   */
  desc = videoLayer->GetDescription();
  if (!strcmp (desc.name, "VIA Unichrome Video"))
  {
    isVIAUnichrome = true;
    clearAlpha = 0xff;
  }

  if (scrSurface)
  {
      DFBSurfacePixelFormat fmt;
      double                displayRatio;

    scrSurface->GetSize(&Xres,&Yres);
    fprintf (stderr,
             "[dfb] width = %d, height = %d\n",
             Xres, Yres);
    lwidth  = dwidth  = Xres;
    lheight = dheight = Yres;

    displayRatio = (double) Xres / (double) Yres;
    SetParValues(displayRatio, displayRatio);

    fmt = scrSurface->GetPixelFormat();
    fprintf (stderr,
             "[dfb] got fmt = 0x%08x bpp = %d\n",
             fmt, DFB_BITS_PER_PIXEL(fmt));
    Bpp = DFB_BITS_PER_PIXEL(fmt);

    if (Xres > OSD_FULL_WIDTH)
      Xres = OSD_FULL_WIDTH;
    if (Yres > OSD_FULL_HEIGHT)
      Yres = OSD_FULL_HEIGHT;

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
    if (setupStore->pixelFormat == 0)
      vidDsc.pixelformat = DSPF_I420;
    else if (setupStore->pixelFormat == 1)
      vidDsc.pixelformat = DSPF_YV12;
    else if (setupStore->pixelFormat == 2)
    {
      vidDsc.pixelformat = DSPF_YUY2;
      useStretchBlit = true;
      OSDpseudo_alpha = false;
    }

    osdSurface   = dfb->CreateSurface (osdDsc);
    osdSurface->Clear(0,0,0,clearAlpha); //clear and
    osdSurface->Flip(); // Flip the field
    osdSurface->Clear(0,0,0,clearAlpha); //clear and

    desc = osdLayer->GetDescription();
    fprintf(stderr,
            "[dfb] Using this layer for OSD: (%s - [%dx%d])\n",
            desc.name, osdSurface->GetWidth(), osdSurface->GetHeight());

    reportSurfaceCapabilities ("osdSurface", osdSurface);

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
      videoSurface->Clear(0,0,0,clearAlpha); //clear and
    }
    else
    {
      videoSurface=videoLayer->GetSurface();
      videoSurface->Clear(COLORKEY,clearAlpha); //clear and
      videoSurface->Flip(); // Flip the field
      videoSurface->Clear(COLORKEY,clearAlpha); //clear and
      videoSurface->Flip(); // Flip the field
    }

    reportSurfaceCapabilities ("videoSurface", videoSurface);

    if (!setupStore->useMGAtv)
    {
      fprintf(stderr,"[dfb] Configuring CooperativeLevel for Overlay\n");
      videoLayer->SetCooperativeLevel(DLSCL_ADMINISTRATIVE);

      fprintf(stderr,"[dfb] Configuring CooperativeLevel for OSD\n");
      osdLayer->SetCooperativeLevel(DLSCL_ADMINISTRATIVE);
    }

    desc = osdLayer->GetDescription();
    fprintf(stderr,"[dfb] Using this layer for OSD: %s\n", desc.name);

    desc = videoLayer->GetDescription();
    fprintf(stderr,"[dfb] Using this layer for Video out: %s\n", desc.name);

  }

  GetDisplayFrameTime();
  /* create an event buffer with all devices attached */
  events = dfb->CreateInputEventBuffer(DICAPS_ALL,
                                       (setupStore->useMGAtv) ? DFB_TRUE : DFB_FALSE);
}

/* ---------------------------------------------------------------------------
 */
void cDFBVideoOut::GetDisplayFrameTime (void)
{
  if (videoLayer)
  {
      struct timeval  tv1, tv2;
      int             t1, t2;

    videoLayer->GetScreen()->WaitForSync();
    gettimeofday(&tv1,NULL);
    videoLayer->GetScreen()->WaitForSync();
    gettimeofday(&tv2,NULL);
    t1 = (tv1.tv_sec & 1) * 1000000 + tv1.tv_usec;
    t2 = (tv2.tv_sec & 1) * 1000000 + tv2.tv_usec;
    fprintf (stderr,"[dfb] Display frame time is %dµs\n", t2 - t1);
    displayTimeUS = t2 - t1;
  }
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
              if (!setupStore->CatchRemoteKey(dfbRemote->Name(),
                                              event.key_symbol))
              {
                dfbRemote->PutKey(event.key_symbol);
              }
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
    DFBDisplayLayerConfigFlags  failed;

  if (videoLayer || useStretchBlit)
  {
    if (!videoSurface ||
        aspect_changed ||
        currentPixelFormat != setupStore->pixelFormat)
    {

      fprintf(stderr,
              "[dfb] (re)configuring Videolayer to %d x %d (%dx%d)\n",
              fwidth,fheight,swidth,sheight);
      dlc.flags   = (DFBDisplayLayerConfigFlags)(DLCONF_WIDTH | DLCONF_HEIGHT | DLCONF_PIXELFORMAT | DLCONF_OPTIONS);

      useStretchBlit = false;
      OSDpseudo_alpha = (isVIAUnichrome) ? false: true;
      if (setupStore->pixelFormat == 0)
        dlc.pixelformat = DSPF_I420;
      else if (setupStore->pixelFormat == 1)
        dlc.pixelformat = DSPF_YV12;
      else if (setupStore->pixelFormat == 2)
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
      dlc.flags = (DFBDisplayLayerConfigFlags)((int) dlc.flags |
                                                      DLCONF_OPTIONS);
      dlc.options       = DLOP_FIELD_PARITY;

#if (DIRECTFB_MAJOR_VERSION > 0 || ((DIRECTFB_MINOR_VERSION == 9) && DIRECTFB_MICRO_VERSION > 22))
      dlc.flags = (DFBDisplayLayerConfigFlags)
                      ((int) dlc.flags | DLCONF_SURFACE_CAPS);
      dlc.surface_caps  = DSCAPS_DOUBLE;
#endif

      vidDsc.flags = (DFBSurfaceDescriptionFlags) (DSDESC_CAPS |
                                                   DSDESC_WIDTH |
                                                   DSDESC_HEIGHT |
                                                   DSDESC_PIXELFORMAT);
      vidDsc.caps        = DSCAPS_VIDEOONLY;
      vidDsc.width       = dlc.width;
      vidDsc.height      = dlc.height;
      vidDsc.pixelformat = dlc.pixelformat;

      currentPixelFormat = setupStore->pixelFormat;

      pixelformat = dlc.pixelformat;

      desc = videoLayer->GetDescription();

      if (!useStretchBlit)
      {
        if (videoSurface)
          videoSurface->Release();
        videoSurface = NULL;

        /* --------------------------------------------------------------------
         * set the default options to none
         */
        dlc.options = (DFBDisplayLayerOptions)( DLOP_NONE );
#if 0
        if (desc.caps & DLCAPS_DEINTERLACING)
        {
          /* ------------------------------------------------------------------
           * if deinterlacing is supported we'll use that
           */
          dlc.options = (DFBDisplayLayerOptions)( DLOP_DEINTERLACING );
        }
#endif
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
          if (desc.caps & DLCAPS_DST_COLORKEY)
          {
            /* ----------------------------------------------------------------
             * no alpha channel but destination color keying is supported
             */
            dlc.options = (DFBDisplayLayerOptions)((int)dlc.options|
                                                   DLOP_DST_COLORKEY);
          }
        }

        if (setupStore->useMGAtv)
          dlc.options = (DFBDisplayLayerOptions)((int)dlc.options|
                                                 DLOP_FIELD_PARITY);
        try
        {
          if (isVIAUnichrome)
            videoLayer->SetLevel(1);
        }
        catch (DFBException *ex)
        {
          fprintf (stderr,"Caught: action=%s, result=%s Failed: SetLevel()\n",
                   ex->GetAction(), ex->GetResult());
        }

        /*
         * --------------------------------------------------------------------
         * Try with triple or double buffering
         */
        dlc.flags = (DFBDisplayLayerConfigFlags)
                      ((int) dlc.flags | DLCONF_BUFFERMODE);

        //dlc.buffermode = DLBM_TRIPLE;

        //videoLayer->TestConfiguration(dlc, &failed);
        //if (failed & DLCONF_BUFFERMODE)
        //{
        //  fprintf(stderr, "[dfb]: SetParms (): failed to set buffermode "
        //          "to triple mode, trying back video\n");
          dlc.buffermode = DLBM_BACKVIDEO;
          videoLayer->TestConfiguration(dlc, &failed);
          if (failed & DLCONF_BUFFERMODE)
          {
            fprintf(stderr, "[dfb]: SetParms (): failed to set buffermode "
                    "to back video, reverting to normal\n");
            dlc.buffermode = DLBM_FRONTONLY;
          }
        //}

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
          /* ------------------------------------------------------------------
           * workaround for a bug in DirectFB: force SetScreenLocation to be
           * executed always.
           */
          videoLayer->SetScreenLocation(0.0,0.0,0.5,0.5);
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
        if (videoSurface)
        {
          /* ------------------------------------------------------------------
           * clear previous used surface. there were some troubles when we
           * started in I420 mode, switched to YUY2 and video format changed
           * from 4:3 to 16:9 .
           */
          videoSurface->Clear(COLORKEY,0); //clear and
          videoSurface->Release();
        }

        videoSurface=NULL;
        videoSurface=dfb->CreateSurface(vidDsc);
      }
      reportSurfaceCapabilities ("videoSurface", videoSurface);

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

/* ----------------------------------------------------------------------------
 */
void cDFBVideoOut::OpenOSD (int x, int y)
{
    IDirectFBSurface  *tmpSurface;

  cVideoOut::OpenOSD(x, y);
  tmpSurface = (useStretchBlit) ? osdSurface : scrSurface;
  tmpSurface->Clear(0,0,0,clearAlpha);
  tmpSurface->Flip();
  tmpSurface->Clear(0,0,0,clearAlpha);
}

/* ---------------------------------------------------------------------------
 */
void cDFBVideoOut::OSDStart()
{
    IDirectFBSurface  *tmpSurface;

  tmpSurface = (useStretchBlit) ? osdSurface : scrSurface;
  tmpSurface->SetBlittingFlags(DSBLIT_NOFX);
}

/* ---------------------------------------------------------------------------
 */
void cDFBVideoOut::OSDCommit()
{
  OSDdirty=false;
  OSDpresent = true;
}

/* ---------------------------------------------------------------------------
 */
void cDFBVideoOut::Refresh(cBitmap *Bitmap)
{
    int               pitch;
    uint8_t           *dst;
    IDirectFBSurface  *tmpSurface;
    DFBRegion         modArea;
    DFBRectangle      osdsrc;

  if (Bitmap->Dirty(modArea.x1,modArea.y1,modArea.x2,modArea.y2))
  {
    tmpSurface = (useStretchBlit) ? osdSurface : scrSurface;
    tmpSurface->Lock(DSLF_WRITE, (void **)&dst, &pitch) ;
    Draw(Bitmap,dst,pitch,(isVIAUnichrome) ? true:false);
    tmpSurface->Unlock();

    /* ------------------------------------------------------------------------
     * TODO: Have to get area coordinates in screen dimensions from Draw().
     *       In case Draw() scales down, bitmap dirty area coordinates
     *       could not be transformed the following way.
     */
    modArea.x1 += OSDxOfs + Bitmap->X0();
    modArea.y1 += OSDyOfs + Bitmap->Y0();
    modArea.x2 += OSDxOfs + Bitmap->X0();
    modArea.y2 += OSDyOfs + Bitmap->Y0();

    tmpSurface->Flip(&modArea,DSFLIP_WAIT);
    osdsrc.x = modArea.x1;
    osdsrc.y = modArea.y1;
    osdsrc.w = modArea.x2 - modArea.x1 + 1;
    osdsrc.h = modArea.y2 - modArea.y1 + 1;
    tmpSurface->Blit(tmpSurface, &osdsrc, modArea.x1, modArea.y1);

    Bitmap->Clean();
  }
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

  tmpSurface->Clear(0,0,0,clearAlpha);
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
    osdMutex.Unlock();
    tmpSurface->Clear(COLORKEY,clearAlpha); //clear and
  }
  else
  {
    tmpSurface->Clear(COLORKEY,clearAlpha); //clear and
    tmpSurface->Flip(); // Flip the field
    if (!isVIAUnichrome)
    {
      tmpSurface->Clear(COLORKEY,clearAlpha); //clear and
      tmpSurface->Flip(); // Flip the field
    }
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
    if (osdClrBack) {
      clearBackground = clearBackCount;
      osdClrBack = false;
    }

    try {
      if (clearBackground) {
        scrSurface->Clear(0,0,0,0);
        clearBackground--;
      }

      scrSurface->SetBlittingFlags(DSBLIT_NOFX);
      scrSurface->StretchBlit(videoSurface, &src, &dst);
      if (OSDpresent)
      {
        osdsrc.x = osdsrc.y = 0;
        osdsrc.w = Xres;osdsrc.h=Yres;
        scrSurface->SetBlittingFlags(DSBLIT_BLEND_ALPHACHANNEL);
        scrSurface->Blit(osdSurface, &osdsrc, 0, 0);
      }
      scrSurface->Flip(NULL, DSFLIP_WAITFORSYNC);

    } catch (DFBException *ex){
      fprintf(stderr,"--- OSD refresh failed failed\n");
    }
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

  OsdRefreshCounter=0;
  events_not_done = 0;
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
      clearBackground = (aspect_changed || osdClrBack) ? clearBackCount: clearBackground;
      osdClrBack = false;

      if (clearBackground)
      {
        scrSurface->Clear(0,0,0,0);
        clearBackground--;
      }

      osdMutex.Unlock();

      scrSurface->SetBlittingFlags(DSBLIT_NOFX);
      scrSurface->StretchBlit(videoSurface, &src, &dst);

      if (OSDpresent)
      {
          DFBRectangle  osdsrc;
        osdsrc.x = osdsrc.y = 0;
        osdsrc.w = Xres;osdsrc.h=Yres;
        scrSurface->SetBlittingFlags(DSBLIT_BLEND_ALPHACHANNEL);
#if 0
        /* --------------------------------------------------------------------
         * test for OSD scaleinf to 4:3 aspect on a 16:9 tv
         */
          DFBRectangle  osddest;
        osddest.x = 90; osddest.y = 0;
        osddest.w = 540; osddest.h = Yres;
        scrSurface->StretchBlit(osdSurface, &osdsrc, &osddest);
#else
        scrSurface->Blit(osdSurface, &osdsrc, 0, 0);
#endif

      }
      //scrSurface->Flip(NULL, (setupStore->useMGAtv) ? DSFLIP_WAITFORSYNC:DSFLIP_ONSYNC);
      scrSurface->Flip(NULL, DSFLIP_WAITFORSYNC);

    }
    else
    {
      videoSurface->Flip(NULL, DSFLIP_ONSYNC);
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

  if (videoSurface) videoSurface->Release();
  if (osdSurface)   osdSurface->Release();
  if (scrSurface)   scrSurface->Release();
  if (videoLayer)   videoLayer->Release ();
  if (osdLayer)     osdLayer->Release();
  if (dfb)          dfb->Release();
}

#ifdef USE_SUBPLUGINS
/* ---------------------------------------------------------------------------
 */
extern "C" void *
SubPluginCreator(cSetupStore *s)
{
  return new cDFBVideoOut(s);
}
#endif
