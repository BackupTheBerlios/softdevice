/*
 * setup-softdevice.h
 *
 * See the README file for copyright information and how to reach the authors.
 *
 * $Id: setup-softdevice.h,v 1.42 2007/05/10 19:54:44 wachm Exp $
 */

#ifndef __SETUP_SOFTDEVICE_H
#define __SETUP_SOFTDEVICE_H
#include <stdint.h>

#ifndef STAND_ALONE
#include <vdr/menu.h>
#include <vdr/plugin.h>
#include <vdr/i18n.h>
#else
#include "VdrReplacements.h"
#endif

#include "setup-softlog.h"

#ifdef HAVE_CONFIG
# include "config.h"
#endif

#define VOUT_XV       1
#define VOUT_FB       2
#define VOUT_DFB      3
#define VOUT_VIDIX    4
#define VOUT_DUMMY    5
#define VOUT_SHM      6
#define VOUT_QUARTZ   7

#define ALSA_DEVICE_NAME_LENGTH  64

#define CAP_BRIGHTNESS       1
#define CAP_CONTRAST         2
#define CAP_HUE              4
#define CAP_SATURATION       8
#define CAP_HWDEINTERLACE   16

#define VID_MAX_PARM_VALUE  100

#define MINAVOFFSET (-250)
#define MAXAVOFFSET (250)

#define MAX_CROP_LINES  50
#define MAX_CROP_COLS   50

/* ----------------------------------------------------------------------------
 * index to this array correspond to AFD values
 */
#define SETUP_CROPMODES 4
extern const char *crop_str[SETUP_CROPMODES+1];

/*-----------------------------------------------------------------------------
 */
#define SETUP_USERKEYS 11
extern const char *userKeyUsage[SETUP_USERKEYS];

/* ---------------------------------------------------------------------------
 */
#define SETUP_DEINTMODES 9
extern const char *deint_str[SETUP_DEINTMODES];

/*-----------------------------------------------------------------------------
 */
#define SETUP_PPMODES 4
extern const char *pp_str[SETUP_PPMODES];

/* ----------------------------------------------------------------------------
 * give some readable values for aspect ratio selection instead fo 0, 1 values
 */
#define SETUP_XVSTARTUPASPECT 3
extern const char *xv_startup_aspect[SETUP_XVSTARTUPASPECT];

/*-----------------------------------------------------------------------------
 */
#define SETUP_OSDMODES 3
extern const char *osdModeNames[SETUP_OSDMODES];

/*-----------------------------------------------------------------------------
 */
#define SETUP_VIDEOASPECTNAMES        7
#define SETUP_VIDEOASPECTNAMES_COUNT  (SETUP_VIDEOASPECTNAMES-1)
#define SETUP_VIDEOASPECTNAMES_LAST   (SETUP_VIDEOASPECTNAMES_COUNT-1)
extern const char *videoAspectNames[SETUP_VIDEOASPECTNAMES];

/*-----------------------------------------------------------------------------
 */
#define SETUP_BUFFERMODES 4
extern const char *bufferModes[SETUP_BUFFERMODES];

/*-----------------------------------------------------------------------------
 */
#define SETUP_AC3MODENAMES 5
extern const char *ac3ModeNames[SETUP_AC3MODENAMES];

extern const char *userKeyUsage[SETUP_USERKEYS];

#define SETUP_SYNC_TIMER_NAMES  4
extern const char *syncTimerNames[SETUP_SYNC_TIMER_NAMES];

/* ----------------------------------------------------------------------------
 * allow changing of output pixfmt
 */
#define SETUP_PIXFMT 4
extern const char *pix_fmt[SETUP_PIXFMT];

/*-----------------------------------------------------------------------------
 */
#define SETUP_SUSPENDVIDEO 3
extern const char *suspendVideo[SETUP_SUSPENDVIDEO];

/* ---------------------------------------------------------------------------
 */
struct cSetupStore {
  public:
    void InitSetupStore();
    bool          SetupParse(const char *Name, const char *Value);
    char          *getPPdeintValue(void);
    char          *getPPValue(void);
    inline void CropModeNext(void) {
       cropMode = (cropMode == (SETUP_CROPMODES-1)) ? 0 : cropMode + 1;
    };

    int   xvAspect;
    int   xvMaxArea;
    int   xvFullscreen;
    int   xvUseDefaults;
    int   outputMethod;
    int   pixelFormat;
    bool  pixelFormatLocked;
    int   cropMode;
    int   cropModeToggleKey;
    int   cropTopLines;
    int   cropBottomLines;
    int   cropLeftCols;
    int   cropRightCols;
    int   expandTopBottomLines;
    int   expandLeftRightCols;
    int   autodetectAspect;
    int   deintMethod;
    int   ppMethod;
    int   ppQuality;
    int   mirror;
    int   syncOnFrames;
    int   avOffset;
    int   screenPixelAspect;
    int   zoom;
    int   zoomFactor;
    int   zoomCenterX;
    int   zoomCenterY;
    int   useMGAtv;
    int   viaTv;
    int   cle266HWdecode;
    int   tripleBuffering;
    int   useStretchBlit;
    bool  stretchBlitLocked;
    int   shouldSuspend;
    int   osdMode;
    int   ac3Mode;
    int   useMixer;
    int   bufferMode;
    int   mainMenu;
    int   syncTimerMode;
    int   vidBrightness,
          vidContrast,
          vidHue,
          vidSaturation,
          vidDeinterlace,
          vidCaps;
    int   useSetSourceRectangle,
          setSourceRectangleLocked;
    char  alsaDevice [ALSA_DEVICE_NAME_LENGTH];
    char  alsaAC3Device [ALSA_DEVICE_NAME_LENGTH];
};

#define OSDMODE_PSEUDO    0
#define OSDMODE_SOFTWARE  1

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

extern cSetupSoftlog *softlog;
extern cSetupStore *setupStore;
extern int setupStoreShmId;

#endif //__SETUP_SOFTDEVICE_H
