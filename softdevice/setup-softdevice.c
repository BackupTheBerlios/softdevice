/*
 * setup-softdevice.c
 *
 * See the README file for copyright information and how to reach the authors.
 *
 * $Id: setup-softdevice.c,v 1.19 2005/06/12 20:45:20 wachm Exp $
 */

#include "video.h"
#include "setup-softdevice.h"
#include "mpeg2decoder.h"

#define MINAVOFFSET (-250)
#define MAXAVOFFSET (250)

/* ----------------------------------------------------------------------------
 * index to this array correspond to AFD values
 */
char *crop_str [] = {
        "none",
        "4:3",
        "16:9",
        "14:9",
        NULL
     };
#define CROPMODEMAX 3
#define CROPMODES (CROPMODEMAX+1)

/* ---------------------------------------------------------------------------
 */
char *deint_str [] = {
        "none",
        "lavc",
#ifdef FB_SUPPORT
        "FB-intern",
#endif        
#ifdef PP_LIBAVCODEC
        "linblend",
        "linipol",
        "cubicipol",
        "median",
        "ffmpeg",
#endif //PP_LIBAVCODEC
        NULL
     };

/*-----------------------------------------------------------------------------
*/
#ifdef PP_LIBAVCODEC
char *pp_str[]={
        "none",
        "fast",
        "default",
	NULL
	};
#endif //PP_LIBAVCODEC

/* ----------------------------------------------------------------------------
 * allow changing of output pixfmt
 */
char *pix_fmt [] = {
        "I420",
        "YV12",
        "YUV2",
        NULL
     };

/* ----------------------------------------------------------------------------
 * give some readable values for aspect ratio selection instead fo 0, 1 values
 */
char *xv_startup_aspect [] = {
        "16:9 wide",
        "4:3 normal",
        NULL
     };

/* ----------------------------------------------------------------------------
 */
char *suspendVideo [] = {
        "playing",
        "suspended",
        NULL
     };

/* ----------------------------------------------------------------------------
 */
char *osdMode [] = {
        "pseudo",
        "software",
        NULL
     };

/* ----------------------------------------------------------------------------
 */
char *videoAspectNames [] = {
        "Default",
        "5:4",
        "4:3",
        "16:9",
        "16:10",
        NULL
     };
/* ----------------------------------------------------------------------------
 */
char *bufferModes [] = {
        "save",
        "good seeking",
        "HDTV",
        NULL
     };

/* ----------------------------------------------------------------------------
 */
char *ac3ModeNames [] = {
        "Stereo (2CH)",
        "5.1 S/P-DIF",
        "5.1 Analog (4CH)",
        "5.1 Analog (6CH)",
        NULL
     };

/* ----------------------------------------------------------------------------
 */
char *userKeyUsage [] = {
        "none",
        "User1",
        "User2",
        "User3",
        "User4",
        "User5",
        "User6",
        "User7",
        "User8",
        "User9",
        NULL
     };
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
  deintMethod   = 0;
  ppMethod   = 0;
  ppQuality   = 0;
  syncOnFrames  = 0;
  avOffset      = 0;
  shouldSuspend = 0;
  ac3Mode       = 0;
  useMGAtv      = 0;
  bufferMode    = 0;
  /* --------------------------------------------------------------------------
   * these screen width/height values are operating in square pixel mode.
   * for non square pixel mode should be set via osd to 720/576
   */
  screenPixelAspect   = 0;

  strcpy (alsaDevice, "");
  strcpy (alsaSPDIFDevice, "hw:0,2");
  voArgs = aoArgs = NULL;
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
    cropMode = clamp (0, cropMode, CROPMODEMAX);
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
  } else if (!strcasecmp(Name, "Suspend")) {
    shouldSuspend = atoi (Value);
    fprintf(stderr, "[setup-softdevice] shouldSuspend to: %d\n", shouldSuspend);
    shouldSuspend = clamp (0, shouldSuspend, 1);
  } else if (!strcasecmp(Name, "AC3Mode")) {
    ac3Mode = atoi (Value);
    fprintf(stderr, "[setup-softdevice] alsa ac3Mode set to: %d\n", ac3Mode);
    ac3Mode = clamp (0, ac3Mode, 3);
  }  else  return false;

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
  if (strcmp(pp_str[ppMethod], "none") == 0) return "";
  else if (strcmp(pp_str[ppMethod], "fast") == 0) return "fa";
  else if (strcmp(pp_str[ppMethod], "default") == 0) return "de";
  else return NULL;
}

/* ---------------------------------------------------------------------------
 */
void cSetupStore::CropModeNext(void)
{
  cropMode = (cropMode == CROPMODEMAX) ? 0 : cropMode + 1;
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
cMenuSetupSoftdevice::cMenuSetupSoftdevice(cPlugin *plugin)
{
  if (plugin)
    SetPlugin(plugin);

  copyData = setupStore;
  data = &setupStore;

  if (data->outputMethod == VOUT_XV)
  {
    Add(new cMenuEditStraItem(tr("Xv startup aspect"),
                             &data->xvAspect,
                             2,
                             xv_startup_aspect));

    /* ------------------------------------------------------------------------
     * don't offer that menu option as there is no immediate check
     * if we have a still a operational system
     */
    //Add(new cMenuEditBoolItem(tr("Xv MaxArea"),
    //                         &data->xvMaxArea, tr("no"), tr("yes")));
  }

  Add(new cMenuEditStraItem(tr("CropMode"),
                            &data->cropMode,
                            CROPMODES,
                            crop_str));

  Add(new cMenuEditStraItem(tr("CropModeToggleKey"),
                            &data->cropModeToggleKey,
                            10,
                            userKeyUsage));

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
  Add(new cMenuEditStraItem(tr("Postprocessing Method"),
                              &data->ppMethod,3,pp_str));
  Add(new cMenuEditIntItem(tr("Postprocessing Quality"),
                              &data->ppQuality,0,6));
#endif
  Add(new cMenuEditStraItem(tr("Buffer Mode"),
                              &data->bufferMode,3,bufferModes));
  
  if (data->outputMethod == VOUT_DFB || data->outputMethod == VOUT_VIDIX)
  {
    Add(new cMenuEditStraItem(tr("Pixel Format"),
                              &data->pixelFormat,
                              3,
                              pix_fmt));
  }

  Add(new cMenuEditBoolItem(tr("Picture mirroring"),
                            &data->mirror, tr("off"), tr("on")));

  Add(new cMenuEditIntItem(tr("A/V Delay"),
                           &data->avOffset,
                           MINAVOFFSET, MAXAVOFFSET));

  Add(new cMenuEditStraItem(tr("Screen Aspect"),
                            &data->screenPixelAspect,
                            5,
                            videoAspectNames));

  Add(new cMenuEditStraItem(tr("Playback"),
                            &data->shouldSuspend,
                            2,
                            suspendVideo));
  Add(new cMenuEditStraItem(tr("OSD alpha blending"),
                            &data->osdMode,
                            2,
                            osdMode));

  Add(new cMenuEditStraItem(tr("AC3 Mode"),
                            &data->ac3Mode,
                            4,
                            ac3ModeNames));
}

/* ---------------------------------------------------------------------------
 */
eOSState cMenuSetupSoftdevice::ProcessKey(eKeys Key)
{
    eOSState state = cOsdMenu::ProcessKey(Key);

  if (state == osUnknown)
  {
    switch (Key)
    {
      case kOk:
        Store();
        state = osBack;
        break;
      default:
        break;
    }
  }
  else if (state == osBack)
  {
    setupStore = copyData;
    fprintf (stderr, "[setup-softdevice] restoring setup state\n");
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
  SetupStore ("Deinterlace Method", setupStore.deintMethod);
  SetupStore ("Postprocess Method", setupStore.ppMethod);
  SetupStore ("Postprocess Quality", setupStore.ppQuality);
  SetupStore ("PixelFormat",        setupStore.pixelFormat);
  SetupStore ("Picture mirroring",  setupStore.mirror);
  SetupStore ("avOffset",           setupStore.avOffset);
  SetupStore ("AlsaDevice",         setupStore.alsaDevice);
  SetupStore ("AlsaSPDIFDevice",    setupStore.alsaSPDIFDevice);
  SetupStore ("PixelAspect",        setupStore.screenPixelAspect);
  SetupStore ("Suspend",            setupStore.shouldSuspend);
  SetupStore ("OSDalphablend",      setupStore.osdMode);
  SetupStore ("AC3Mode",            setupStore.ac3Mode);
  SetupStore ("bufferMode",            setupStore.bufferMode);
}
