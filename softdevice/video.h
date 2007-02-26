/*
 * video.h: A plugin for the Video Disk Recorder
 *
 * See the README file for copyright information and how to reach the author.
 *
 * $Id: video.h,v 1.51 2007/02/26 23:00:34 lucke Exp $
 */

#ifndef VIDEO_H
#define VIDEO_H

#ifndef STAND_ALONE
#include <vdr/plugin.h>
#include <vdr/remote.h>
#else
#include "VdrReplacements.h"
#endif

#ifdef HAVE_CONFIG
# include "config.h"
#endif

#include "setup-softdevice.h"
#include "sync-timer.h"
#include "PicBuffer.h"

#define DV_FORMAT_UNKNOWN -1
#define DV_FORMAT_NORMAL  1
#define DV_FORMAT_WIDE    2

#define OSD_FULL_WIDTH    736
#define OSD_FULL_HEIGHT   576

#define SRC_HEIGHT         576
#define SRC_WIDTH          736

#define AVRG_OFF_CNT        16

#ifndef STAND_ALONE
class cSoftRemote : public cRemote {
  public:
          cSoftRemote(const char *Name) : cRemote(Name) {};
          virtual ~cSoftRemote() {};
          virtual bool PutKey(uint64_t Code, bool Repeat = false,
                          bool Release = false)
          { return Put(Code,Repeat,Release); };
};
#endif

class cVideoOut: public cPicBufferManager, public cThread {
private:
    int     aspect_I;
    double  aspect_F;

    void    AdjustToZoomFactor(int *tw, int *th),
            AdjustSourceArea(int tw, int th),
            AdjustToDisplayGeometry(double afd_aspect);

    // -----------------------------------------------------------------------
    // oldPicture is a reference to a previous decoded frame. It is used
    // when there is osd drawing activity in case of no video currently
    // available. Access to this pointer must be protected by corresponding
    // mutex: oldPictureMutex . This includes changeing the contens e.g.
    // when a decoded picture will be displayed and thus will be the reference
    // for future operation on oldPicture.
    //
    sPicBuffer  *oldPicture;
    cMutex      oldPictureMutex;
    void        SetOldPicture(sPicBuffer *pic);

protected:
    inline double GetAspect_F()
    { return aspect_F;};

    bool    OSDpresent,
            OSDpseudo_alpha;
    int     current_osdMode;
    int     scaleVid, vidX1, vidX2, vidY1, vidY2;
    int     Xres, Yres, Bpp; // the child class MUST set these params (for OSD Drawing)
    int     dx, dy,
            dwidth, dheight,
            old_x, old_y, old_dwidth, old_dheight,
            screenPixelAspect,
            prevZoomFactor,
            zoomCenterX,
            zoomCenterY;
    double  realZoomFactor;
    volatile int        fwidth, fheight;
    int     swidth, sheight,
            sxoff, syoff,
            lwidth, lheight,
            lxoff, lyoff,
            flags,
            display_aspect,
            current_aspect,
            currentPixelFormat,
            cutTop, cutBottom,
            cutLeft, cutRight,
            expandTopBottom,
            expandLeftRight,
            aspect_changed,
            current_afd,
            interlaceMode,
            displayTimeUS;
    double  parValues[SETUP_VIDEOASPECTNAMES_COUNT];

    bool    videoInitialized;

    cSetupStore *setupStore;

    bool active;
    volatile uint16_t OsdRefreshCounter;
    // should be set to 0 everytime OSD is shown
    // (software alpha blending mode).

    virtual void RecalculateAspect(void);

    /* -----------------------------------------------------------------------
     * To be used in video thread only: Action()
     */
    virtual bool IsSoftOSDMode();

    /* -----------------------------------------------------------------------
     */
    int     frame,
            droppedFrames,
            repeatedFrames;
    bool    offsetInHold;
    int     hurryUp;
    int     delay;
    int     dispTime;
    int     frametime;
    int     offsetHistory[AVRG_OFF_CNT];
    int     offsetIndex;
    int     offsetCount;
    int     offsetAverage;
    int     offsetSum;
    int     dropStart,
            dropEnd,
            dropInterval,
            offsetClampHigh,
            offsetClampLow,
            offsetUse;
    bool    useAverage4Drop;


public:
    cVideoOut(cSetupStore *setupStore);
    virtual ~cVideoOut();

    virtual void ProcessEvents ()
    {};
    // will be called every xx ms, at a minimum after each frame.
    // Can be used to process for example keypress events

    virtual void Sync(cSyncTimer *syncTimer, int *delay);
    virtual void YUV(sPicBuffer *buf) { return; };
    //virtual void YUV(uint8_t *Py, uint8_t *Pu, uint8_t *Pv, int Width, int Height, int Ystride, int UVstride) { return; };
    virtual void Pause(void) {return;};
    virtual void SetParValues(double displayAspect, double displayRatio);
    virtual void CheckAspect(int new_afd, double new_asp);
    virtual void CheckAspectDimensions (sPicBuffer *pic);
    virtual void CheckArea(int w, int h);
    virtual void DrawVideo_420pl(cSyncTimer *syncTimer,
                                 sPicBuffer *pic);
    virtual void DrawStill_420pl(sPicBuffer *buf);
    virtual void EvaluateDelay(int aPTS, int pts, int frametime);
    virtual void ResetDelay(void);
    virtual bool Initialize(void) {videoInitialized = true; return 1;};
    virtual bool Reconfigure (int format = 0,
                    int width = SRC_WIDTH, int height = SRC_HEIGHT)
    {return 1;};

    virtual void Suspend(void) { return;};
    virtual bool Resume(void) { return true;};
    inline void FreezeMode(bool freeze)
    {freezeMode=freeze;};
    bool freezeMode;

    inline void GetLockLastPic(sPicBuffer *&pic)
            // Returns a pointer to the last decoded frame.
            // The caller has to unlock the picture buffer after use
            // by calling cVideoOut::UnlockBuffer().
    {
            oldPictureMutex.Lock();
            pic=oldPicture;
            if (oldPicture)
		LockBuffer(oldPicture);
            oldPictureMutex.Unlock();
    };


    virtual void Action(void);
    // osd control thread. Refreshes the osd on dimension changes and
    // activates fallback mode if the osd was not updated for a too long
    // time

    uint8_t *PixelMask;
    uint8_t *OsdPy;
    uint8_t *OsdPu;
    uint8_t *OsdPv;
    uint8_t *OsdPAlphaY;
    uint8_t *OsdPAlphaUV;
    // buffers for software osd alpha blending
    void init_OsdBuffers();

    int OsdHeight;
    int OsdWidth;
    // current dimensions of the software OSD
private:
    int     Osd_changed;

public:
    virtual void ClearOSD();
    // clear the OSD buffer

    virtual void OpenOSD();
    virtual void SetVidWin(int ScaleVid, int VidX1, int VidY1,
                    int VidX2, int VidY2);
    virtual int GetOSDColorkey();
    virtual void CloseOSD();

    virtual void AdjustOSDMode();

    virtual void GetOSDDimension(int &OsdWidth,int &OsdHeight,
                                 int &xPan, int &yPan)
    // called whenever OSD is to be displayed
    // every video-out should implement a method which desired osd dimension
    // for scaling, if -1,-1 is returned no scaling is done.
    { OsdWidth=-1;OsdHeight=-1; xPan = yPan = 0;};

    virtual void GetOSDMode(int &Depth, bool &HasAlpha, bool &AlphaInversed,
                    bool &IsYUV, uint8_t *&PixelMask)
    { Depth=32; HasAlpha=true; AlphaInversed=false; IsYUV=false;
            PixelMask=NULL;};
    // should be implemented by all video out method to set the OSD pixel mode

    // RGB modes
    virtual void GetLockOsdSurface(uint8_t *&osd, int &stride,
                    bool *&dirtyLines)
    { osd=NULL; stride=0; dirtyLines=NULL;};
    virtual void CommitUnlockOsdSurface()
    { OSDpresent=true; Osd_changed=1; };

    // Software YUV mode
    virtual void GetLockSoftOsdSurface(
                        uint8_t *&osdPy, uint8_t *&osdPu, uint8_t *&osdPv,
                        uint8_t *&osdPAlphaY, uint8_t *&osdPAlphaUV,
                        int &strideY, int &strideUV)
    { osdPy=OsdPy; osdPu=OsdPu; osdPv=OsdPv;
            osdPAlphaY=OsdPAlphaY; osdPAlphaUV=OsdPAlphaUV;
            strideY=OSD_FULL_WIDTH; strideUV=OSD_FULL_WIDTH/2;};

    virtual void CommitUnlockSoftOsdSurface( int osdwidth, int osdheight)
    { OSDpresent=true; OsdWidth=osdwidth; OsdHeight=osdheight; Osd_changed=1;};

};

#endif // VIDEO_H
