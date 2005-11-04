/*
 * setup-softdevice.c
 *
 * See the README file for copyright information and how to reach the authors.
 *
 * $Id: setup-softdevice.c,v 1.35 2005/11/04 21:36:12 lucke Exp $
 */

#include "video.h"
#include "setup-softdevice.h"
#include "mpeg2decoder.h"

#define MINAVOFFSET (-250)
#define MAXAVOFFSET (250)

#define MAX_CROP_LINES  50
#define MAX_CROP_COLS   50

/* ----------------------------------------------------------------------------
 * index to this array correspond to AFD values
 */
#define SETUP_CROPMODES 4
const char *crop_str[SETUP_CROPMODES+1];

/* ---------------------------------------------------------------------------
 */
#define SETUP_DEINTMODES 9
const char *deint_str[SETUP_DEINTMODES] = {
        "none",      // translated in cMenuSetupSoftdevice::cMenuSetupSoftdevice()
        "lavc",      // no need to translate
#ifdef FB_SUPPORT
        "FB-intern", // no need to translate
#endif        
#ifdef PP_LIBAVCODEC
        "linblend",  // no need to translate
        "linipol",   // no need to translate
        "cubicipol", // no need to translate
        "median",    // no need to translate
        "ffmpeg",    // no need to translate
#endif //PP_LIBAVCODEC
        NULL
     };

/*-----------------------------------------------------------------------------
 */
#define SETUP_PPMODES 4
const char *pp_str[SETUP_PPMODES];

/* ----------------------------------------------------------------------------
 * allow changing of output pixfmt
 */
#define SETUP_PIXFMT 4
const char *pix_fmt[SETUP_PIXFMT];

/* ----------------------------------------------------------------------------
 * give some readable values for aspect ratio selection instead fo 0, 1 values
 */
#define SETUP_XVSTARTUPASPECT 3
const char *xv_startup_aspect[SETUP_XVSTARTUPASPECT];

/*-----------------------------------------------------------------------------
 */
#define SETUP_SUSPENDVIDEO 3
const char *suspendVideo[SETUP_SUSPENDVIDEO];

/*-----------------------------------------------------------------------------
 */
#define SETUP_OSDMODES 3
const char *osdModeNames[SETUP_OSDMODES];

/*-----------------------------------------------------------------------------
 */
#define SETUP_VIDEOASPECTNAMES 6
const char *videoAspectNames[SETUP_VIDEOASPECTNAMES];

/*-----------------------------------------------------------------------------
 */
#define SETUP_BUFFERMODES 4
const char *bufferModes[SETUP_BUFFERMODES];

/*-----------------------------------------------------------------------------
 */
#define SETUP_AC3MODENAMES 5
const char *ac3ModeNames[SETUP_AC3MODENAMES];

/*-----------------------------------------------------------------------------
 */
#define SETUP_USERKEYS 11
const char *userKeyUsage[SETUP_USERKEYS];

#define SETUP_SYNC_TIMER_NAMES  4
const char *syncTimerNames[SETUP_SYNC_TIMER_NAMES];

/* ----------------------------------------------------------------------------
 */
static inline int clamp (int min, int val, int max)
{
  if (val < min)
    return min;
  if (val > max)
    return max;
  return val;
}

/* ----------------------------------------------------------------------------
 */
cSetupStore setupStore;

cSetupStore::cSetupStore ()
{
  xvAspect      = 1;   // XV_FORMAT_NORMAL;
  xvMaxArea     = 0;
  outputMethod  = 0;
  cropMode      = 0;
  cropModeToggleKey = 0;
  cropTopLines      = 0;
  cropBottomLines   = 0;
  cropLeftCols      = 0;
  cropRightCols     = 0;
  deintMethod   = 0;
  ppMethod   = 0;
  ppQuality   = 0;
  syncOnFrames  = 0;
  avOffset      = 0;
  shouldSuspend = 0;
  ac3Mode       = 0;
  useMGAtv      = 0;
  useStretchBlit = 0;
  bufferMode    = 0;
  mainMenu  = 1;
  syncTimerMode = 2;

  /* --------------------------------------------------------------------------
   * these screen width/height values are operating in square pixel mode.
   * for non square pixel mode should be set via osd to 720/576
   */
  screenPixelAspect   = 0;

  strcpy (alsaDevice, "");
  strcpy (alsaAC3Device, "");
  voArgs = aoArgs = NULL;

  xv_startup_aspect[0] = tr("16:9 wide");
  xv_startup_aspect[1] = tr("4:3 normal");
  xv_startup_aspect[2] = NULL;

  deint_str[0] = tr("none");

  pp_str[0] = tr("none");
  pp_str[1] = tr("fast");
  pp_str[2] = tr("default");
  pp_str[3] = NULL;

  bufferModes[0] = tr("save");
  bufferModes[1] = tr("good seeking");
  bufferModes[2] = tr("HDTV");
  bufferModes[3] = NULL;

  pix_fmt[0] = "I420"; // no need to translate
  pix_fmt[1] = "YV12"; // no need to translate
  pix_fmt[2] = "YUY2"; // no need to translate
  pix_fmt[3] = NULL;

  crop_str[0] = tr("none");
  crop_str[1] = "4:3";  // no need to translate
  crop_str[2] = "16:9"; // no need to translate
  crop_str[3] = "14:9"; // no need to translate
  crop_str[4] = NULL;

  userKeyUsage[0] = tr("none");
  userKeyUsage[1] = "User1"; // no need to translate
  userKeyUsage[2] = "User2"; // no need to translate
  userKeyUsage[3] = "User3"; // no need to translate
  userKeyUsage[4] = "User4"; // no need to translate
  userKeyUsage[5] = "User5"; // no need to translate
  userKeyUsage[6] = "User6"; // no need to translate
  userKeyUsage[7] = "User7"; // no need to translate
  userKeyUsage[8] = "User8"; // no need to translate
  userKeyUsage[9] = "User9"; // no need to translate
  userKeyUsage[10] = NULL;

  videoAspectNames[0] = tr("default");
  videoAspectNames[1] = "5:4";   // no need to translate
  videoAspectNames[2] = "4:3";   // no need to translate
  videoAspectNames[3] = "16:9";  // no need to translate
  videoAspectNames[4] = "16:10"; // no need to translate
  videoAspectNames[5] = NULL;

  suspendVideo[0] = tr("playing");
  suspendVideo[1] = tr("suspended");
  suspendVideo[2] = NULL;

  osdModeNames[0] = tr("pseudo");
  osdModeNames[1] = tr("software");
  osdModeNames[2] = NULL;

  ac3ModeNames[0] = "Stereo (2CH)";     // no need to translate?
  ac3ModeNames[1] = "5.1 S/P-DIF";      // no need to translate?
  ac3ModeNames[2] = "5.1 Analog (4CH)"; // no need to translate?
  ac3ModeNames[3] = "5.1 Analog (6CH)"; // no need to translate?
  ac3ModeNames[4] = NULL;

  syncTimerNames[0] = "usleep";
  syncTimerNames[1] = "rtc";
  syncTimerNames[2] = "sig";
  syncTimerNames[3] = NULL;
}

bool cSetupStore::SetupParse(const char *Name, const char *Value)
{
  if (!strcasecmp(Name,"Deinterlace Method")) {
    deintMethod = atoi(Value);

#ifdef FB_SUPPORT
  #ifdef PP_LIBAVCODEC
    deintMethod = clamp (0, deintMethod, 7);
  #else
    deintMethod = clamp (0, deintMethod, 2);
  #endif //PP_LIBAVCODEC
#else
  #ifdef PP_LIBAVCODEC
    deintMethod = clamp (0, deintMethod, 6);
  #else
    deintMethod = clamp (0, deintMethod, 1);
  #endif //PP_LIBAVCODEC
#endif //FB_SUPPORT

    fprintf (stderr,
            "[setup-softdevice] deinterlace method set to %d %s\n",
            deintMethod,
            deint_str [deintMethod]);
  }
#ifdef PP_LIBAVCODEC
  else if (!strcasecmp(Name,"Postprocess Method")) {
            ppMethod=atoi(Value); 
            ppMethod=clamp(0,ppMethod,2);
  }  else if (!strcasecmp(Name,"Postprocess Quality")) {
            ppQuality=atoi(Value); 
            ppQuality=clamp(0,ppQuality,6);
  }
#endif
  else if(!strcasecmp(Name,"bufferMode")) {
        bufferMode=atoi(Value);
        bufferMode=clamp(0,bufferMode,2);
  }
  else if(!strcasecmp(Name,"CropMode")) {
    cropMode = atoi(Value);
    cropMode = clamp (0, cropMode, (SETUP_CROPMODES-1));
    fprintf (stderr, "[setup-softdevice] cropping mode set to %d (%s)\n",
             cropMode,
             crop_str [cropMode]);
  } else if(!strcasecmp(Name,"CropModeToggleKey")) {
    cropModeToggleKey = atoi(Value);
    cropModeToggleKey = clamp (0, cropModeToggleKey, 9);
    fprintf (stderr,
             "[setup-softdevice] cropping mode toggle key set to %d (%s)\n",
             cropModeToggleKey,
             userKeyUsage [cropModeToggleKey]);
  } else if(!strcasecmp(Name,"CropTopLines")) {
    cropTopLines = atoi(Value);
    cropTopLines = clamp (0, cropTopLines, MAX_CROP_LINES);
    fprintf(stderr,"[setup-softdevice] Cropping %d lines from top\n",
            cropTopLines);
  } else if(!strcasecmp(Name,"CropBottomLines")) {
    cropBottomLines = atoi(Value);
    cropBottomLines = clamp (0, cropBottomLines, MAX_CROP_LINES);
    fprintf(stderr,"[setup-softdevice] Cropping %d lines from bottom\n",
            cropBottomLines);
  } else if(!strcasecmp(Name,"CropLeftCols")) {
    cropLeftCols = atoi(Value);
    cropLeftCols = clamp (0, cropLeftCols, MAX_CROP_COLS);
    fprintf(stderr,"[setup-softdevice] Cropping %d columns from left\n",
            cropLeftCols);
  } else if(!strcasecmp(Name,"CropRightCols")) {
    cropRightCols = atoi(Value);
    cropRightCols = clamp (0, cropRightCols, MAX_CROP_COLS);
    fprintf(stderr,"[setup-softdevice] Cropping %d columns from right\n",
            cropRightCols);
  } else if (!strcasecmp(Name,"PixelFormat")) {
    pixelFormat = atoi(Value);
    pixelFormat = clamp (0, pixelFormat, 2);

    fprintf (stderr,
             "[setup-softdevice] pixel format set to (%s)\n",
             pix_fmt [pixelFormat]);
  } else if(!strcasecmp(Name, "Xv-Aspect")) {
    xvAspect = atoi(Value);
    xvAspect = clamp (0, xvAspect, 1);

    fprintf (stderr,
             "[setup-softdevice] startup aspect set to (%s)\n",
             xv_startup_aspect [xvAspect]);

  } else if(!strcasecmp(Name, "Xv-MaxArea")) {
    /* ------------------------------------------------------------------------
     * ignore that on setup load as it would override commandline settings
     */
    //xvMaxArea = atoi(Value);
    //xvMaxArea = clamp (0, xvMaxArea, 1);
    //fprintf (stderr,
    //         "[setup-softdevice] using max area (%s)\n",
    //         (xvMaxArea) ? "YES" : "NO");
    ; // empty statement
  } else if(!strcasecmp(Name, "Picture mirroring")) {
    mirror = atoi(Value);
    fprintf(stderr,"[softdevice] picture mirroring set to %d (%s)\n",
            mirror,
            mirror ? "on" : "off");
  } else if(!strcasecmp(Name, "UseStretchBlit")) {
    useStretchBlit = clamp (0, atoi(Value), 1);
    fprintf(stderr,"[softdevice] UseStretchBlitset to %s\n",
            useStretchBlit ? "on" : "off");
  } else if (!strcasecmp(Name, "SyncAllFrames")) {
    syncOnFrames = atoi(Value);
    syncOnFrames = clamp (0, syncOnFrames, 1);
  } else if (!strcasecmp(Name, "avOffset")) {
    avOffset = atoi(Value);
    avOffset = clamp (MINAVOFFSET, avOffset, MAXAVOFFSET);
    fprintf(stderr,"[setup-softdevice] A/V Offset set to (%d)\n",
            avOffset);
  } else if (!strcasecmp(Name, "AlsaDevice") && strlen(alsaDevice) == 0) {
    strncpy(alsaDevice, Value, ALSA_DEVICE_NAME_LENGTH);
    alsaDevice [ALSA_DEVICE_NAME_LENGTH-1] = 0;
    fprintf(stderr, "[setup-softdevice] alsa device set to: %s\n", alsaDevice);
  } else if (!strcasecmp(Name, "PixelAspect")) {
    screenPixelAspect = atoi (Value);
    screenPixelAspect = clamp (0, screenPixelAspect, 4);
  } else if (!strcasecmp(Name, "OSDalphablend")) {
    osdMode = atoi (Value);
    osdMode = clamp (0, osdMode, 1);
    fprintf(stderr,"[setup-softdevice] setting alpha blend mode to %s\n",
            osdModeNames[osdMode]);
  } else if (!strcasecmp(Name, "Suspend")) {
    shouldSuspend = atoi (Value);
    fprintf(stderr, "[setup-softdevice] shouldSuspend to: %d\n", shouldSuspend);
    shouldSuspend = clamp (0, shouldSuspend, 1);
  } else if (!strcasecmp(Name, "AC3Mode")) {
    ac3Mode = atoi (Value);
    fprintf(stderr, "[setup-softdevice] alsa ac3Mode set to: %d\n", ac3Mode);
    ac3Mode = clamp (0, ac3Mode, 3);
  } else if (!strcasecmp(Name, "AlsaAC3Device") && strlen(alsaAC3Device) == 0) {
    strncpy(alsaAC3Device, Value, ALSA_DEVICE_NAME_LENGTH);
    alsaAC3Device [ALSA_DEVICE_NAME_LENGTH-1] = 0;
    fprintf(stderr, "[setup-softdevice] alsa AC3 device set to: %s\n",
            alsaAC3Device);
  } else if (!strcasecmp(Name, "mainMenu")) {
    mainMenu = atoi (Value);
    mainMenu = clamp (0, mainMenu, 1);
    fprintf(stderr, "[setup-softdevice] mainMenu: %d\n", mainMenu);
  } else if (!strcasecmp(Name, "syncTimerMode")) {
    syncTimerMode = atoi (Value);
    syncTimerMode = clamp (0, syncTimerMode, 2);
    fprintf(stderr, "[setup-softdevice] syncTimerMode: %s\n",
            syncTimerNames[syncTimerMode]);
  }  else
    return false;

  return true;
}

/* ---------------------------------------------------------------------------
 */
char *cSetupStore::getPPdeintValue(void)
{
  if (strcmp(deint_str[deintMethod], "linblend") == 0) return "lb";
  else if (strcmp(deint_str[deintMethod], "linipol") == 0) return "li";
  else if (strcmp(deint_str[deintMethod], "cubicipol") == 0) return "ci";
  else if (strcmp(deint_str[deintMethod], "median") == 0) return "md";
  else if (strcmp(deint_str[deintMethod], "ffmpeg") == 0) return "fd";
  else return NULL;
}

/* ---------------------------------------------------------------------------
 */
char *cSetupStore::getPPValue(void)
{
#ifdef PP_LIBAVCODEC
  if (strcmp(pp_str[ppMethod], tr("none")) == 0) return "";
  else if (strcmp(pp_str[ppMethod], tr("fast")) == 0) return "fa";
  else if (strcmp(pp_str[ppMethod], tr("default")) == 0) return "de";
#endif
  return NULL;
}

/* ---------------------------------------------------------------------------
 */
void cSetupStore::CropModeNext(void)
{
  cropMode = (cropMode == (SETUP_CROPMODES-1)) ? 0 : cropMode + 1;
}

/* ---------------------------------------------------------------------------
 */
bool cSetupStore::CatchRemoteKey(const char *remoteName, uint64 key)
{
    char  buffer[32];
    eKeys keySym;

  snprintf(buffer, sizeof(buffer), "%016LX", (uint64) key);
  keySym = Keys.Get(remoteName, buffer);
  if (keySym >= kUser1 && keySym <= kUser9)
  {
    keySym = (eKeys) (keySym - kUser1 + 1);
    if (cropModeToggleKey && cropModeToggleKey == keySym)
    {
      CropModeNext();
      return true;
    }
  }
  return false;
}

/* ---------------------------------------------------------------------------
 */
cMenuSetupCropping::cMenuSetupCropping(const char *name) : cOsdMenu(name, 33)
{
  copyData = setupStore;
  data = &setupStore;

  crop_str[0] = tr("none");
  Add(new cMenuEditStraItem(tr("CropMode"),
                            &data->cropMode,
                            SETUP_CROPMODES,
                            crop_str));

  userKeyUsage[0] = tr("none");
  Add(new cMenuEditStraItem(tr("CropModeToggleKey"),
                            &data->cropModeToggleKey,
                            (SETUP_USERKEYS-1),
                            userKeyUsage));

  if (data->outputMethod != VOUT_FB)
  {
    Add(new cOsdItem(" ", osUnknown, false));

    Add(new cMenuEditIntItem(tr("Crop lines from top"),
                             &data->cropTopLines,
                             0,
                             MAX_CROP_LINES));

    Add(new cMenuEditIntItem(tr("Crop lines from bottom"),
                             &data->cropBottomLines,
                             0,
                             MAX_CROP_LINES));
  }

  if (data->outputMethod == VOUT_XV || data->outputMethod == VOUT_DFB)
  {
    Add(new cMenuEditIntItem(tr("Crop columns from left"),
                             &data->cropLeftCols,
                             0,
                             MAX_CROP_COLS));
    Add(new cMenuEditIntItem(tr("Crop columns from right"),
                             &data->cropRightCols,
                             0,
                             MAX_CROP_COLS));
  }
}

/* ---------------------------------------------------------------------------
 */
eOSState cMenuSetupCropping::ProcessKey(eKeys Key)
{
    eOSState state = cOsdMenu::ProcessKey(Key);

  switch (state)
  {
    case osUnknown:
      switch (Key)
      {
        case kOk:
          state = osBack;
          break;
        default:
          break;
      }
      break;
    case osBack:
      setupStore = copyData;
      fprintf (stderr, "[setup-cropping] restoring setup state\n");
      break;
    default:
      break;
  }
  return state;
}

/* ---------------------------------------------------------------------------
 */
cMenuSetupPostproc::cMenuSetupPostproc(const char *name) : cOsdMenu(name, 33)
{
  copyData = setupStore;
  data = &setupStore;

  deint_str[0] = tr("none");
  if (data->outputMethod == VOUT_FB)
  {
    Add(new cMenuEditStraItem(tr("Deinterlace Method"),
                              &data->deintMethod,
#ifdef PP_LIBAVCODEC
                              8,
#else
                              3,
#endif //PP_LIBAVCODEC
                              deint_str));
  }
  else
  {
    Add(new cMenuEditStraItem(tr("Deinterlace Method"),
                              &data->deintMethod,
#ifdef PP_LIBAVCODEC
                              7,
#else
                              2,
#endif //PP_LIBAVCODEC
                              deint_str));
  }

#ifdef PP_LIBAVCODEC
  pp_str[0] = tr("none");
  pp_str[1] = tr("fast");
  pp_str[2] = tr("default");
  Add(new cMenuEditStraItem(tr("Postprocessing Method"),
                              &data->ppMethod,(SETUP_PPMODES-1),pp_str));
  Add(new cMenuEditIntItem(tr("Postprocessing Quality"),
                              &data->ppQuality,0,6));
#endif

  Add(new cMenuEditBoolItem(tr("Picture mirroring"),
                            &data->mirror, tr("off"), tr("on")));

}

/* ---------------------------------------------------------------------------
 */
eOSState cMenuSetupPostproc::ProcessKey(eKeys Key)
{
    eOSState state = cOsdMenu::ProcessKey(Key);

  switch (state)
  {
    case osUnknown:
      switch (Key)
      {
        case kOk:
          state = osBack;
          break;
        default:
          break;
      }
      break;
    case osBack:
      setupStore = copyData;
      fprintf (stderr, "[setup-postproc] restoring setup state\n");
      break;
    default:
      break;
  }
  return state;
}

/* ---------------------------------------------------------------------------
 */
cMenuSetupSoftdevice::cMenuSetupSoftdevice(cPlugin *plugin)
{
  if (plugin)
    SetPlugin(plugin);

  copyData = setupStore;
  data = &setupStore;

  Add(new cOsdItem(tr("Cropping")));
  Add(new cOsdItem(tr("Post processing")));
  Add(new cOsdItem(" ", osUnknown, false));

  if (data->outputMethod == VOUT_XV)
  {
    xv_startup_aspect[0] = tr("16:9 wide");
    xv_startup_aspect[1] = tr("4:3 normal");
    Add(new cMenuEditStraItem(tr("Xv startup aspect"),
                             &data->xvAspect,
                             (SETUP_XVSTARTUPASPECT-1),
                             xv_startup_aspect));
  }

  videoAspectNames[0] = tr("default");
  Add(new cMenuEditStraItem(tr("Screen Aspect"),
                            &data->screenPixelAspect,
                            (SETUP_VIDEOASPECTNAMES-1),
                            videoAspectNames));

  osdModeNames[0] = tr("pseudo");
  osdModeNames[1] = tr("software");
  Add(new cMenuEditStraItem(tr("OSD alpha blending"),
                            &data->osdMode,
                            (SETUP_OSDMODES-1),
                            osdModeNames));

  Add(new cOsdItem(" ", osUnknown, false));

  bufferModes[0] = tr("save");
  bufferModes[1] = tr("good seeking");
  bufferModes[2] = tr("HDTV");
  Add(new cMenuEditStraItem(tr("Buffer Mode"),
                              &data->bufferMode,(SETUP_BUFFERMODES-1),bufferModes));

  suspendVideo[0] = tr("playing");
  suspendVideo[1] = tr("suspended");
  Add(new cMenuEditStraItem(tr("Playback"),
                            &data->shouldSuspend,
                            (SETUP_SUSPENDVIDEO-1),
                            suspendVideo));

  Add(new cOsdItem(" ", osUnknown, false));

  Add(new cMenuEditIntItem(tr("A/V Delay"),
                           &data->avOffset,
                           MINAVOFFSET, MAXAVOFFSET));

  Add(new cMenuEditStraItem(tr("AC3 Mode"),
                            &data->ac3Mode,
                            (SETUP_AC3MODENAMES-1),
                            ac3ModeNames));

  Add(new cMenuEditStraItem(tr("Sync Mode"),
                            &data->syncTimerMode,
                            (SETUP_SYNC_TIMER_NAMES-1),
                            syncTimerNames));

  Add(new cOsdItem(" ", osUnknown, false));

  if (data->outputMethod == VOUT_DFB || data->outputMethod == VOUT_VIDIX)
  {
    Add(new cMenuEditStraItem(tr("Pixel Format"),
                              &data->pixelFormat,
                              (SETUP_PIXFMT-1),
                              pix_fmt));
  }

  if (data->outputMethod == VOUT_DFB)
  {
    Add(new cMenuEditBoolItem(tr("Use StretchBlit"),
                              &data->useStretchBlit, tr("off"), tr("on")));
  }

  Add(new cMenuEditBoolItem(tr("Hide main menu entry"),
                            &data->mainMenu, tr("yes"), tr("no")));
}

/* ---------------------------------------------------------------------------
 */
eOSState cMenuSetupSoftdevice::ProcessKey(eKeys Key)
{
    eOSState state = cOsdMenu::ProcessKey(Key);

  switch (state)
  {
    case osUnknown:
      switch (Key)
      {
        case kOk:
          if (!strcmp(Get(Current())->Text(),tr("Cropping")))
          {
            return AddSubMenu (new cMenuSetupCropping(tr("Cropping")));
          }
          else if (!strcmp(Get(Current())->Text(),tr("Post processing")))
          {
            return AddSubMenu (new cMenuSetupPostproc(tr("Post processing")));
          }
          Store();
          state = osBack;
          break;
        default:
          break;
      }
      break;
    case osBack:
      setupStore = copyData;
      fprintf (stderr, "[setup-softdevice] restoring setup state\n");
      break;
    default:
      break;
  }
  return state;
}

/* ---------------------------------------------------------------------------
 */
void cMenuSetupSoftdevice::Store(void)
{
#if 0
  if (setupStore.deintMethod != data.deintMethod) {
    fprintf(stderr,
            "[setup-softdevice] deinterlace method changed to (%d) %s\n",
            data.deintMethod, deint_str [data.deintMethod]);
  }
#endif


  fprintf (stderr, "[setup-softdevice] storing data\n");
//  setupStore = data;
  SetupStore ("Xv-Aspect",          setupStore.xvAspect);
  // don't save max area value as it is ignored on load
  //SetupStore ("Xv-MaxArea",         setupStore.xvMaxArea);
  SetupStore ("CropMode",           setupStore.cropMode);
  SetupStore ("CropModeToggleKey",     setupStore.cropModeToggleKey);
  SetupStore ("CropTopLines",        setupStore.cropTopLines);
  SetupStore ("CropBottomLines",     setupStore.cropBottomLines);
  SetupStore ("CropLeftCols",        setupStore.cropLeftCols);
  SetupStore ("CropRightCols",       setupStore.cropRightCols);
  SetupStore ("Deinterlace Method", setupStore.deintMethod);
  SetupStore ("Postprocess Method", setupStore.ppMethod);
  SetupStore ("Postprocess Quality", setupStore.ppQuality);
  SetupStore ("PixelFormat",        setupStore.pixelFormat);
  SetupStore ("UseStretchBlit",     setupStore.useStretchBlit);
  SetupStore ("Picture mirroring",  setupStore.mirror);
  SetupStore ("avOffset",           setupStore.avOffset);
  SetupStore ("AlsaDevice",         setupStore.alsaDevice);
  SetupStore ("AlsaAC3Device",      setupStore.alsaAC3Device);
  SetupStore ("PixelAspect",        setupStore.screenPixelAspect);
  SetupStore ("Suspend",            setupStore.shouldSuspend);
  SetupStore ("OSDalphablend",      setupStore.osdMode);
  SetupStore ("AC3Mode",            setupStore.ac3Mode);
  SetupStore ("bufferMode",           setupStore.bufferMode);
  SetupStore ("mainMenu",             setupStore.mainMenu);
  SetupStore ("syncTimerMode",        setupStore.syncTimerMode);
}
