/*
 * softdevice plugin for VDR
 *
 * Copyright (C) 2005 Martin Wache
 *
 * This code is distributed under the terms and conditions of the
 * GNU GENERAL PUBLIC LICENSE. See the file COPYING for details.
 *
 * $Id: MacVdrClient.c,v 1.2 2007/05/10 19:57:19 wachm Exp $
 */

#include <signal.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>

#include "video-quartz.h"
#include "shm-common.h"
#include "utils.h"

//#define SHMDEB(out...) {printf("MCVDR[%04d]:",(int)(getTimeMilis() % 10000));printf(out);}

#ifndef SHMDEB
#define SHMDEB(out...)
#endif

ShmCtlBlock * ctl;
bool active=true;

void sig_handler(int signal) {
        printf("got signal, ending\n");
        active=false;
};

class cShmRemote : public cSoftRemote, public cThread {
        private:
                bool running;
        public:
                cShmRemote(const char *Name)
                        : cSoftRemote(Name), running(false)
                        {};

                ~cShmRemote()
                {};

                virtual bool PutKey(uint64_t Code, bool Repeat = false,
                                bool Release = false);

                virtual void Action();
};

bool cShmRemote::PutKey(uint64_t Code, bool Repeat,
                                bool Release) {
        if (ctl)
        {
                SHMDEB("get lock for the key\n");
                sem_wait_lock(ctl->semid,KEY_MUT);
                SHMDEB("got lock for the key\n");

                ctl->key=Code;

                // release lock
                sem_sig_unlock(ctl->semid,KEY_MUT);
                // signal new key
                sem_sig_unlock(ctl->semid,KEY_SIG);
                SHMDEB("signal new key\n");
        };
        return true;
};

void cShmRemote::Action() {
        running=true;
        while (running) {
                // I don't know if there is a timeout mechanism (which would probably the better solution),
                // so we just signal every once and a while so that the client processes its events.
                if (ctl)
                        sem_sig_unlock(ctl->semid,PICT_SIG);
                usleep(113000);
        };
};

int main(int argc, char **argv) {
        cSetupStore *SetupStore=NULL;
        cSetupSoftlog *softlog=new cSetupSoftlog();
        int pict_shmid_sav;
        int osd_shmid_sav;
        signal(SIGINT,sig_handler);
        signal(SIGQUIT,sig_handler);

        int ctl_shmid;
        key_t ctl_key=CTL_KEY;

        //int curr_pict_shmid;
        uint8_t *curr_pict=0;
        uint8_t *curr_osd=0;
        sPicBuffer picture;
        //uint8_t *pixel[4];

        if ((ctl_shmid = shmget(ctl_key, sizeof( ShmCtlBlock ), 0666)) < 0) {
                fprintf(stderr,"ctl_shmid error in shmget!\n");
                fprintf(stderr,"Check if the Vdr is running with the softdevice and the option \"-vo shm:\"!\n");
                exit(1);
        }

        if ( (ctl = (ShmCtlBlock *)shmat(ctl_shmid,NULL,0))
                        == (ShmCtlBlock *) -1 ) {
                fprintf(stderr,"ctl_shmid error attatching shm ctl %d!\n",ctl_shmid);
                exit(-1);
        };

        if ( (SetupStore = (cSetupStore *) shmat(ctl->setup_shmid,NULL,0))
                        == (cSetupStore *) -1 ) {
                fprintf(stderr,"Error attatching to setupStore shm (id: %d)!\n",
                                ctl->setup_shmid);
                exit(-1);
        };

        cQuartzVideoOut *vout=new cQuartzVideoOut(SetupStore,softlog);
        quartzRemote= new cShmRemote("softdevice-quartz");
        SetupStore->xvFullscreen=0;

        if ( !vout->Initialize() ) {
                fprintf(stderr,"Could not init video out!\n");
                delete vout;
                exit(-1);
        };

#if  0 
        // create a picture in shm
        ctl->max_width=736;
        ctl->max_height=576;
        ctl->stride0=ctl->max_width;
        ctl->stride2=ctl->stride1=ctl->max_width/2;
        ctl->format=PIX_FMT_YUV420P;

        if ( (pict_shmid_sav = ctl->pict_shmid = shmget(IPC_PRIVATE,
                                        ctl->max_width*ctl->max_height*2,
                                        IPC_CREAT | 0666)) < 0 ) {
                fprintf(stderr,"error creating  pict_shm! width %d height %d\n",
                        ctl->max_width,ctl->max_height);
                delete vout;
                exit(-1);
        };

        if ( (curr_pict = (uint8_t*)shmat(ctl->pict_shmid,NULL,0))
                        == (uint8_t*) -1 ) {
                fprintf(stderr,"error attatching shm ctl!\n");
                delete vout;
                exit(-1);
        };
/*
        // request removing after detaching
        if ( ctl->pict_shmid > 0)
                shmctl (ctl->pict_shmid, IPC_RMID, 0);
*/
        picture.format=PIX_FMT_YUV420P;
        picture.stride[0]=ctl->stride0;
        picture.stride[1]=ctl->stride1;
        picture.stride[2]=ctl->stride1;
        picture.pixel[0]=curr_pict;
        picture.pixel[1]=curr_pict+ctl->max_height*ctl->stride0;
        picture.pixel[2]=picture.pixel[1]+ctl->max_height/2*ctl->stride1;
        picture.edge_width=picture.edge_height=0;
#else
        // put quartz image into ctl shm
        ctl->max_width=IMAGE_WIDTH;
        ctl->max_height=IMAGE_HEIGHT;
        ctl->stride0=ctl->max_width*2;
        ctl->format=PIX_FMT_YUV422;

        pict_shmid_sav=ctl->pict_shmid=vout->pic_shm_id;

        picture.format=PIX_FMT_YUV422;
        picture.stride[0]=ctl->stride0;
        picture.pixel[0]=NULL;
        
#endif
        
        osd_shmid_sav=ctl->osd_shmid=vout->osd_shm_id;
        ctl->osd_stride=IMAGE_WIDTH*4;
        ctl->osd_max_width=ctl->osd_width=IMAGE_WIDTH;
        ctl->osd_max_height=ctl->osd_height=IMAGE_HEIGHT;
        ctl->osd_depth=32;
        
        ctl->attached=1;
        //printf("pict_shmid: %d max: (%d,%d), stride0: %d, stride2: %d\n",
        //                       ctl->pict_shmid, ctl->max_width,ctl->max_height,
        //                       ctl->stride0,ctl->stride2);

        ctl->key=NO_KEY;
        (dynamic_cast<cShmRemote*>(quartzRemote))->Start();        
        // wakeup remote thread to unsuspend video/audio
        sem_sig_unlock(ctl->semid,KEY_SIG);

        while (active && vout->IsOpen() ) {
                if (!ctl->attached) {
                        ctl->pict_shmid=pict_shmid_sav;
                        ctl->osd_shmid=osd_shmid_sav;
                        ctl->attached=1;
                };

                //SHMDEB("wait for signal\n");
                sem_wait_lock(ctl->semid,PICT_SIG);
                //SHMDEB("got signal\n");
                sem_wait_lock(ctl->semid,PICT_MUT,SEM_UNDO);
                //SHMDEB("got lock\n");

                if (ctl->new_pict) {
                        int width=ctl->width>ctl->max_width?
                                ctl->max_width:ctl->width;
                        int height=ctl->height>ctl->max_height?
                                ctl->max_height:ctl->height;

                        picture.width=ctl->width;
                        picture.height=ctl->height;
                        picture.dtg_active_format=ctl->new_afd;
                        picture.aspect_ratio=ctl->new_asp;

                        vout->CheckArea(width,height);
                        //vout->CheckAspect(ctl->new_afd,ctl->new_asp);
                        vout->DrawStill_420pl(&picture);
                        ctl->new_pict=0;
                };
                if (ctl->new_osd) {
                        SHMDEB("new osd picture\n");
/*                        if ( !vout->useShm ) {
                                uint8_t *dest_osd;
                                int osd_stride;
                                bool *dirtyLines;
                                vout->GetLockOsdSurface(dest_osd,osd_stride,
                                                dirtyLines);
                                memcpy(dest_osd,curr_osd,ctl->osd_height*osd_stride);
                        };
  */                      vout->CommitUnlockOsdSurface();
                        ctl->new_osd=0;
                };

                vout->GetOSDDimension(ctl->osd_width,ctl->osd_height,
                                      ctl->osd_xPan,ctl->osd_yPan);
                // consumed all pictures - set semaphore to 0
                sem_zero(ctl->semid,PICT_SIG);
                // unlock picture ctl
                sem_sig_unlock(ctl->semid,PICT_MUT,SEM_UNDO);
                //SHMDEB("released the lock\n");
                vout->ProcessEvents();
                // in case there are no semaphores (no vdr connected)
                usleep(3000);
        };
        ctl->attached=0;
        // request removing after detaching
        if ( ctl->pict_shmid > 0)
                shmctl (ctl->pict_shmid, IPC_RMID, 0);
        ctl->pict_shmid=-1;
        delete vout;
};

