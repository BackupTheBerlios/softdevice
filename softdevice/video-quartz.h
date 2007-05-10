/*
 *
 * video-quartz.h: A plugin for the Video Disk Recorder
 *
 * Copyright Martin Wache 2006
 *
 * 
 */
#ifndef __VIDEO_QUARTZ_H__
#define __VIDEO_QUARTZ_H__

#include "video.h"

#undef always_inline
namespace MacOs {
#include <Carbon/Carbon.h>
#include <OpenGL/gl.h>
#include <OpenGL/glext.h>
#include <AGL/agl.h>
};

using namespace std;
using namespace MacOs;

#define IMAGE_WIDTH 720
#define IMAGE_HEIGHT 576
extern cSoftRemote *quartzRemote;

class cQuartzVideoOut : public cVideoOut {
private:
        cMutex glMutex;

        WindowRef theWindow;
        WindowGroupRef winGroup;
        CGContextRef context;
        MenuRef windMenu;
        MenuRef movMenu;
        MenuRef aspectMenu;
       
        Rect imgRect; // size of the original image (unscaled)
        Rect dstRect; // size of the displayed image (after scaling)
        Rect winRect; // size of the window containg the displayed image (include padding)
        Rect oldWinRect; // size of the window containg the displayed image (include padding) when NOT in FS mode
        Rect deviceRect; // size of the display device
        Rect oldWinBounds;

        int winLevel;

        unsigned char *image_data[2];
        unsigned int image_size;
        unsigned int image_buffer_size;

        GLuint osd_texture;
        unsigned char * osd_image_data;

        void CreateWindow( uint32_t d_width, uint32_t d_height, 
                WindowAttributes windowAttrs);

        void InitGl();
        bool glInitialized;
        AGLContext    aglCtx;
        AGLPixelFormat	aglFmt;
        bool isFullscreen;
        GLuint p_textures[2];
        int curr_index; 
        int image_width;
        int image_height;
        GDHandle deviceHdl;
        int lastTime;
 
        sPicBuffer privBuffer;
public:
        int osd_shm_id;
        int pic_shm_id;
        cQuartzVideoOut(cSetupStore *setupStore, cSetupSoftlog *Softlog);
        virtual ~cQuartzVideoOut();

        virtual void ProcessEvents();
        virtual void YUV(sPicBuffer *buf);
        void Render();
        
        virtual void GetLockOsdSurface(uint8_t *&osd, int &stride, bool *&dirtyLines); 
        virtual void CommitUnlockOsdSurface();
 
        void WindowResized();
        void ToggleFullscreen();
        inline bool IsOpen()
        { return (theWindow); }
        
        void RmShmMemory();

        OSStatus MouseEventHandler(EventHandlerCallRef nextHandler, EventRef event, void *userData);

        OSStatus WindowEventHandler(EventHandlerCallRef nextHandler, EventRef event, void *userData);

};

#endif
