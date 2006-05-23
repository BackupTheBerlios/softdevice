/*
 * video.c: A plugin for the Video Disk Recorder
 *
 * See the README file for copyright information and how to reach the author.
 *
 * $Id: video-dfb.c,v 1.61 2006/05/23 21:28:54 lucke Exp $
 */

#include <sys/mman.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <vdr/plugin.h>
#include "video-dfb.h"
#include "utils.h"
#include "setup-softdevice.h"


#ifdef HAVE_CONFIG
# include "config.h"
#else
# define HAVE_SetSourceLocation 0
# define HAVE_DSBLIT_INTERLACED 0
# if (DIRECTFB_MAJOR_VERSION == 0) && (DIRECTFB_MINOR_VERSION == 9) && (DIRECTFB_MICRO_VERSION < 23)
#   define HAVE_GraphicsDeviceDescription 0
#   define HAVE_DIEF_REPEAT               0
# else
#   define HAVE_GraphicsDeviceDescription 1
#   define HAVE_DIEF_REPEAT               1
# endif
# if (DIRECTFB_MAJOR_VERSION > 0 || ((DIRECTFB_MINOR_VERSION == 9) && DIRECTFB_MICRO_VERSION > 22))
#   define HAVE_DSCAPS_DOUBLE 1
# else
#   define HAVE_DSCAPS_DOUBLE 0
# endif
#endif

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
#if HAVE_GraphicsDeviceDescription
    DFBGraphicsDeviceDescription  caps;

  dfb->GetDeviceDescription(&caps);
#else
    DFBCardCapabilities           caps;

  dfb->GetCardCapabilities(&caps);
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
#if HAVE_DSBLIT_INTERLACED
  if (caps.blitting_flags & DSBLIT_INTERLACED ) fprintf(stderr,"Interlaced ");
#endif
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

  fprintf(stderr, "PixelFormat = 0x%08x ", surf->GetPixelFormat());
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
static void BESColorkeyState(IDirectFBDisplayLayer *layer, bool state)
{
  if (layer)
  {
      DFBDisplayLayerConfig       dlc;
    dlc.flags = DLCONF_OPTIONS;
    dlc.options = (state) ? DLOP_DST_COLORKEY : DLOP_NONE;
    try
    {
      layer->SetConfiguration(dlc);
      layer->SetDstColorKey(COLORKEY);
    }
    catch (DFBException *ex)
    {
      fprintf (stderr,"[dfb] BES-%s: action=%s, result=%s\n",
               (state) ? "ON" : "OFF",
               ex->GetAction(), ex->GetResult());
      delete ex;
    }
  }
}

/* ---------------------------------------------------------------------------
 */
static void ReportLayerInfo(IDirectFBDisplayLayer *layer, char *name)
{
      DFBDisplayLayerConfig layerConfiguration;

  if (!layer)
  {
    fprintf (stderr, "[dfb] no layer info. layer == NULL\n");
    return;
  }

  layer->GetConfiguration(&layerConfiguration);
  {
    fprintf(stderr,
            "[dfb] (%s): flags, options, pixelformat: %08x, %08x %08x\n",
            name,
            layerConfiguration.flags,
            layerConfiguration.options,
            layerConfiguration.pixelformat);
    fprintf(stderr,
            "[dfb] (%s): width, height:               %d %d\n",
            name,
            layerConfiguration.width,
            layerConfiguration.height);
  }
}

/* ---------------------------------------------------------------------------
 */
cDFBVideoOut::cDFBVideoOut(cSetupStore *setupStore)
              : cVideoOut(setupStore)
{
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
  prevOsdMode = setupStore->osdMode;
  setupStore->osdMode = 0;
  isVIAUnichrome = false;
  clearAlpha = 0x00;
  clearBackground = 0;
  clearBackCount = 2; // by default for double buffering;
  videoLayerLevel = 1;
  OSDpresent = false;
  fieldParity = 0;    // 0 - top field first, 1 - bottom field first

  if(setupStore->viaTv)
    setupStore->tripleBuffering = 1;

  try
  {
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
    fprintf(stderr,"[dfb] Enumerating display Layers\n");

    osdLayer=dfb->GetDisplayLayer(DLID_PRIMARY);
    if (!osdLayer) {
      fprintf(stderr,"[dfb] no OSD layer exiting\n");
      exit(EXIT_FAILURE);
    }

    if (!setupStore->useMGAtv)
    {
      fprintf(stderr,"[dfb] Configuring CooperativeLevel for OSD\n");
      osdLayer->SetCooperativeLevel(DLSCL_ADMINISTRATIVE);
    }

    osdLayerDescription = osdLayer->GetDescription();

    osdLayer->GetConfiguration(&osdLayerConfiguration);
    osdLayerConfiguration.flags = DLCONF_ALL;

    videoLayer = NULL;
    layerInfo = &layerList [ANY_LAYER];
    if (setupStore->useMGAtv) {
      layerInfo = &layerList [CRTC2_LAYER_NEW];
      currentPixelFormat = setupStore->pixelFormat = 2;
      setupStore->useStretchBlit = 1;

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

    if (!videoLayer)
    {
      fprintf(stderr,"[dfb]: could not find suitable videolayer\n");
      exit(EXIT_FAILURE);
    }

    videoLayerDescription = videoLayer->GetDescription();

    /* --------------------------------------------------------------------------
     * check for VIA Unichrome presence
     */
    if (!strcmp (videoLayerDescription.name, "VIA Unichrome Video"))
    {
      isVIAUnichrome = true;
      clearAlpha = 0xff;

      ReportLayerInfo(osdLayer, "osdLayer");
      if (osdLayerDescription.caps & DLCAPS_ALPHACHANNEL)
      {

        fprintf(stderr, "[dfb] osdLayer has alpha channel\n");
        osdLayerConfiguration.options = DLOP_ALPHACHANNEL;
        videoLayerLevel = -1;
      }
      else
      {
        fprintf(stderr, "[dfb] osdLayer without !! alpha channel\n");
      }
    }

    osdLayer->SetCooperativeLevel(DLSCL_EXCLUSIVE);
    osdLayer->SetConfiguration(osdLayerConfiguration);

    if (setupStore->useMGAtv)
      EnableFieldParity(osdLayer);
    if (setupStore->viaTv)
      EnableFieldParity(videoLayer);

    scrSurface = osdLayer->GetSurface();

    reportSurfaceCapabilities ("scrSurface", scrSurface);

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
      {
        vidDsc.pixelformat = DSPF_I420;
        setupStore->useStretchBlit = useStretchBlit = false;
      }
      else if (setupStore->pixelFormat == 1)
      {
        vidDsc.pixelformat = DSPF_YV12;
        setupStore->useStretchBlit = useStretchBlit = false;
      }
      else if (setupStore->pixelFormat == 2)
      {
        vidDsc.pixelformat = DSPF_YUY2;
        if (setupStore->useStretchBlit)
        {
          useStretchBlit = true;
          OSDpseudo_alpha = false;
        }
      }

      osdSurface   = dfb->CreateSurface (osdDsc);
      osdSurface->Clear(0,0,0,clearAlpha); //clear and
      osdSurface->Flip(); // Flip the field
      osdSurface->Clear(0,0,0,clearAlpha); //clear and

      fprintf(stderr,
              "[dfb] Using this layer for OSD: (%s - [%dx%d])\n",
              osdLayerDescription.name,
              osdSurface->GetWidth(), osdSurface->GetHeight());

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

        if (!setupStore->viaTv)
          BESColorkeyState(videoLayer, true);
      }

      fprintf(stderr,
              "[dfb] Using this layer for OSD:        %s\n"
              "[dfb] Using this layer for Video out:  %s\n",
              osdLayerDescription.name,
              videoLayerDescription.name);
    }

    GetDisplayFrameTime();
    /* create an event buffer with all devices attached */
    events = dfb->CreateInputEventBuffer(DICAPS_ALL,
                                         (setupStore->useMGAtv) ? DFB_TRUE : DFB_FALSE);
  }
  catch (DFBException *ex)
  {
    fprintf (stderr,"[dfb] init EXITING:action=%s, result=%s\n",
             ex->GetAction(), ex->GetResult());
    delete ex;
    exit(EXIT_FAILURE);
  }
}

/* ---------------------------------------------------------------------------
 */
void cDFBVideoOut::EnableFieldParity(IDirectFBDisplayLayer *layer)
{
    DFBDisplayLayerConfig         dlc;
    DFBDisplayLayerDescription    desc;

  desc = layer->GetDescription();

  dlc.flags = (DFBDisplayLayerConfigFlags)
                (DLCONF_PIXELFORMAT | DLCONF_BUFFERMODE | DLCONF_OPTIONS);
  dlc.buffermode = DLBM_TRIPLE;
  fprintf(stderr,"[dfb] Set DLBM_TRIPLE for layer [%s]\n", desc.name);

  dlc.pixelformat = DSPF_ARGB;
  clearBackCount = 3;             // 3 for triple, 2 for double buffering

  if (desc.caps & DLCAPS_FIELD_PARITY)
  {
    dlc.options = DLOP_FIELD_PARITY;
    fprintf(stderr,
            "[dfb] DLOP_FIELD_PARITY supported by layer [%s]\n",
            desc.name);
  }
  else
  {
    dlc.options = DLOP_NONE;
    fprintf(stderr,
            "[dfb] DLOP_FIELD_PARITY not supported by layer [%s]\n",
            desc.name);
  }

  try
  {
    layer->SetCooperativeLevel(DLSCL_EXCLUSIVE);
    layer->SetOpacity(0xff);
    layer->SetConfiguration(dlc);
    if (desc.caps & DLCAPS_FIELD_PARITY)
      layer->SetFieldParity(fieldParity);
  }
  catch(DFBException *ex)
  {
    fprintf (stderr,"[dfb] EnableFieldParity: action=%s, result=%s\n",
             ex->GetAction(), ex->GetResult());
    delete ex;
  }
}

/* ---------------------------------------------------------------------------
 */
void cDFBVideoOut::GetDisplayFrameTime (void)
{
  if (videoLayer)
  {
      struct timeval  tv1, tv2;
      int             t1, t2;

    try
    {
      videoLayer->GetScreen()->WaitForSync();
      gettimeofday(&tv1,NULL);
      videoLayer->GetScreen()->WaitForSync();
      gettimeofday(&tv2,NULL);
    }
    catch (DFBException *ex)
    {
      fprintf (stderr,"[dfb] GetDisplayFrameTime: action=%s, result=%s\n",
               ex->GetAction(), ex->GetResult());
      delete ex;
      tv1.tv_sec = tv2.tv_sec = tv1.tv_usec = tv2.tv_usec = 0;
    }
    t1 = (tv1.tv_sec & 1) * 1000000 + tv1.tv_usec;
    t2 = (tv2.tv_sec & 1) * 1000000 + tv2.tv_usec;
    fprintf (stderr,"[dfb] Display frame time is %d microseconds\n", t2 - t1);
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
    dfbRemote = new cSoftRemote ("softdevice-dfb");
  }
  videoInitialized = true;

  return true;
}

/* ---------------------------------------------------------------------------
 */
void cDFBVideoOut::ProcessEvents ()
{
    DFBInputEvent event;

  if (!videoInitialized)
    return;

  while (events->GetEvent( DFB_EVENT(&event) ))
  {
    if (event.type == DIET_KEYPRESS && dfbRemote)
    {
      switch (event.key_symbol)
      {
      case DIKS_SHIFT: case DIKS_CONTROL: case DIKS_ALT:
      case DIKS_ALTGR: case DIKS_META: case DIKS_SUPER: case DIKS_HYPER:
        break;
      default:
        if (!setupStore->CatchRemoteKey(dfbRemote->Name(), event.key_symbol))
        {
#if HAVE_DIEF_REPEAT
          dfbRemote->PutKey(event.key_symbol, event.flags & DIEF_REPEAT);
#else
          dfbRemote->PutKey(event.key_symbol, false);
#endif
        }
        break;
      }
    }
  }

  if (OsdRefreshCounter && (OsdRefreshCounter % 3) == 0)
    ShowOSD();
}

/* ---------------------------------------------------------------------------
 */
void cDFBVideoOut::SetParams()
{
    DFBDisplayLayerConfig       dlc;
    DFBDisplayLayerConfigFlags  failed;

  if (videoLayer || useStretchBlit)
  {
    if ( ! videoSurface || aspect_changed ||
        currentPixelFormat != setupStore->pixelFormat ||
        cutTop != setupStore->cropTopLines ||
        cutBottom != setupStore->cropBottomLines ||
        cutLeft != setupStore->cropLeftCols ||
        cutRight != setupStore->cropRightCols ||
        useStretchBlit != setupStore->useStretchBlit )
    {

      cutTop    = setupStore->cropTopLines;
      cutBottom = setupStore->cropBottomLines;
      cutLeft   = setupStore->cropLeftCols;
      cutRight  = setupStore->cropRightCols;

      fprintf(stderr,
              "[dfb] (re)configuring Videolayer to %d x %d (%dx%d)\n",
              fwidth,fheight,swidth,sheight);
      dlc.flags   = (DFBDisplayLayerConfigFlags)(DLCONF_WIDTH | DLCONF_HEIGHT | DLCONF_PIXELFORMAT | DLCONF_OPTIONS);

      useStretchBlit = false;
      OSDpseudo_alpha = !isVIAUnichrome;
      if (setupStore->pixelFormat == 0)
      {
        dlc.pixelformat = DSPF_I420;
        setupStore->useStretchBlit = useStretchBlit = false;
      }
      else if (setupStore->pixelFormat == 1)
      {
        dlc.pixelformat = DSPF_YV12;
        setupStore->useStretchBlit = useStretchBlit = false;
      }
      else if (setupStore->pixelFormat == 2)
      {
        dlc.pixelformat = DSPF_YUY2;
        if (setupStore->useStretchBlit)
        {
          useStretchBlit = true;
          OSDpseudo_alpha = false;
        }
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

#if HAVE_DSCAPS_DOUBLE
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
        if (videoLayerDescription.caps & DLCAPS_DEINTERLACING)
        {
          /* ------------------------------------------------------------------
           * if deinterlacing is supported we'll use that
           */
          dlc.options = (DFBDisplayLayerOptions)( DLOP_DEINTERLACING );
        }
#endif
        if (isVIAUnichrome)
        {
          try
          {
            videoLayer->SetLevel(videoLayerLevel);
          }
          catch (DFBException *ex)
          {
            fprintf (stderr,"[dfb] SetParams: action=%s, result=%s Failed: SetLevel()\n",
                     ex->GetAction(), ex->GetResult());
            delete ex;
          }
        }
        else if (videoLayerDescription.caps & DLCAPS_ALPHACHANNEL)
        {
          /* ------------------------------------------------------------------
           * use alpha channel if it is supported and disable pseudo alpha mode
           */
          dlc.options = (DFBDisplayLayerOptions)((int)dlc.options|DLOP_ALPHACHANNEL);
          OSDpseudo_alpha = false;
        }
        else
        {
          if (videoLayerDescription.caps & DLCAPS_DST_COLORKEY)
          {
            /* ----------------------------------------------------------------
             * no alpha channel but destination color keying is supported
             */
            dlc.options = (DFBDisplayLayerOptions)((int)dlc.options|
                                                   DLOP_DST_COLORKEY);
          }
        }

        if (setupStore->useMGAtv || setupStore->viaTv) {
          dlc.options = (DFBDisplayLayerOptions)((int)dlc.options|
                                                 DLOP_FIELD_PARITY);
          fprintf(stderr, "[dfb] SetParams: Enabling DLOP_FIELD_PARITY\n");
        }
        /*
         * --------------------------------------------------------------------
         * Try with triple or double buffering
         */
        dlc.flags = (DFBDisplayLayerConfigFlags)
                      ((int) dlc.flags | DLCONF_BUFFERMODE);

        dlc.buffermode = DLBM_UNKNOWN;
        if (setupStore->tripleBuffering)
        {
          dlc.buffermode = DLBM_TRIPLE;

          try
          {
            videoLayer->TestConfiguration(dlc, &failed);
          }
          catch (DFBException *ex)
          {
            if (failed & DLCONF_BUFFERMODE)
            {
              fprintf(stderr, "[dfb]: SetParms (): failed to set buffermode "
                      "to triple, will try back video\n");
              dlc.buffermode = DLBM_UNKNOWN;
            }
            delete ex;
          }
        }
        if (dlc.buffermode == DLBM_UNKNOWN)
        {
          dlc.buffermode = DLBM_BACKVIDEO;
          try
          {
            videoLayer->TestConfiguration(dlc, &failed);
          }
          catch (DFBException *ex)
          {
            if (failed & DLCONF_BUFFERMODE)
            {
              fprintf(stderr, "[dfb]: SetParams: failed to set buffermode "
                      "to back video, reverting to normal\n");
              dlc.buffermode = DLBM_FRONTONLY;
            }
            delete ex;
          }
        }

        /* --------------------------------------------------------------------
         * OK, try to set the video layer configuration
         */
        try
        {
          videoLayer->SetConfiguration(dlc);
#if HAVE_SetSourceLocation
          videoLayer->SetSourceRectangle (sxoff, syoff, swidth, sheight);
#endif
        }
        catch (DFBException *ex)
        {
          fprintf (stderr,"[dfb] SetParams: action=%s, result=%s\n",
                   ex->GetAction(), ex->GetResult());
          delete ex;
        }
        if (videoLayerDescription.caps & DLCAPS_SCREEN_LOCATION)
        {
          /* ------------------------------------------------------------------
           * workaround for a bug in DirectFB: force SetScreenLocation to be
           * executed always.
           */
          try
          {
            videoLayer->SetScreenLocation(0.0,0.0,0.5,0.5);
            videoLayer->SetScreenLocation((float) lxoff / (float) dwidth,
                                          (float) lyoff / (float) dheight,
                                          (float) lwidth / (float) dwidth,
                                          (float) lheight / (float) dheight);
          }
          catch (DFBException *ex)
          {
            fprintf (stderr, "[dfb] SetParams: action=%s, result=%s\n",
                     ex->GetAction(), ex->GetResult());
            delete ex;
          }
        }
        else
        {
          fprintf(stderr,"Can't configure ScreenLocation. Hope it is Fullscreen\n");
        }

        /* --------------------------------------------------------------------
         * set colorkey now. some driver accepct that _after_ configuration
         * has been set ! But check that color keying is supported.
         */
        try
        {
          if (videoLayerDescription.caps & DLCAPS_DST_COLORKEY)
            videoLayer->SetDstColorKey(COLORKEY);

          videoSurface=videoLayer->GetSurface();
          videoSurface->Clear(COLORKEY,0xff);
        }
        catch (DFBException *ex)
        {
          fprintf (stderr, "[dfb] SetParams (a) EXITING: action=%s, result=%s\n",
                   ex->GetAction(), ex->GetResult());
          delete ex;
          exit(EXIT_FAILURE);
        }
      }
      else
      {
        fprintf (stderr, "[dfb] creating new surface (stretchBlit)\n");
        try
        {
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
#if HAVE_DSBLIT_INTERLACED
	  if (setupStore->useMGAtv)
	  {
	    vidDsc.caps = DFB_ADD_SURFACE_CAPS(vidDsc.caps, DSCAPS_INTERLACED);
	  }
#endif

          videoSurface=dfb->CreateSurface(vidDsc);
          /* --------------------------------------------------------------------
           * Here is probably a bug in DirectFB, as Clear() does _not_ work
           * the same, as manual wipe out YUY2 surface. I tried Clear() on my
           * tests, but it did not solve the problem. That happens with my
           * Matrox G550.
           * videoSurface->Clear(0,0,0,0);
           * The workaround code assumes that in case of stretchBlit, pixelformat
           * YUY2 is used.
           */
          {
            int     *dst, i, j;
            int     pitch;

            videoSurface->Lock(DSLF_WRITE, (void **)&dst, &pitch);
            for(i = 0; i < vidDsc.height; ++i, dst += pitch/4)
              for (j = 0; j < vidDsc.width / 2; ++j)
                dst [j] = 0x80008000;
            videoSurface->Unlock();
          }
        }
        catch (DFBException *ex)
        {
          fprintf (stderr, "[dfb] SetParams (b) EXITING: action=%s, result=%s\n",
                   ex->GetAction(), ex->GetResult());
          delete ex;
          exit(EXIT_FAILURE);
        }
      }
      reportSurfaceCapabilities ("videoSurface", videoSurface);

      fprintf(stderr,"[dfb] (re)configured 0x%08x\n",pixelformat);
    }
  }
  else
  {
    fprintf(stderr,"No Videolayer available. Exiting...\n");
    exit(EXIT_FAILURE);
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
void cDFBVideoOut::OpenOSD ()
{
    IDirectFBSurface  *tmpSurface;

  if (!videoInitialized)
    return;

  cVideoOut::OpenOSD();
  try
  {
    tmpSurface = (useStretchBlit) ? osdSurface : scrSurface;
    tmpSurface->Clear(0,0,0,clearAlpha);
    tmpSurface->Flip();
    tmpSurface->Clear(0,0,0,clearAlpha);
  }
  catch (DFBException *ex)
  {
    fprintf (stderr,"[dfb] OpenOSD: action=%s, result=%s\n",
             ex->GetAction(), ex->GetResult());
    delete ex;
  }
}

/* ---------------------------------------------------------------------------
 */
void cDFBVideoOut::OSDStart()
{
    IDirectFBSurface  *tmpSurface;

  if (!videoInitialized)
    return;

  try
  {
    tmpSurface = (useStretchBlit) ? osdSurface : scrSurface;
    tmpSurface->SetBlittingFlags(DSBLIT_NOFX);
  }
  catch (DFBException *ex)
  {
    fprintf (stderr,"[dfb] OSDStart: action=%s, result=%s\n",
             ex->GetAction(), ex->GetResult());
    delete ex;
  }
}

/* ---------------------------------------------------------------------------
 */
void cDFBVideoOut::GetLockOsdSurface(uint8_t *&osd, int &stride,
                                     bool *&DirtyLines)
{
  int               pitch;
  uint8_t           *dst;

  dirtyLines = DirtyLines = NULL;
  osd = NULL;
  stride = 0;

  if (!videoInitialized)
    return;

  try
  {
    dirtyLines=DirtyLines=new bool[Yres];
    memset(dirtyLines,false,sizeof(dirtyLines));

    tmpOsdSurface = (useStretchBlit) ? osdSurface : scrSurface;
    tmpOsdSurface->Lock(DSLF_WRITE, (void **)&dst, &pitch) ;
    osd=dst;stride=pitch;
  }
  catch (DFBException *ex)
  {
    fprintf (stderr,"[dfb] Refresh: action=%s, result=%s\n",
        ex->GetAction(), ex->GetResult());
    delete ex;
  }
}

/* ---------------------------------------------------------------------------
 */
void cDFBVideoOut::CommitUnlockOsdSurface()
{
  if (!videoInitialized || !dirtyLines || !tmpOsdSurface)
    return;

  //printf("CommitUnlockOsdSurface %p\n",tmpOsdSurface);fflush(stdout);
  try
  {
    DFBRectangle      osdsrc;

    tmpOsdSurface->Unlock();
    tmpOsdSurface->Flip();

    int miny=0;
    int maxy=0;
    tmpOsdSurface->SetBlittingFlags(DSBLIT_NOFX);
    do {
      while (!dirtyLines[miny] && miny < Yres)
        miny++;

      if (miny >= Yres)
        break;

      maxy=miny;
      while (dirtyLines[maxy] && maxy < Yres)
        maxy++;

      osdsrc.x = 0;
      osdsrc.y = miny;
      osdsrc.w = Xres;
      osdsrc.h = maxy-miny + 1;
      tmpOsdSurface->Blit(tmpOsdSurface,&osdsrc,0,miny);

      //printf("CommitUnlockOsdSurface3 %p\n",tmpOsdSurface);fflush(stdout);
      miny=maxy;

    } while ( miny<Yres);
  }
  catch (DFBException *ex)
  {
    fprintf (stderr,"[dfb] Refresh: action=%s, result=%s\n",
        ex->GetAction(), ex->GetResult());
    delete ex;
  }
  //printf("CommitUnlockOsdSurface %p 4\n",tmpOsdSurface);fflush(stdout);
  free(dirtyLines);
  dirtyLines=NULL;
  tmpOsdSurface=NULL;
  cVideoOut::CommitUnlockOsdSurface();
}

/* ---------------------------------------------------------------------------
 */
void cDFBVideoOut::GetOSDMode(int &Depth, bool &HasAlpha, bool &AlphaInversed,
                              bool &IsYUV, uint8_t *&PixelMask)
{
  Depth=Bpp;
  HasAlpha=!OSDpseudo_alpha;
  AlphaInversed=isVIAUnichrome;
  IsYUV=false;
  PixelMask=NULL;
}

/* ---------------------------------------------------------------------------
 */
void cDFBVideoOut::GetOSDDimension( int &Width, int &Height, int &xPan, int &yPan)
{
  Width=Xres;
  Height=Yres;
  xPan = yPan = 0;
}

#else

/* ---------------------------------------------------------------------------
 */
void cDFBVideoOut::Refresh()
{
    int               pitch;
    uint8_t           *dst;
    IDirectFBSurface  *tmpSurface;

  if (!videoInitialized)
    return;

  tmpSurface = (useStretchBlit) ? osdSurface : scrSurface;

  try
  {
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
  catch (DFBException *ex)
  {
    fprintf (stderr,"[dfb] Refresh: action=%s, result=%s\n",
             ex->GetAction(), ex->GetResult());
    delete ex;
  }
}
#endif

/* ---------------------------------------------------------------------------
 */
void cDFBVideoOut::CloseOSD()
{
    IDirectFBSurface  *tmpSurface;

  if (!videoInitialized)
    return;

  tmpSurface = (useStretchBlit) ? osdSurface : scrSurface;

  try
  {
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
  catch (DFBException *ex)
  {
    fprintf (stderr,"[dfb] CloseOSD: action=%s, result=%s\n",
             ex->GetAction(), ex->GetResult());
    delete ex;
  }
}

/* ---------------------------------------------------------------------------
 */
void cDFBVideoOut::ShowOSD ()
{
  if (!videoInitialized)
    return;

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

    try
    {
      if (clearBackground) {
        scrSurface->Clear(0,0,0,0);
        clearBackground--;
      }

#if HAVE_DSBLIT_INTERLACED
      if (setupStore->useMGAtv)
      {
        scrSurface->SetBlittingFlags(DSBLIT_INTERLACED);
        scrSurface->StretchBlit(videoSurface, &src, &dst);
      }
      else
      {
        scrSurface->SetBlittingFlags(DSBLIT_NOFX);
        scrSurface->StretchBlit(videoSurface, &src, &dst);
      }
#else
      scrSurface->SetBlittingFlags(DSBLIT_NOFX);
      scrSurface->StretchBlit(videoSurface, &src, &dst);
#endif
      if (OSDpresent)
      {
        osdsrc.x = osdsrc.y = 0;
        osdsrc.w = Xres;osdsrc.h=Yres;
        scrSurface->SetBlittingFlags(DSBLIT_BLEND_ALPHACHANNEL);
        scrSurface->Blit(osdSurface, &osdsrc, 0, 0);
      }
      scrSurface->Flip(NULL, DSFLIP_WAITFORSYNC);

    }
    catch (DFBException *ex)
    {
      fprintf (stderr,"[dfb] ShowOSD: action=%s, result=%s\n",
               ex->GetAction(), ex->GetResult());
      delete ex;
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

  if (!videoInitialized)
    return;

  SetParams();

  //fprintf(stderr,"[dfb] draw frame (%d x %d) Y: %d UV: %d\n", Width, Height, Ystride, UVstride);
  try
  {
    videoSurface->Lock(DSLF_WRITE, (void **)&dst, &pitch);
    if (pixelformat == DSPF_I420 || pixelformat == DSPF_YV12)
    {
#if HAVE_SetSourceLocation
      Py += Ystride  * cutTop * 2 + cutLeft * 2;
      Pv += UVstride * cutTop + cutLeft;
      Pu += UVstride * cutTop + cutLeft;

      dst += pitch * cutTop * 2;

      for(hi=cutTop*2; hi < Height-cutBottom*2; hi++){
        fast_memcpy(dst + cutLeft * 2, Py, Width - (cutLeft + cutRight) * 2);
        Py  += Ystride;
        dst += pitch;
      }

      dst += pitch * cutBottom * 2 + pitch * cutTop / 2;

      for(hi=cutTop; hi < Height/2-cutBottom; hi++) {
        fast_memcpy(dst + cutLeft, Pu, Width/2 - (cutLeft + cutRight));
        Pu  += UVstride;
        dst += pitch / 2;
      }

      dst += pitch * cutBottom / 2 + pitch * cutTop / 2;

      for(hi=cutTop; hi < Height/2-cutBottom; hi++) {
        fast_memcpy(dst + cutLeft, Pv, Width/2 - (cutLeft + cutRight));
        Pv  += UVstride;
        dst += pitch / 2;
      }
#else
      Py += (Ystride  * syoff)   + Ystride  * cutTop * 2;
      Pv += (UVstride * syoff/2) + UVstride * cutTop;
      Pu += (UVstride * syoff/2) + UVstride * cutTop;

      dst += pitch * cutTop * 2;

      for(hi=cutTop*2; hi < sheight-cutBottom*2; hi++){
        fast_memcpy(dst+cutLeft*2, Py+sxoff+cutLeft*2, swidth-(cutLeft+cutRight)*2);
        Py  += Ystride;
        dst += pitch;
      }

      dst += pitch * cutBottom * 2 + pitch * cutTop / 2;

      for(hi=cutTop; hi < sheight/2-cutBottom; hi++) {
        fast_memcpy(dst+cutLeft, Pu+sxoff/2+cutLeft, swidth/2-(cutLeft+cutRight));
        Pu  += UVstride;
        dst += pitch / 2;
      }

      dst += pitch * cutBottom / 2 + pitch * cutTop / 2;

      for(hi=cutTop; hi < sheight/2-cutBottom; hi++) {
        fast_memcpy(dst+cutLeft, Pv+sxoff/2+cutLeft, swidth/2-(cutLeft+cutRight));
        Pv  += UVstride;
        dst += pitch / 2;
      }
#endif
    } else if (pixelformat == DSPF_YUY2) {

      if (interlaceMode) {
        yv12_to_yuy2_il_c(Py + Ystride  * cutTop * 2 + cutLeft * 2,
                          Pu + UVstride * cutTop + cutLeft,
                          Pv + UVstride * cutTop + cutLeft,
                          dst + pitch * cutTop * 2 + cutLeft * 4,
                          Width - 2 * (cutLeft + cutRight),
                          Height - 2 * (cutTop + cutBottom),
                          Ystride, UVstride, pitch);
      } else {
        yv12_to_yuy2(Py + Ystride  * cutTop * 2 + cutLeft * 2,
                     Pu + UVstride * cutTop + cutLeft,
                     Pv + UVstride * cutTop + cutLeft,
                     dst + pitch * cutTop * 2 + cutLeft * 4,
                     Width - 2 * (cutLeft + cutRight),
                     Height - 2 * (cutTop + cutBottom),
                     Ystride, UVstride, pitch);
      }
     }

    videoSurface->Unlock();

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

#if HAVE_DSBLIT_INTERLACED
      if (setupStore->useMGAtv)
      {
        scrSurface->SetBlittingFlags(DSBLIT_INTERLACED);
        scrSurface->StretchBlit(videoSurface, &src, &dst);
      } else {
        scrSurface->SetBlittingFlags(DSBLIT_NOFX);
        scrSurface->StretchBlit(videoSurface, &src, &dst);
      }
#else
      scrSurface->SetBlittingFlags(DSBLIT_NOFX);
      scrSurface->StretchBlit(videoSurface, &src, &dst);
#endif

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
      if (setupStore->tripleBuffering)
        videoSurface->Flip(/*NULL, DSFLIP_ONSYNC*/);
      else
        videoSurface->Flip(NULL, DSFLIP_ONSYNC);
    }
  }
  catch (DFBException *ex)
  {
    fprintf (stderr, "[dfb] YUV: action=%s, result=%s\n",
             ex->GetAction(), ex->GetResult());
    delete ex;
  }
}

/* ---------------------------------------------------------------------------
 */
cDFBVideoOut::~cDFBVideoOut()
{
  fprintf(stderr,"Releasing DFB\n");

  setupStore->osdMode = prevOsdMode;

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
