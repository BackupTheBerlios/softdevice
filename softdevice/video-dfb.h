/*
 * video-dfb.h: A plugin for the Video Disk Recorder
 *
 * See the README file for copyright information and how to reach the author.
 *
 * $Id: video-dfb.h,v 1.29 2008/07/20 16:41:01 lucke Exp $
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
    IDirectFBDisplayLayer         *osdLayer, *videoLayer;
    DFBSurfaceDescription         scrDsc, osdDsc, vidDsc;
    DFBDisplayLayerDescription    osdLayerDescription,
                                  videoLayerDescription;
    DFBDisplayLayerConfig         osdLayerConfiguration;
    IDirectFBSurface              *osdSurface, *videoSurface, *scrSurface;

    DFBSurfacePixelFormat pixelformat;
    cSoftRemote            *dfbRemote;

    IDirectFBEventBuffer  *events;

    bool deinterlace;
    bool alphablend;
    bool useStretchBlit;
    bool useSetSourceRectangle;
    bool osdClrBack;
    bool isVIAUnichrome;
    int   clearAlpha;
    int   clearBackCount,
          clearBackground;
    int   prevOsdMode;
    int   videoLayerLevel;

    void  SetParams();
    void  EnableFieldParity(IDirectFBDisplayLayer *layer),
          SetFieldParity(IDirectFBDisplayLayer *layer, int fieldOrder),
          ReportSurfaceCapabilities (IDirectFBSurface *surf,char *name),
          BESColorkeyState(IDirectFBDisplayLayer *layer, bool state),
          ReportLayerInfo(IDirectFBDisplayLayer *layer, char *name);

#ifdef HAVE_CLE266_MPEG_DECODER
    IDirectFBSurface* mpegfb[LAST_PICBUF];
    int               mpegfb_ofs[4];
    bool SetupCle266Buffers(int, int);
#endif // HAVE_CLE266_MPEG_DECODER

    void ClearBorders();

  protected:
    virtual bool IsSoftOSDMode();

  public:
    IDirectFB	*dfb;
    cDFBVideoOut(cSetupStore *setupStore, cSetupSoftlog *Softlog);
    virtual ~cDFBVideoOut();
    void ProcessEvents ();
    void GetDisplayFrameTime();

    bool *dirtyLines;
    IDirectFBSurface  *tmpOsdSurface;
    virtual void OpenOSD();
    virtual void GetOSDMode(int &Depth, bool &HasAlpha, bool &AlphaInversed,
                            bool &IsYUV);
    virtual void GetOSDDimension(int &OsdWidth,int &OsdHeight,
                                 int &xPan, int &yPan);
    virtual void OSDStart();
    virtual void GetLockOsdSurface(uint8_t *&osd, int &stride,
                                   bool *&dirtyLines);
    virtual void CommitUnlockOsdSurface();
    virtual void YUV(sPicBuffer *Pic);
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
