/*
 * setup-softdevice.c
 *
 * See the README file for copyright information and how to reach the authors.
 *
 * $Id: setup-softdevice.c,v 1.12 2005/03/04 20:04:20 lucke Exp $
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
        NULL
     };

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
        "Monitor",
        "TV  4:3 PAL",
        "TV 16:9 PAL",
        "5:4 as 4:3",
        "test 2",
        NULL
     };
struct sVideoAspectsValues {
  int   width,
        height;
} videoAspectValues [] = {
  { 768, 576},
  { 720, 576},
  { 540, 576},
  { 800, 576},
  {1280, 1024},
  {768,576}
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
  outputMethod  = 0;
  cropMode      = 0;
  deintMethod   = 0;
  syncOnFrames  = 0;
  avOffset      = 0;
  shouldSuspend = 0;

  useMGAtv      = 0;
  /* --------------------------------------------------------------------------
   * these screen width/height values are operating in square pixel mode.
   * for non square pixel mode should be set via osd to 720/576
   */
  screenPixelAspect   = 0;

  strcpy (alsaDevice, "");
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
  } else if(!strcasecmp(Name,"CropMode")) {
    cropMode = atoi(Value);
    cropMode = clamp (0, cropMode, 2);
    fprintf (stderr, "[setup-softdevice] cropping mode set to %d (%s)\n",
             cropMode,
             crop_str [cropMode]);
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
  } else  return false;

  return true;
}

/* ---------------------------------------------------------------------------
 */
void cSetupStore::getScreenDimension(int &w, int &h)
{
  w = videoAspectValues [screenPixelAspect]. width;
  h = videoAspectValues [screenPixelAspect]. height;
}

/* ---------------------------------------------------------------------------
 */
char *cSetupStore::getPPValue(void)
{
  if (strcmp(deint_str[deintMethod], "linblend") == 0) return "lb:a";
  else if (strcmp(deint_str[deintMethod], "linipol") == 0) return "li:a";
  else if (strcmp(deint_str[deintMethod], "cubicipol") == 0) return "ci:a";
  else if (strcmp(deint_str[deintMethod], "median") == 0) return "md:a";
  else if (strcmp(deint_str[deintMethod], "ffmpeg") == 0) return "fd:a";
  else return "unknown";
}

/* ---------------------------------------------------------------------------
 */
cMenuSetupSoftdevice::cMenuSetupSoftdevice(void)
{
  copyData = setupStore;
  data = &setupStore;

  if (data->outputMethod == VOUT_XV)
  {
    Add(new cMenuEditStraItem(tr("Xv startup aspect"),
                             &data->xvAspect,
                             2,
                             xv_startup_aspect));
  }

  Add(new cMenuEditStraItem(tr("CropMode"),
                            &data->cropMode,
                            3,
                            crop_str));

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

  Add(new cMenuEditStraItem(tr("Pixel Aspect"),
                            &data->screenPixelAspect,
                            //2,
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
  SetupStore ("CropMode",           setupStore.cropMode);
  SetupStore ("Deinterlace Method", setupStore.deintMethod);
  SetupStore ("PixelFormat",        setupStore.pixelFormat);
  SetupStore ("Picture mirroring",  setupStore.mirror);
  SetupStore ("avOffset",           setupStore.avOffset);
  SetupStore ("AlsaDevice",         setupStore.alsaDevice);
  SetupStore ("PixelAspect",        setupStore.screenPixelAspect);
  SetupStore ("Suspend",            setupStore.shouldSuspend);
  SetupStore ("OSDalphablend",      setupStore.osdMode);
}
