/*
 * softdevice plugin for VDR
 *
 * Copyright (C) 2005 Martin Wache
 *
 * This code is distributed under the terms and conditions of the
 * GNU GENERAL PUBLIC LICENSE. See the file COPYING for details.
 *
 * $Id: video-shm.h,v 1.2 2006/02/03 22:34:54 wachm Exp $
 */

#ifndef __VIDEO_SHM_H__
#define __VIDEO_SHM_H__

#include "video.h"
#include "shm-common.h"

#include <sys/types.h>
#include <sys/ipc.h> 
#include <sys/shm.h> 
#include <sys/sem.h> 

#include <vdr/remote.h>

/*-------------------------------------------------------------------------*/
class cShmVideoOut : public cVideoOut {
        friend class cShmRemote;
        cShmRemote *remote;
        int ctl_shmid;
        key_t ctl_key;
        ShmCtlBlock * ctl;

        int curr_pict_shmid;
        uint8_t *curr_pict;
        uint8_t *pixels[4];

        int curr_osd_shmid;
        uint8_t *osd_surface;

        public:
        cShmVideoOut(cSetupStore *setupStore);
        ~cShmVideoOut();
        
        virtual void GetOSDDimension(int &OsdWidth,int &OsdHeight);
        
        virtual void GetOSDMode(int &Depth, bool &HasAlpha, bool &AlphaInversed,
                    bool &IsYUV, uint8_t *&PixelMask);

        virtual void YUV(uint8_t *Py, uint8_t *Pu, uint8_t *Pv,
                     int Width, int Height, int Ystride, int UVstride);
        virtual void GetLockOsdSurface(uint8_t *&osd, int &stride,
                        bool *&dirtyLines);
        virtual void CommitUnlockOsdSurface();
        virtual void ClearOSD();
};

/* ---------------------------------------------------------------------------
 */
class cShmRemote : public cRemote, private cThread {
  private:
    bool        active;
    cShmVideoOut *vout;

    virtual void Action(void);
  public:
          cShmRemote(const char *Name, cShmVideoOut *video_out) 
                  : cRemote(Name) {
                  vout=video_out;
                  active=true;
                  Start();
          };
          
          ~cShmRemote(); 
          
          void Stop()
          {
                  if (!vout || !active)
                          return;

                  active = false;
                  // signal new key to stop the thread from waiting
                  sem_sig_unlock(vout->ctl->semid,KEY_SIG);
        
                  Cancel(2);
                  vout=NULL;
          };

};

#endif // __VIDEO_SHM_H__
