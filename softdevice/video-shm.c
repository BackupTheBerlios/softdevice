/*
 * softdevice plugin for VDR
 *
 * Copyright (C) 2005 Martin Wache
 *
 * This code is distributed under the terms and conditions of the
 * GNU GENERAL PUBLIC LICENSE. See the file COPYING for details.
 *
 * $Id: video-shm.c,v 1.1 2006/01/17 20:45:46 wachm Exp $
 */

#include "video-shm.h"
#include "utils.h"
#include "SoftOsd.h"

#define SHMDEB(out...) {printf("SHMSERV[%04d]:",(int)(getTimeMilis() % 10000));printf(out);}

#ifndef SHMDEB
#define SHMDEB(out...)
#endif
  
cShmRemote::~cShmRemote(){
        SHMDEB("~cShmRemote\n");
        Stop();
        SHMDEB("~cShmRemote returned from cancel\n");
};

void cShmRemote::Action(void) {
        while (active) {
                SHMDEB("cShmRemote trying to get a lock\n");
                sem_wait_lock(vout->ctl->semid,KEY_MUT);
                SHMDEB("cShmRemote got lock\n");
                
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

                SHMDEB("wait for a new key\n");
                sem_wait_lock(vout->ctl->semid,KEY_SIG);
        };
        SHMDEB("ShmRemote thread ended\n");
};


/*----------------------------------------------------------------------------*/
cShmVideoOut::cShmVideoOut(cSetupStore *setupStore) 
        : cVideoOut(setupStore) {
        ctl_key = CTL_KEY;
        bool Clear_Ctl=false;

        // first try to get an existing ShmCltBlock
        if  ( (ctl_shmid = shmget(ctl_key,sizeof(ShmCtlBlock), 0666)) >= 0 ) {
                fprintf(stderr,"got ctl_shmid %d shm ctl!\n",ctl_shmid);
        } else if ( (ctl_shmid = shmget(ctl_key,sizeof(ShmCtlBlock), 
                                        IPC_CREAT | 0666)) >= 0 ) {
                fprintf(stderr,"created ctl_shmid %d shm ctl!\n",ctl_shmid);
                // created ShmCltBlock, clear it
                Clear_Ctl=true;
        } else {
                fprintf(stderr,"error creating shm ctl!\n");
                exit(-1);
        };

        // attach to the control block
        if ( (ctl = (ShmCtlBlock*)shmat(ctl_shmid,NULL,0)) 
                        == (ShmCtlBlock*) -1 ) {
                fprintf(stderr,"error attatching shm ctl!\n");
                exit(-1);
        };

        if (Clear_Ctl) {
                ctl->semid=-1;
                ctl->pict_shmid=-1;
                ctl->attached = 0;
        };

        if ( ctl->semid == -1 ) {
                // create semaphores
                if ( (ctl->semid = semget(IPC_PRIVATE, 4, 0666 | IPC_CREAT) )
                                == -1) {
                        fprintf(stderr,"error creating semaphore set!\n");
                        exit(-1);
                };
                printf("created semaphores id %d\n",ctl->semid);
        };

        semun sem_val;
        sem_val.val = 0; // nothing available
        if ( semctl(ctl->semid,PICT_SIG, SETVAL, sem_val) == -1 ) {
                printf("error resetting semaphore PICT_SIG\n");
                exit(-1);
        };
        if ( semctl(ctl->semid,KEY_SIG,SETVAL,sem_val) == -1 ) {
                printf("error resetting semaphore KEY_SIG\n");
                exit(-1);
        };
        
        sem_val.val = 1; // not locked
        if ( semctl(ctl->semid,PICT_MUT, SETVAL, sem_val) == -1 ) {
                printf("error resetting semaphore PICT_MUT\n");
                exit(-1);
        };
        if ( semctl(ctl->semid,KEY_MUT,SETVAL,sem_val) == -1 ) {
                printf("error resetting semaphore KEY_MUT\n");
                exit(-1);
        };
   
        curr_pict_shmid=0;
        //ctl->pict_shmid=-1;
        curr_pict=NULL;
        ctl->key=kNone;
        remote = new cShmRemote("softdevice-xv",this);
};

cShmVideoOut::~cShmVideoOut() {
        // stop&delete remote
        remote->Stop();
        
        semun sem_val;
        // remove semaphore 
        if ( semctl(ctl->semid, 0, IPC_RMID, sem_val ) == -1) {
            fprintf(stderr,"error removeing semaphore set!\n");
            exit(1);
        }
        ctl->semid=-1;
        
        if (curr_pict)
                shmdt(curr_pict);
        curr_pict=NULL;
/*
        if ( ctl_shmid > 0)
                shmctl (ctl_shmid, IPC_RMID, 0);
*/
};

void cShmVideoOut::RefreshOSD(cSoftOsd *Osd,bool RefreshAll)
{
    if (current_osdMode==OSDMODE_SOFTWARE)
	    Osd->CopyToBitmap(OsdPy,OsdPv,OsdPu,OsdPAlphaY,OsdPAlphaUV,
                            OSD_FULL_WIDTH,OSD_FULL_WIDTH/2,
			    OsdWidth,OsdHeight,RefreshAll);
};

void cShmVideoOut::YUV(uint8_t *Py, uint8_t *Pu, uint8_t *Pv,
                     int Width, int Height, int Ystride, int UVstride) {
        OsdRefreshCounter=0; 

        if (!ctl->attached) {
                setupStore->shouldSuspend=1;
                return;
        } else setupStore->shouldSuspend=0;

        //SHMDEB("trying to get a lock\n");
        sem_wait_lock(ctl->semid,PICT_MUT);
        //SHMDEB("got a lock\n");
        
        if ( ctl->pict_shmid != curr_pict_shmid ) {
                if (curr_pict) {
                        shmdt(curr_pict);
                        curr_pict=NULL;
                };
                
                if ( ctl->pict_shmid != -1 ) {
                       if ( (curr_pict = (uint8_t *)shmat(ctl->pict_shmid,NULL,0)) 
                                       == (uint8_t*) -1 ) {
                               fprintf(stderr,"error attatching shm pict! Assumeing no client connected\n");
                               ctl->pict_shmid = -1;
                               ctl->attached = 0;
                               curr_pict=NULL;
                               sem_sig_unlock(ctl->semid,PICT_MUT);
                               return;
                       } 
                       pixels[0]=curr_pict;
                       pixels[1]=curr_pict+ctl->height*ctl->stride0;
                       pixels[2]=pixels[1]+ctl->height/2*ctl->stride1;
                       curr_pict_shmid=ctl->pict_shmid;
                };
        };

        
        if (!ctl->pict_shmid || !curr_pict) {
                // unlock picture ctl
                sem_sig_unlock(ctl->semid,PICT_MUT);
                SHMDEB(" no pict_shmid or no curr_pict unlock and return\n");
                return;
        };
      
        int width= ctl->max_width < Width ? ctl->max_width : Width;
        int height= ctl->max_height < Height ? ctl->max_height : Height;    
        
        ctl->width=fwidth;
        ctl->height=fheight;
        ctl->new_afd=current_afd;
        ctl->new_asp=GetAspect_F();
        if (OSDpresent && current_osdMode==OSDMODE_SOFTWARE) {
                
                for (int i = 0; i < height; i++)
                        AlphaBlend(pixels [0] + i * ctl->stride0,
                                       OsdPy+i*OSD_FULL_WIDTH,
                                       Py + i * Ystride,
                                       OsdPAlphaY+i*OSD_FULL_WIDTH,
                                       width );
                for (int i = 0; i < height / 2; i++)
                        AlphaBlend(pixels [1] + i * ctl->stride1,
                                        OsdPu+i*OSD_FULL_WIDTH/2,
                                        Pu + i * UVstride ,
                                        OsdPAlphaUV+i*OSD_FULL_WIDTH/2,
                                        width/2);
                for (int i = 0; i < height / 2 ; i++)
                        AlphaBlend(pixels [2] + i * ctl->stride2 ,
                                        OsdPv+i*OSD_FULL_WIDTH/2,
                                        Pv + i * UVstride,
                                        OsdPAlphaUV+i*OSD_FULL_WIDTH/2,
                                        width / 2);
        } else {
                
                for (int i = 0; i < height; i++)
                        fast_memcpy(pixels [0] + i * ctl->stride0,
                                        Py + i * Ystride,
                                        width );
                for (int i = 0; i < height / 2; i++)
                        fast_memcpy (pixels [1] + i * ctl->stride1,
                                        Pu + i * UVstride,
                                        width / 2);
                for (int i = 0; i < height / 2 ; i++)
                        fast_memcpy (pixels [2] + i * ctl->stride2 ,
                                        Pv + i * UVstride,
                                        width / 2);

        };
        ctl->new_pict++;
        if (ctl->new_pict>30) {
                printf("new_pict > 30; assueming client died!\n");
                ctl->attached=0;
        };
        sem_sig_unlock(ctl->semid,PICT_MUT);
        sem_sig_unlock(ctl->semid,PICT_SIG);
        //SHMDEB("send signal\n");
        
};

void cShmVideoOut::GetOSDDimension(int &OsdWidth,int &OsdHeight) {
   if (current_osdMode==OSDMODE_SOFTWARE) {
                OsdWidth=swidth;
                OsdHeight=sheight;
    };
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