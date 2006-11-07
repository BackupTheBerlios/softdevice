/*
 * softdevice plugin for VDR
 *
 * Copyright (C) 2005 Martin Wache
 *
 * This code is distributed under the terms and conditions of the
 * GNU GENERAL PUBLIC LICENSE. See the file COPYING for details.
 *
 * $Id: video-shm.c,v 1.14 2006/11/07 19:40:10 wachm Exp $
 */

#include "video-shm.h"
#include "utils.h"

//#define SHMDEB(out...) {printf("SHMSERV[%04d]:",(int)(getTimeMilis() % 10000));printf(out);}

#ifndef SHMDEB
#define SHMDEB(out...)
#endif

//#define SIGDEB(out...) {printf("SIGSERV[%04d]:",(int)(getTimeMilis() % 10000));printf(out);}

#ifndef SIGDEB
#define SIGDEB(out...)
#endif
  
cShmRemote::~cShmRemote(){
        SHMDEB("~cShmRemote\n");
        Stop();
        SHMDEB("~cShmRemote returned from cancel\n");
};

void cShmRemote::Action(void) {
        while (active) {
                SIGDEB("cShmRemote trying to get a lock\n");
                sem_wait_lock(vout->ctl->semid,KEY_MUT);
                SIGDEB("cShmRemote got lock\n");
                
                if (!vout->ctl->attached) {
                        vout->setupStore->shouldSuspend=1;
                } else vout->setupStore->shouldSuspend=0;
            
                if (vout->ctl->key!=NO_KEY) {
                        SHMDEB("cShmRemote put key\n");
                        // put and clear
                        Put(vout->ctl->key);
                        vout->ctl->key=NO_KEY;
                };
                        
                // clear signal
                sem_zero(vout->ctl->semid,KEY_SIG);
                // release lock
                sem_sig_unlock(vout->ctl->semid,KEY_MUT);

                SIGDEB("wait for a new key\n");
                sem_wait_lock(vout->ctl->semid,KEY_SIG);
        };
        SHMDEB("ShmRemote thread ended\n");
};


/*----------------------------------------------------------------------------*/
cShmVideoOut::cShmVideoOut(cSetupStore *setupStore) 
        : cVideoOut(setupStore) {
        ctl_key = CTL_KEY;
        bool Clear_Ctl=false;
        InitPicBuffer(&privBuf);

        // first try to get an existing ShmCltBlock
        if  ( (ctl_shmid = shmget(ctl_key,sizeof(ShmCtlBlock), 0666)) >= 0 ) {
                fprintf(stderr,"cShmVideoOut: Got ctl_shmid %d shm ctl!\n",ctl_shmid);
        } else if ( (ctl_shmid = shmget(ctl_key,sizeof(ShmCtlBlock), 
                                        IPC_CREAT | 0666)) >= 0 ) {
                fprintf(stderr,"cShmVideoOut: Created ctl_shmid %d shm ctl!\n",ctl_shmid);
                // created ShmCltBlock, clear it
                Clear_Ctl=true;
        } else {
                fprintf(stderr,"cShmVideoOut: Error creating shm ctl!\n");
                exit(-1);
        };

        // attach to the control block
        if ( (ctl = (ShmCtlBlock*)shmat(ctl_shmid,NULL,0)) 
                        == (ShmCtlBlock*) -1 ) {
                fprintf(stderr,"cShmVideoOut: Error attatching shm ctl!\n");
                exit(-1);
        };

        if (Clear_Ctl) {
                ctl->semid=-1;
                ctl->pict_shmid=-1;
                ctl->osd_shmid=-1;
                ctl->attached = 0;
        };

        if ( ctl->semid == -1 ) {
                // create semaphores
                if ( (ctl->semid = semget(IPC_PRIVATE, 4, 0666 | IPC_CREAT) )
                                == -1) {
                        fprintf(stderr,"cShmVideoOut: Error creating semaphore set!\n");
                        exit(-1);
                };
                SHMDEB("created semaphores id %d\n",ctl->semid);
        };

        semun sem_val;
        sem_val.val = 0; // nothing available
        if ( semctl(ctl->semid,PICT_SIG, SETVAL, sem_val) == -1 ) {
                fprintf(stderr,"cShmVideoOut: Error resetting semaphore PICT_SIG\n");
                exit(-1);
        };
        if ( semctl(ctl->semid,KEY_SIG,SETVAL,sem_val) == -1 ) {
                fprintf(stderr,"cShmVideoOut: Error resetting semaphore KEY_SIG\n");
                exit(-1);
        };
        
        sem_val.val = 1; // not locked
        if ( semctl(ctl->semid,PICT_MUT, SETVAL, sem_val) == -1 ) {
                fprintf(stderr,"cShmVideoOut: Error resetting semaphore PICT_MUT\n");
                exit(-1);
        };
        if ( semctl(ctl->semid,KEY_MUT,SETVAL,sem_val) == -1 ) {
                fprintf(stderr,"cShmVideoOut: Error resetting semaphore KEY_MUT\n");
                exit(-1);
        };
   
        curr_pict_shmid=-1;
        curr_osd_shmid=-1;
        //ctl->pict_shmid=-1;
        curr_pict=NULL;
        osd_surface=NULL;
        ctl->key=NO_KEY;
        remote = new cShmRemote("softdevice-xv",this);
};

cShmVideoOut::~cShmVideoOut() {
        // stop&delete remote
        remote->Stop();
        
        semun sem_val;
        // remove semaphore 
        if ( semctl(ctl->semid, 0, IPC_RMID, sem_val ) == -1) {
            fprintf(stderr,"cShmVideoOut: Error removing semaphore set!\n");
            exit(1);
        }
        ctl->semid=-1;
                        
        if (osd_surface)
                shmdt(osd_surface);
        osd_surface=NULL;
        
        if (curr_pict)
                shmdt(curr_pict);
        curr_pict=NULL;
/*
        if ( ctl_shmid > 0)
                shmctl (ctl_shmid, IPC_RMID, 0);
*/
};

void cShmVideoOut::AdjustOSDMode() {
  current_osdMode = setupStore->osdMode;
}

void cShmVideoOut::GetOSDDimension(int &OsdWidth,int &OsdHeight,
                                   int &xPan, int &yPan) {
   switch (current_osdMode) {
      case OSDMODE_PSEUDO :
                OsdWidth=ctl->osd_width>ctl->osd_max_width?
                        ctl->osd_max_width:ctl->osd_width;
                OsdHeight=ctl->osd_height>ctl->osd_max_height?
                        ctl->osd_max_height:ctl->osd_height;
                xPan = yPan = 0;
             break;
      case OSDMODE_SOFTWARE:
                OsdWidth=swidth;
                OsdHeight=sheight;
                xPan = sxoff;
                yPan = syoff;
             break;
    };
};

void cShmVideoOut::GetOSDMode(int &Depth, bool &HasAlpha, bool &AlphaInversed,
                bool &IsYUV, uint8_t *&PixelMask) {
        if (current_osdMode==OSDMODE_SOFTWARE) {
                IsYUV=true;
                PixelMask=NULL;
                return;
        };

        IsYUV=false; 
        Depth=ctl->osd_depth;
        HasAlpha=false;
        PixelMask=NULL;
};       

void cShmVideoOut::CheckShmIDs() {

        if ( ctl->pict_shmid != curr_pict_shmid ) {
                if (curr_pict) {
                        shmdt(curr_pict);
                        curr_pict=NULL;
                };

                if ( ctl->pict_shmid != -1 ) {
                        if ( (curr_pict = (uint8_t *)shmat(ctl->pict_shmid,NULL,0)) 
                                        == (uint8_t*) -1 ) {
                                fprintf(stderr,"cShmVideoOut: Warning! Could not attatch to shm pict! Assuming no client connected.\n");
                                ctl->pict_shmid = -1;
                                ctl->attached = 0;
                                curr_pict=NULL;
                                return;
                        }
                        curr_pict_shmid=ctl->pict_shmid;
                        privBuf.format=ctl->format;
                        privBuf.max_width=ctl->max_width;
                        privBuf.max_height=ctl->max_height;
                        switch (privBuf.format) {
                                case PIX_FMT_YUV420P:  
                                        SHMDEB("new format YUV420P\n");
                                        privBuf.pixel[0]=curr_pict+ctl->offset0;
                                        privBuf.pixel[1]=curr_pict+ctl->offset1;
                                        privBuf.pixel[2]=curr_pict+ctl->offset2;
                                        privBuf.stride[0]=ctl->stride0;
                                        privBuf.stride[1]=ctl->stride1;
                                        privBuf.stride[2]=ctl->stride2;
                                        break;
                                case PIX_FMT_YUV422:
                                        SHMDEB("new format YUV422\n");
                                        privBuf.pixel[0]=curr_pict+ctl->offset0;
                                        privBuf.pixel[1]=privBuf.pixel[1]=NULL;
                                        privBuf.stride[0]=ctl->stride0;
                                        privBuf.stride[1]=privBuf.stride[2]=0;
                                        break;
                                default:
                                        break;
                        }
                        SHMDEB("new pict %p shmid %d\n",curr_pict,curr_pict_shmid);
                };
        };
 
        if ( ctl->osd_shmid != curr_osd_shmid ) {
                if (osd_surface) {
                        shmdt(osd_surface);
                        osd_surface=NULL;
                };
                
                if ( ctl->osd_shmid != -1 ) {
                       SHMDEB("get new osd_surface %p\n",osd_surface);
                       if ( (osd_surface = (uint8_t *)shmat(ctl->osd_shmid,NULL,0)) 
                                       == (uint8_t*) -1 ) {
                               fprintf(stderr,"cShmVideoOut: Warning! Could not attatch to osd pict(%d)! Assuming no client connected.\n",
                                               ctl->osd_shmid);
                               ctl->pict_shmid = -1;
                               ctl->osd_shmid = -1;
                               ctl->attached = 0;
                               osd_surface=NULL;
                               return;
                       }
                       curr_osd_shmid=ctl->osd_shmid;
                };
	        SHMDEB("got new osd_surface %p\n",osd_surface);
        };
}

void cShmVideoOut::GetLockOsdSurface(uint8_t *&osd, int &stride,
                  bool *&dirtyLines) {
	SHMDEB("GetLockOsdSurface osd_surface %p osd_shmid %d\n",osd_surface,ctl->osd_shmid);

        CheckShmIDs();
        osd=osd_surface;
        stride=ctl->osd_stride;
        dirtyLines=NULL;
};

void cShmVideoOut::ClearOSD() {
        SHMDEB("ClearOsd\n");
        cVideoOut::ClearOSD();
        if (current_osdMode == OSDMODE_SOFTWARE)
                return;

        CheckShmIDs();
        if (osd_surface)
                memset (osd_surface, 0, ctl->osd_stride *ctl->osd_max_height);
        ctl->new_osd++;
        sem_sig_unlock(ctl->semid,PICT_SIG);
};

void cShmVideoOut::CommitUnlockOsdSurface() {
        SHMDEB("CommitOsd\n");
        ctl->new_osd++;        
        sem_sig_unlock(ctl->semid,PICT_SIG);
};

void cShmVideoOut::ProcessEvents() {
        // I don't know if there is a timeout mechanism (which would probably the better solution),
        // so we just signal every once and a while so that the client processes its events.
        SIGDEB("ProcessEvents sending signal\n");
        sem_sig_unlock(ctl->semid,PICT_SIG);
}

void cShmVideoOut::Suspend() {
        SHMDEB("Suspend shm server\n");
        if (osd_surface) {
                shmdt(osd_surface);
                osd_surface=NULL;
                curr_osd_shmid=-1;
        };
        
        if (curr_pict) {
                shmdt(curr_pict);
                curr_pict=NULL;
                curr_pict_shmid=-1;
        }
};

void cShmVideoOut::YUV(sPicBuffer *buf) {
  
        if (!ctl->attached) {
                setupStore->shouldSuspend=1;
                return;
        } else setupStore->shouldSuspend=0;

        SIGDEB("YUV trying to get a lock\n");
        sem_wait_lock(ctl->semid,PICT_MUT,SEM_UNDO);
        SIGDEB("YUV got a lock\n");

        CheckShmIDs();        
        
        if ( ctl->pict_shmid==-1 || !curr_pict ) {
                // unlock picture ctl
                sem_sig_unlock(ctl->semid,PICT_MUT,SEM_UNDO);
                SHMDEB(" no pict_shmid or no curr_pict unlock and return\n");
                return;
        };
        if ( cutTop != setupStore->cropTopLines ||
             cutBottom != setupStore->cropBottomLines ||
             cutLeft != setupStore->cropLeftCols ||
             cutRight != setupStore->cropRightCols) {
                cutTop = setupStore->cropTopLines;
                cutBottom = setupStore->cropBottomLines;
                cutLeft = setupStore->cropLeftCols;
                cutRight = setupStore->cropRightCols;
                ClearPicBuffer(&privBuf);
        }
        
        ctl->width= fwidth<ctl->max_width ? fwidth : ctl->max_width;
        ctl->height= fheight<ctl->max_height ? fheight : ctl->max_height;
        ctl->new_afd=current_afd;
        ctl->new_asp=GetAspect_F();
        if (OSDpresent && current_osdMode==OSDMODE_SOFTWARE) {

                CopyPicBufAlphaBlend(&privBuf,buf,
                                OsdPy,OsdPu,OsdPv,OsdPAlphaY,OsdPAlphaUV, OSD_FULL_WIDTH,
                                cutTop,cutBottom,cutLeft,cutRight); 
        } else {
                CopyPicBuf(&privBuf,buf,
                                cutTop,cutBottom,cutLeft,cutRight);
        };
        ctl->new_pict++;
        if (ctl->new_pict>30) {
                printf("new_pict > 30; assuming client died!\n");
                ctl->attached=0;
        };
        sem_sig_unlock(ctl->semid,PICT_MUT,SEM_UNDO);
        sem_sig_unlock(ctl->semid,PICT_SIG);
        SIGDEB("send signal\n");
        
};

#ifdef USE_SUBPLUGINS
/* ---------------------------------------------------------------------------
 */
extern "C" void *
SubPluginCreator(cSetupStore *s)
{
  return new cShmVideoOut(s);
}
#endif
