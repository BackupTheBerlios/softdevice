/*
 * setup-softdevice.h
 *
 * See the README file for copyright information and how to reach the authors.
 *
 * $Id: setup-softdevice.h,v 1.13 2005/05/29 19:50:44 lucke Exp $
 */

#ifndef __SETUP_SOFTDEVICE_H
#define __SETUP_SOFTDEVICE_H

#define VOUT_XV       1
#define VOUT_FB       2
#define VOUT_DFB      3
#define VOUT_VIDIX    4

#define ALSA_DEVICE_NAME_LENGTH  64

/* ---------------------------------------------------------------------------
 */
class cSetupStore {
  public:
                  cSetupStore ();
    bool          SetupParse(const char *Name, const char *Value);
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
    int   deintMethod;
    int   mirror;
    int   syncOnFrames;
    int   avOffset;
    int   screenPixelAspect;
    int   useMGAtv;
    int   shouldSuspend;
    int   osdMode;
    int   ac3Mode;
    char  alsaDevice [ALSA_DEVICE_NAME_LENGTH];
    char  alsaSPDIFDevice [ALSA_DEVICE_NAME_LENGTH];
    char  *voArgs;
    char  *aoArgs;
};

#define OSDMODE_PSEUDO    0
#define OSDMODE_SOFTWARE  1

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
