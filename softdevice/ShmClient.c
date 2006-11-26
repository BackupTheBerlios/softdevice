/*
 * softdevice plugin for VDR
 *
 * Copyright (C) 2005 Martin Wache
 *
 * This code is distributed under the terms and conditions of the
 * GNU GENERAL PUBLIC LICENSE. See the file COPYING for details.
 *
 * $Id: ShmClient.c,v 1.20 2006/11/26 19:00:17 wachm Exp $
 */

#include <signal.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>

#include "video-xv.h"
#include "shm-common.h"
#include "utils.h"

//#define SHMDEB(out...) {printf("SHMCLIENT[%04d]:",(int)(getTimeMilis() % 10000));printf(out);}

#ifndef SHMDEB
#define SHMDEB(out...)
#endif

ShmCtlBlock * ctl;
bool active=true;

void sig_handler(int signal) {
        printf("got signal, ending\n");
        active=false;
};

class cShmRemote : public cSoftRemote, cThread {
        protected:
                bool running;
        public:
                cShmRemote(const char *Name)
                        : cSoftRemote(Name), running(false)
                        {};

                ~cShmRemote()
                { running=false; };

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
        cSetupStore SetupStore;
        SetupStore.xvFullscreen=0;
        cXvVideoOut *vout=new cXvVideoOut(&SetupStore);
        xvRemote= new cShmRemote("softdevice-xv");

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
                fprintf(stderr,"Check if vdr and the softdevice are running with the option -vo shm:\n");
                exit(1);
        }

        if ( (ctl = (ShmCtlBlock *)shmat(ctl_shmid,NULL,0))
                        == (ShmCtlBlock *) -1 ) {
                fprintf(stderr,"ctl_shmid error attatching shm ctl!\n");
                fprintf(stderr,"Check if vdr and the softdevice are running with the option -vo shm:\n");
                exit(-1);
        };
        
        if ( !vout->Initialize()  ) {
                fprintf(stderr,"Could not init video out!\n");
                exit(-1);
        };
       
        while (!vout->Reconfigure(SetupStore.pixelFormat) 
                        && SetupStore.pixelFormat < 3 )
                SetupStore.pixelFormat++;
        
        if ( vout->useShm && vout->xv_image ) {
                ctl->pict_shmid= vout->shminfo.shmid;
                ctl->max_width=vout->xv_image->width;
                ctl->max_height=vout->xv_image->height;
                ctl->format=vout->privBuf.format;
                switch (vout->GetFormat()) {
                        case FOURCC_I420:
                                ctl->offset0=vout->xv_image->offsets[0];
                                ctl->offset1=vout->xv_image->offsets[1];
                                ctl->offset2=vout->xv_image->offsets[2];
                                ctl->stride0=vout->xv_image->pitches[0];
                                ctl->stride1=vout->xv_image->pitches[1];
                                ctl->stride2=vout->xv_image->pitches[2];
                                break;
                        case FOURCC_YV12:
                                ctl->offset0=vout->xv_image->offsets[0];
                                ctl->offset1=vout->xv_image->offsets[2];
                                ctl->offset2=vout->xv_image->offsets[1];
                                ctl->stride0=vout->xv_image->pitches[0];
                                ctl->stride1=vout->xv_image->pitches[2];
                                ctl->stride2=vout->xv_image->pitches[1];
                                break;
                        case FOURCC_YUY2:
                                ctl->offset0=vout->xv_image->offsets[0];
                                ctl->offset1=ctl->offset2=0;
                                ctl->stride0=vout->xv_image->pitches[0];
                                ctl->stride1=ctl->stride2=0;
                                break;
                };
                
                picture.pixel[0]=picture.pixel[1]=picture.pixel[2]=NULL;
                picture.stride[0]=picture.stride[1]=picture.stride[2]=0;
                picture.edge_width=picture.edge_height=0;
                picture.max_width=ctl->max_width;
                picture.max_height=ctl->max_height;
        } else {
                // create a picture in shm
                ctl->max_width=736;
                ctl->max_height=576;
                ctl->stride0=ctl->max_width;
                ctl->stride2=ctl->stride1=ctl->max_width/2;

                if ( (ctl->pict_shmid = shmget(IPC_PRIVATE,
                                  ctl->max_width*ctl->max_height*2,
                                  IPC_CREAT | 0666)) < 0 ) {
                        fprintf(stderr,"error creating  pict_shm!\n");
                        exit(-1);
                };

                if ( (curr_pict = (uint8_t*)shmat(ctl->pict_shmid,NULL,0))
                                == (uint8_t*) -1 ) {
                        fprintf(stderr,"error attatching shm ctl!\n");
                        exit(-1);
                };

                // request removing after detaching
                if ( ctl->pict_shmid > 0)
                        shmctl (ctl->pict_shmid, IPC_RMID, 0);

                picture.stride[0]=ctl->stride0;
                picture.stride[1]=ctl->stride1;
                picture.stride[2]=ctl->stride1;
                picture.pixel[0]=curr_pict;
                picture.pixel[1]=curr_pict+ctl->max_height*ctl->stride0;
                picture.pixel[2]=picture.pixel[1]+ctl->max_height/2*ctl->stride1;
                picture.edge_width=picture.edge_height=0;
                picture.max_width=ctl->max_width;
                picture.max_height=ctl->max_height;
        }
        ctl->attached=1;
        //printf("pict_shmid: %d max: (%d,%d), stride0: %d, stride2: %d\n",
        //                       ctl->pict_shmid, ctl->max_width,ctl->max_height,
        //                       ctl->stride0,ctl->stride2);

        if (vout->useShm) {
                ctl->osd_shmid= vout->osd_shminfo.shmid;
        } else {
                if ( (ctl->osd_shmid = shmget(IPC_PRIVATE,
                                 vout->osd_image->bytes_per_line*
                                 vout->osd_max_width,
                                                IPC_CREAT | 0666)) < 0 ) {
                        fprintf(stderr,"error creating  osd_shm!\n");
                        exit(-1);
                };

                if ( (curr_osd = (uint8_t*)shmat(ctl->osd_shmid,NULL,0))
                                == (uint8_t*) -1 ) {
                        fprintf(stderr,"error attatching osd shm!\n");
                        exit(-1);
                };
        };
        ctl->osd_stride=vout->osd_image->bytes_per_line;
        ctl->osd_depth=vout->osd_image->bits_per_pixel;
        ctl->osd_max_width=vout->osd_max_width;
        ctl->osd_max_height=vout->osd_max_height;
        vout->GetOSDDimension(ctl->osd_width,ctl->osd_height,
                              ctl->osd_xPan,ctl->osd_yPan);
        ctl->colorkey=vout->GetOSDColorkey();
        //printf("osd_shmid %d stride %d\n",ctl->osd_shmid,ctl->osd_stride);

        ctl->key=NO_KEY;
        // wakeup remote thread to unsuspend video/audio
        sem_sig_unlock(ctl->semid,KEY_SIG);

        while (active) {
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
                        if ( vout->useShm ) {
                                vout->DrawStill_420pl(&picture);
                        } else {
                                vout->DrawStill_420pl(&picture);
                        };
                        ctl->new_pict=0;
                };
                if (ctl->new_osd) {
                        SHMDEB("new osd picture\n");
                        if ( !vout->useShm ) {
                                uint8_t *dest_osd;
                                int osd_stride;
                                bool *dirtyLines;
                                vout->GetLockOsdSurface(dest_osd,osd_stride,
                                                dirtyLines);
                                memcpy(dest_osd,curr_osd,ctl->osd_height*osd_stride);
                        };
                        vout->CommitUnlockOsdSurface();
                        ctl->new_osd=0;
                };
                vout->ProcessEvents();

                vout->GetOSDDimension(ctl->osd_width,ctl->osd_height,
                                      ctl->osd_xPan,ctl->osd_yPan);
                // consumed all pictures - set semaphore to 0
                sem_zero(ctl->semid,PICT_SIG);
                // unlock picture ctl
                sem_sig_unlock(ctl->semid,PICT_MUT,SEM_UNDO);
                //SHMDEB("released the lock\n");
                // in case there are no semaphores (no vdr connected)
                usleep(13000);
        };
        ctl->attached=0;
        ctl->pict_shmid=-1;
};

