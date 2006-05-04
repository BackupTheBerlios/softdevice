/*
 * setup-softdevice.h
 *
 * See the README file for copyright information and how to reach the authors.
 *
 * $Id: setup-softdevice.h,v 1.29 2006/05/04 21:40:12 lucke Exp $
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

#define VOUT_XV       1
#define VOUT_FB       2
#define VOUT_DFB      3
#define VOUT_VIDIX    4
#define VOUT_DUMMY    5
#define VOUT_SHM      6

#define ALSA_DEVICE_NAME_LENGTH  64

#define CAP_BRIGHTNESS  1
#define CAP_CONTRAST    2
#define CAP_HUE         4
#define CAP_SATURATION  8

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
#define SETUP_VIDEOASPECTNAMES 6
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
class cSetupStore {
  public:
                  cSetupStore ();
    virtual       ~cSetupStore () {};
    bool          SetupParse(const char *Name, const char *Value);
    char          *getPPdeintValue(void);
    char          *getPPValue(void);
    void          CropModeNext(void);

    virtual bool  CatchRemoteKey(const char *remoteName, uint64 key);

    int   xvAspect;
    int   xvMaxArea;
    int   xvFullscreen;
    int   outputMethod;
    int   pixelFormat;
    int   cropMode;
    int   cropModeToggleKey;
    int   cropTopLines;
    int   cropBottomLines;
    int   cropLeftCols;
    int   cropRightCols;
    int   expandTopBottomLines;
    int   expandLeftRightCols;
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
    int   tripleBuffering;
    int   useStretchBlit;
    int   shouldSuspend;
    int   osdMode;
    int   ac3Mode;
    int   bufferMode;
    int   mainMenu;
    int   syncTimerMode;
    int   vidBrightness,
          vidContrast,
          vidHue,
          vidSaturation,
          vidCaps;
    char  alsaDevice [ALSA_DEVICE_NAME_LENGTH];
    char  alsaAC3Device [ALSA_DEVICE_NAME_LENGTH];
    char  *voArgs;
    char  *aoArgs;
};

#define OSDMODE_PSEUDO    0
#define OSDMODE_SOFTWARE  1

extern cSetupStore setupStore;

#endif //__SETUP_SOFTDEVICE_H
