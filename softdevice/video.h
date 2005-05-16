/*
 * video.h: A plugin for the Video Disk Recorder
 *
 * See the README file for copyright information and how to reach the author.
 *
 * $Id: video.h,v 1.11 2005/05/16 15:53:13 wachm Exp $
 */

#ifndef VIDEO_H
#define VIDEO_H

#include <vdr/plugin.h>
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

// MMX - 3Dnow! defines

#undef PREFETCH
#undef EMMS

#ifdef USE_3DNOW
//#warning Using 3Dnow! extensions
#define PREFETCH "prefetch "
#define EMMS     __asm__ (" femms \n": :  )
#elif defined ( USE_MMX2 )
//#warning Using MMX2 extensions
#define PREFETCH "prefetchnta "
#define EMMS     __asm__ (" emms \n": :  )
#else
//#warning Using MMX extensions
#define PREFETCH
#define EMMS     __asm__ (" emms \n": :  )
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

class cVideoOut: public cThread {
private:
protected:
    cMutex  osdMutex;
    int     OSDxOfs, OSDyOfs,
            OSDw, OSDh;
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
            aspect_changed,
            current_afd;

    cSetupStore *setupStore;
                
    cOsd *osd;
    bool active;
    bool OSDdirty;

public:
    cVideoOut(cSetupStore *setupStore);
    virtual ~cVideoOut();
    virtual void Size(int w, int h) {OSDw = w; OSDh = h;};
    virtual void OSDStart();
    virtual void OSDCommit();
    virtual void OpenOSD(int X, int Y);
    virtual void CloseOSD();
    virtual void Sync(cSyncTimer *syncTimer, int *delay);
    virtual void YUV(uint8_t *Py, uint8_t *Pu, uint8_t *Pv, int Width, int Height, int Ystride, int UVstride) { return; };
    virtual void Pause(void) {return;};
    virtual void CheckAspect(int new_afd, float new_asp);
    virtual void CheckAspectDimensions (AVFrame *picture, AVCodecContext *context);
    virtual bool Initialize(void) {return 1;};
    virtual bool Reconfigure (int format) {return 1;};
    virtual bool GetInfo(int *fmt, unsigned char **dest,int *w, int *h) {return false;};

    virtual void Suspend(void) { return;};
    virtual bool Resume(void) { return true;};

    virtual void Action(void);
    // osd control thread. Refreshes the osd on dimension changes and
    // activates fallback mode if the osd was not updated for a too long
    // time

    void SetOsd(cOsd * Osd) {osd=Osd;};
    // sets a pointer to the current osd. For refreshing of the osd 

    uint8_t *PixelMask;
#if VDRVERSNUM >= 10307
    int Osd_changed;
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

    virtual void GetOSDDimension(int &OsdWidth,int &OsdHeight)
    // called whenever OSD is to be displayed
    // every video-out should implement a method which desired osd dimension
    // for scaling, if -1,-1 is returned no scaling is done.
    { OsdWidth=-1;OsdHeight=-1;};

    virtual void ClearOSD();
    // clear the OSD buffer
    
    virtual void Refresh(cBitmap *Bitmap) { return; };
    
    void Draw(cBitmap *Bitmap,
              unsigned char * buf,
              int linelen,
              bool inverseAlpha = false);
    // draws the bitmap in buf. The bitmap is scaled automaticaly if
    // GetOSDDimension return smaller dimensions
      
   void ToYUV(cBitmap *Bitmap);
   // converts the bitmap to YUV format, saved in OSDP[yuv] and 
   // OSDPAlpha[YUV]. The bitmap is also scaled if requested
   
   void AlphaBlend(uint8_t *dest,uint8_t *P1,uint8_t *P2,
       uint8_t *alpha,uint16_t count);
   // performes alpha blending in software

#else
    cWindowLayer *layer[MAXNUMWINDOWS];
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
