/*
 * video.h: A plugin for the Video Disk Recorder
 *
 * See the README file for copyright information and how to reach the author.
 *
 * $Id: video.h,v 1.2 2004/10/23 21:33:26 lucke Exp $
 */

#ifndef VIDEO_H
#define VIDEO_H

#include <vdr/plugin.h>
#include <avcodec.h>

#define DV_FORMAT_UNKNOWN -1
#define DV_FORMAT_NORMAL  1
#define DV_FORMAT_WIDE    2

#if VDRVERSNUM < 10307

class cWindowLayer {
  private:
    int left, top;
    int width, height, bpp, xres, yres;
    unsigned char *imagedata;
  public:
    cWindowLayer(int X, int Y, int W, int H, int Bpp, int Xres, int Yres);
    ~cWindowLayer();
    void Render(cWindow *Window);
    void Draw(unsigned char * buf, int linelen, unsigned char * keymap);
    void Move(int x, int y);
    void Region (int *x, int *y, int *w, int *h);
    bool visible;
};

#endif

class cVideoOut {
private:
protected:
    cMutex  osdMutex;
    int     OSDxOfs, OSDyOfs;
    bool    OSDpresent,
            OSDpseudo_alpha;
    int     Xres, Yres, Bpp; // the child class MUST set these params (for OSD Drawing)
    int     dwidth, dheight,
            fwidth, fheight,
            swidth, sheight,
            sxoff, syoff,
            lwidth, lheight,
            lxoff, lyoff,
            flags,
            display_aspect,
            current_aspect,
            currentPixelFormat,
            aspect_changed,
            current_afd;

public:
    virtual ~cVideoOut();
    virtual void OpenOSD(int X, int Y);
    virtual void CloseOSD();
    virtual void YUV(uint8_t *Py, uint8_t *Pu, uint8_t *Pv, int Width, int Height, int Ystride, int UVstride) { return; };
    virtual void Pause(void) {return;};
    virtual void CheckAspect(int new_afd, float new_asp);
    virtual void CheckAspectDimensions (AVFrame *picture, AVCodecContext *context);
    virtual bool Initialize(void) {return 1;};
    virtual bool Reconfigure (int format) {return 1;};
    virtual bool GetInfo(int *fmt, unsigned char **dest,int *w, int *h) {return false;};

#if VDRVERSNUM >= 10307

    virtual void Refresh(cBitmap *Bitmap) { return; };
    void Draw(cBitmap *Bitmap, unsigned char * buf,
                      int linelen);

#else
    cWindowLayer *layer[MAXNUMWINDOWS];
    uint8_t *PixelMask;
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
