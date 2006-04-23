/*
 * video-dfb.h: A plugin for the Video Disk Recorder
 *
 * See the README file for copyright information and how to reach the author.
 *
 * $Id: video-dfb.h,v 1.19 2006/04/23 19:38:29 wachm Exp $
 */

#ifndef VIDEO_DFB_H
#define VIDEO_DFB_H

#include "video.h"
#include <dfb++.h>
#include <directfb.h>
#include <directfb_version.h>
#include <directfb_keynames.h>

#include <pthread.h>

#include <vdr/remote.h>
#include <vdr/thread.h>


class cDFBVideoOut : public cVideoOut {
  private:
    IDirectFBDisplayLayer *osdLayer, *videoLayer;
    DFBSurfaceDescription scrDsc, osdDsc, vidDsc;
    IDirectFBSurface      *osdSurface, *videoSurface, *scrSurface;

    DFBSurfacePixelFormat pixelformat;
    cSoftRemote            *dfbRemote;

    IDirectFBEventBuffer  *events;

    bool deinterlace;
    bool alphablend;
    bool useStretchBlit;
    bool osdClrBack;
    bool isVIAUnichrome;
    int   clearAlpha;
    int   clearBackCount,
          clearBackground,
          fieldParity;
    int   prevOsdMode;

    void SetParams();
    void EnableFieldParity(IDirectFBDisplayLayer *layer);

  public:
    IDirectFB	*dfb;
    cDFBVideoOut(cSetupStore *setupStore);
    virtual ~cDFBVideoOut();
    void ProcessEvents ();
    void ShowOSD ();
    void GetDisplayFrameTime();

#if VDRVERSNUM >= 10307
    bool *dirtyLines;
    IDirectFBSurface  *tmpOsdSurface;
    virtual void OpenOSD();
    virtual void GetOSDMode(int &Depth, bool &HasAlpha, bool &AlphaInversed,
                            bool &IsYUV, uint8_t *&PixelMask);
    virtual void GetOSDDimension(int &OsdWidth,int &OsdHeight,
                                 int &xPan, int &yPan);
    virtual void OSDStart();
    virtual void GetLockOsdSurface(uint8_t *&osd, int &stride,
                                   bool *&dirtyLines);
    virtual void CommitUnlockOsdSurface();
#else
    virtual void Refresh();
#endif

    virtual void YUV(uint8_t *Py, uint8_t *Pu, uint8_t *Pv,
                     int Width, int Height, int Ystride, int UVstride);
    virtual void Pause(void);
    virtual void CloseOSD();
    virtual bool Initialize (void);
};

/* ---------------------------------------------------------------------------
 */
class cDFBRemote : public cRemote, private cThread {
  private:
    bool                  active;
    cDFBVideoOut          *video_out;

    virtual void  Action(void);

  public:
                  cDFBRemote(const char *Name, cDFBVideoOut *vout);
                  ~cDFBRemote();
    void          DFBRemoteStart (void);
    void          PutKey (DFBInputDeviceKeySymbol key, bool repeat);
};

#endif // VIDEO_DFB_H
