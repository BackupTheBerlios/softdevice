/*
 * setup-softdevice.h
 *
 * See the README file for copyright information and how to reach the authors.
 *
 * $Id: setup-softdevice.h,v 1.6 2005/02/18 13:31:27 wachm Exp $
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
    bool  SetupParse(const char *Name, const char *Value);
    char  *getPPValue(void);
    void  getScreenDimension(int &w, int &h);

    int   xvAspect;
    int   outputMethod;
    int   pixelFormat;
    int   cropMode;
    int   deintMethod;
    int   mirror;
    int   syncOnFrames;
    int   avOffset;
    int   screenPixelAspect;
    int   useMGAtv;
    int   shouldSuspend;
    int   osdMode;
    char  alsaDevice [ALSA_DEVICE_NAME_LENGTH];
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
    cMenuSetupSoftdevice(void);
};

extern cSetupStore setupStore;

#endif //__SETUP_SOFTDEVICE_H
