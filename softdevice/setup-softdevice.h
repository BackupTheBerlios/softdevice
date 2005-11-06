/*
 * setup-softdevice.h
 *
 * See the README file for copyright information and how to reach the authors.
 *
 * $Id: setup-softdevice.h,v 1.23 2005/11/06 07:26:31 lucke Exp $
 */

#ifndef __SETUP_SOFTDEVICE_H
#define __SETUP_SOFTDEVICE_H

#define VOUT_XV       1
#define VOUT_FB       2
#define VOUT_DFB      3
#define VOUT_VIDIX    4
#define VOUT_DUMMY    5

#define ALSA_DEVICE_NAME_LENGTH  64

#define CAP_BRIGHTNESS  1
#define CAP_CONTRAST    2
#define CAP_HUE         4
#define CAP_SATURATION  8

#define VID_MAX_PARM_VALUE  100

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
    int   deintMethod;
    int   ppMethod;
    int   ppQuality;
    int   mirror;
    int   syncOnFrames;
    int   avOffset;
    int   screenPixelAspect;
    int   useMGAtv;
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

/* ---------------------------------------------------------------------------
 */
class cMenuSetupVideoParm : public cOsdMenu
{
  private:
    cSetupStore *data, copyData;

  protected:
    virtual eOSState ProcessKey(eKeys Key);

  public:
    cMenuSetupVideoParm(const char *name);
};

/* ---------------------------------------------------------------------------
 */
class cMenuSetupCropping : public cOsdMenu
{
  private:
    cSetupStore *data, copyData;

  protected:
    virtual eOSState ProcessKey(eKeys Key);

  public:
    cMenuSetupCropping(const char *name);
};

/* ---------------------------------------------------------------------------
 */
class cMenuSetupPostproc : public cOsdMenu
{
  private:
    cSetupStore *data, copyData;

  protected:
    virtual eOSState ProcessKey(eKeys Key);

  public:
    cMenuSetupPostproc(const char *name);
};

/* ---------------------------------------------------------------------------
 */
class cMenuSetupSoftdevice : public cMenuSetupPage {
  private:
    cSetupStore *data, copyData;

  protected:
    virtual eOSState ProcessKey(eKeys Key);
    virtual void Store(void);

  public:
    cMenuSetupSoftdevice(cPlugin *plugin = NULL);
};

extern cSetupStore setupStore;

#endif //__SETUP_SOFTDEVICE_H
