/*
 * setup-softdevice.c
 *
 * See the README file for copyright information and how to reach the authors.
 *
 * $Id: setup-softdevice.c,v 1.42 2006/04/23 20:21:27 wachm Exp $
 */

#include <string.h>
#include <stdlib.h>
#include "setup-softdevice.h"

/* ----------------------------------------------------------------------------
 * index to this array correspond to AFD values
 */
const char *crop_str[SETUP_CROPMODES+1];

/* ---------------------------------------------------------------------------
 */
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
const char *pp_str[SETUP_PPMODES];

/* ----------------------------------------------------------------------------
 * allow changing of output pixfmt
 */
const char *pix_fmt[SETUP_PIXFMT];

/* ----------------------------------------------------------------------------
 * give some readable values for aspect ratio selection instead fo 0, 1 values
 */
const char *xv_startup_aspect[SETUP_XVSTARTUPASPECT];

/*-----------------------------------------------------------------------------
 */
#define SETUP_SUSPENDVIDEO 3
const char *suspendVideo[SETUP_SUSPENDVIDEO];

/*-----------------------------------------------------------------------------
 */
const char *osdModeNames[SETUP_OSDMODES];

/*-----------------------------------------------------------------------------
 */
const char *videoAspectNames[SETUP_VIDEOASPECTNAMES];

/*-----------------------------------------------------------------------------
 */
const char *bufferModes[SETUP_BUFFERMODES];

/*-----------------------------------------------------------------------------
 */
const char *ac3ModeNames[SETUP_AC3MODENAMES];

const char *userKeyUsage[SETUP_USERKEYS];

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
  expandTopBottomLines = 0;
  expandLeftRightCols = 0;
  deintMethod   = 0;
  ppMethod   = 0;
  ppQuality   = 0;
  syncOnFrames  = 0;
  avOffset      = 0;
  shouldSuspend = 0;
  ac3Mode       = 0;
  useMGAtv      = 0;
  viaTv         = 0;
  tripleBuffering = 0;
  useStretchBlit  = 0;
  bufferMode      = 0;
  mainMenu  = 1;
  syncTimerMode = 2;
  vidCaps = 0;
  vidBrightness = vidHue = vidContrast = vidSaturation = VID_MAX_PARM_VALUE / 2;

  /* --------------------------------------------------------------------------
   * these screen width/height values are operating in square pixel mode.
   * for non square pixel mode should be set via osd to 720/576
   */
  screenPixelAspect   = 0;
  zoom                = 0;
  zoomFactor          = 0;
  zoomCenterX         = 0;
  zoomCenterY         = 0;

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

  bufferModes[0] = tr("safe");
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
  } else if(!strcasecmp(Name,"ExpandTopBottomLines")) {
    expandTopBottomLines = atoi(Value);
    expandTopBottomLines = clamp (0, expandTopBottomLines, MAX_CROP_LINES/2);
    fprintf(stderr,"[setup-softdevice] Expanding %d columns at top and bottom\n",
            expandTopBottomLines);
  } else if(!strcasecmp(Name,"ExpandLeftRightCols")) {
    expandLeftRightCols = atoi(Value);
    expandLeftRightCols = clamp (0, expandLeftRightCols, MAX_CROP_COLS/2);
    fprintf(stderr,"[setup-softdevice] Expanding %d columns at left and right\n",
            expandLeftRightCols);
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
  } else if (!strcasecmp(Name, "vidBrightness")) {
    vidBrightness = atoi (Value);
    vidBrightness = clamp (0, vidBrightness, 100);
    fprintf(stderr, "[setup-softdevice] vidBrightness: %d\n", vidBrightness);
  } else if (!strcasecmp(Name, "vidContrast")) {
    vidContrast = atoi (Value);
    vidContrast = clamp (0, vidContrast, 100);
    fprintf(stderr, "[setup-softdevice] vidContrast: %d\n", vidContrast);
  } else if (!strcasecmp(Name, "vidHue")) {
    vidHue = atoi (Value);
    vidHue = clamp (0, vidHue, 100);
    fprintf(stderr, "[setup-softdevice] vidHue: %d\n", vidHue);
  } else if (!strcasecmp(Name, "vidSaturation")) {
    vidSaturation = atoi (Value);
    vidSaturation = clamp (0, vidSaturation, 100);
    fprintf(stderr, "[setup-softdevice] vidSaturation: %d\n", vidSaturation);
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
#ifndef STAND_ALONE
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
#endif
  return false;
}

