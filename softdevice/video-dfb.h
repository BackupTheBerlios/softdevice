/*
 * video-dfb.h: A plugin for the Video Disk Recorder
 *
 * See the README file for copyright information and how to reach the author.
 *
 * $Id: video-dfb.h,v 1.4 2004/12/21 05:55:42 lucke Exp $
 */

#ifndef VIDEO_DFB_H
#define VIDEO_DFB_H

#include "video.h"
#include <dfb++.h>
#include <directfb.h>
#include <directfb_keynames.h>

#include <pthread.h>

#include <vdr/remote.h>
#include <vdr/thread.h>

class cDFBRemote;

class cDFBVideoOut : public cVideoOut {
  private:
    IDirectFBDisplayLayer *osdLayer, *videoLayer;
    DFBSurfaceDescription scrDsc, osdDsc, vidDsc;
    IDirectFBSurface      *osdSurface, *videoSurface, *scrSurface;

    DFBSurfacePixelFormat pixelformat;
    cDFBRemote            *dfbRemote;

    IDirectFBEventBuffer  *events;

    bool deinterlace;
    bool alphablend;
    bool useStretchBlit;
    bool osdClrBack;
    bool isVIAUnichrome;
    int   clearAlpha;

    void SetParams();

  public:
    IDirectFB	*dfb;
    cDFBVideoOut();
    virtual ~cDFBVideoOut();
    void ProcessEvents ();
    void ShowOSD ();

#if VDRVERSNUM >= 10307
    virtual void Refresh(cBitmap *Bitmap);
    virtual void OSDStart();
    virtual void OSDCommit();
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
    void          PutKey (DFBInputDeviceKeySymbol key);
};

#endif // VIDEO_DFB_H
