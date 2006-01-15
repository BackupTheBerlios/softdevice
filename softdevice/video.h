/*
 * video.h: A plugin for the Video Disk Recorder
 *
 * See the README file for copyright information and how to reach the author.
 *
 * $Id: video.h,v 1.25 2006/01/15 21:11:25 wachm Exp $
 */

#ifndef VIDEO_H
#define VIDEO_H

#include <vdr/plugin.h>

#ifdef HAVE_CONFIG
# include "config.h"
#endif

#include "setup-softdevice.h"
#include "sync-timer.h"

#if VDRVERSNUM >= 10307
#include <vdr/osd.h>
#endif
#include <avcodec.h>

#define DV_FORMAT_UNKNOWN -1
#define DV_FORMAT_NORMAL  1
#define DV_FORMAT_WIDE    2

#define OSD_FULL_WIDTH    736
#define OSD_FULL_HEIGHT   576

#define MAX_PAR 5

// MMX - 3Dnow! defines

#undef PREFETCH
#undef EMMS

#ifdef USE_3DNOW
//#warning Using 3Dnow! extensions
#define PREFETCH "prefetch "
#define MOVQ     "movntq "
#define EMMS     __asm__ __volatile__  (" femms \n": : : "memory"  )
#elif defined ( USE_MMX2 )
//#warning Using MMX2 extensions
#define PREFETCH "prefetchnta "
#define MOVQ     "movntq "
#define EMMS     __asm__ __volatile__ (" emms \n": : : "memory"  )
#else
//#warning Using MMX extensions
#define PREFETCH
#define MOVQ     "movq "
#define EMMS     __asm__ __volatile__ (" emms \n": : : "memory"  )
#endif


#if VDRVERSNUM < 10307

class cWindowLayer {
  private:
    int left, top;
    int width, height, bpp, xres, yres;
    unsigned char *imagedata;
    bool          OSDpseudo_alpha;
  public:
    cWindowLayer(int X, int Y, int W, int H, int Bpp,
                 int Xres, int Yres, bool alpha);
    ~cWindowLayer();
    void Render(cWindow *Window);
    void Draw(unsigned char * buf, int linelen, unsigned char * keymap);
    void Move(int x, int y);
    void Region (int *x, int *y, int *w, int *h);
    bool visible;
};

#endif

class cSoftOsd;

class cVideoOut: public cThread {
private:
    int     aspect_I;
    double  aspect_F;

protected:
    cMutex  osdMutex,
            areaMutex;
    bool    OSDpresent,
            OSDpseudo_alpha;
    int     current_osdMode;
    int     Xres, Yres, Bpp; // the child class MUST set these params (for OSD Drawing)
    int     dx, dy, 
            dwidth, dheight,
            old_x, old_y, old_dwidth, old_dheight,
            screenPixelAspect;
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
            aspect_changed,
            current_afd,
            displayTimeUS;
    double  parValues[MAX_PAR];

    AVFrame *old_picture;
    int     old_width, old_height;

    cSetupStore *setupStore;

    bool active;

    virtual void RecalculateAspect(void);

public:
    cVideoOut(cSetupStore *setupStore);
    virtual ~cVideoOut();
    virtual void CloseOSD();
    
    virtual void Sync(cSyncTimer *syncTimer, int *delay);
    virtual void YUV(uint8_t *Py, uint8_t *Pu, uint8_t *Pv, int Width, int Height, int Ystride, int UVstride) { return; };
    virtual void Pause(void) {return;};
    virtual void SetParValues(double displayAspect, double displayRatio);
    virtual void CheckAspect(int new_afd, double new_asp);
    virtual void CheckAspectDimensions (AVFrame *picture, AVCodecContext *context);
    virtual void CheckArea(int w, int h);
    virtual void DrawVideo_420pl (cSyncTimer *syncTimer, int *delay,
                                  AVFrame *picture, AVCodecContext *context);
    virtual void DrawStill_420pl (uint8_t *pY, uint8_t *pU, uint8_t *pV,
                                  int w, int h, int yPitch, int uvPitch);
    virtual bool Initialize(void) {return 1;};
    virtual bool Reconfigure (int format) {return 1;};
    virtual bool GetInfo(int *fmt, unsigned char **dest,int *w, int *h) {return false;};

    virtual void Suspend(void) { return;};
    virtual bool Resume(void) { return true;};
    inline void FreezeMode(bool freeze)
    {freezeMode=freeze;};
    bool freezeMode;

    virtual void Action(void);
    // osd control thread. Refreshes the osd on dimension changes and
    // activates fallback mode if the osd was not updated for a too long
    // time

    virtual void InvalidateOldPicture(void);
    virtual void SetOldPicture(AVFrame *picture, int width, int height);

    uint8_t *PixelMask;
    int     Osd_changed;
    uint8_t *OsdPy;
    uint8_t *OsdPu; 
    uint8_t *OsdPv;
    uint8_t *OsdPAlphaY; 
    uint8_t *OsdPAlphaUV;
    // buffers for software osd alpha blending 
    void init_OsdBuffers();
    
    uint16_t OsdHeight;
    uint16_t OsdWidth;
    // current dimensions of the OSD
    
    uint16_t OsdRefreshCounter;
    // should be setted to null everytime OSD is shown 
    // (software alpha blending mode).  

    virtual void ClearOSD();
    // clear the OSD buffer

#if VDRVERSNUM >= 10307
    cSoftOsd *osd;
    void OSDFlush_nolock(cSoftOsd *Osd,bool RefreshAll=false);
    inline void OSDFlush(cSoftOsd *Osd,bool RefreshAll=false)
    {osdMutex.Lock();OSDFlush_nolock(Osd,RefreshAll); osdMutex.Unlock();};
    
    virtual void OpenOSD(int X, int Y, cSoftOsd *Osd);
    
    virtual void GetOSDDimension(int &OsdWidth,int &OsdHeight)
    // called whenever OSD is to be displayed
    // every video-out should implement a method which desired osd dimension
    // for scaling, if -1,-1 is returned no scaling is done.
    { OsdWidth=-1;OsdHeight=-1;};
     
    virtual void GetOSDMode(int &Depth, bool &HasAlpha, bool &AlphaInversed, 
                    bool &IsYUV, uint8_t *&PixelMask)
    { Depth=32; HasAlpha=true; AlphaInversed=false; IsYUV=false; 
            PixelMask=NULL;};
    // should be implemented by all video out method to set the OSD pixel mode

    virtual void RefreshOSD(cSoftOsd *SoftOsd, bool RefreshAll=false) 
    { return; };
      
   void AlphaBlend(uint8_t *dest,uint8_t *P1,uint8_t *P2,
       uint8_t *alpha,uint16_t count);
   // performes alpha blending in software

#else
    int OSDxOfs,OSDyOfs;

    cWindowLayer *layer[MAXNUMWINDOWS];
    virtual void OpenOSD(int X, int Y);
    virtual bool OpenWindow(cWindow *Window);
    virtual void CommitWindow(cWindow *Window);
    virtual void ShowWindow(cWindow *Window);
    virtual void HideWindow(cWindow *Window, bool Hide);
    virtual void MoveWindow(cWindow *Window, int X, int Y);
    virtual void CloseWindow(cWindow *Window);
    virtual void Refresh() {return;};
#endif

};

#endif // VIDEO_H
