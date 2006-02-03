/*
 * softdevice plugin for VDR
 *
 * Copyright (C) 2005 Martin Wache
 *
 * This code is distributed under the terms and conditions of the
 * GNU GENERAL PUBLIC LICENSE. See the file COPYING for details.
 *
 * $Id: ShmClient.c,v 1.2 2006/02/03 22:34:54 wachm Exp $
 */

#include <signal.h>

#include "video-xv.h"
#include "shm-common.h"
#include "vdr/keys.h"
#include "utils.h"

#define SHMDEB(out...) {printf("SHMCLIENT[%04d]:",(int)(getTimeMilis() % 10000));printf(out);}

#ifndef SHMDEB
#define SHMDEB(out...)
#endif

ShmCtlBlock * ctl;
bool active=true;

void sig_handler(int signal) {
        printf("got signal, ending\n");
        active=false;
};

class cShmXvRemote : public cXvRemote {
        public:  
                cShmXvRemote(const char *Name, cXvVideoOut *vout)
                        : cXvRemote(Name,vout) 
                        {};
                        
                ~cShmXvRemote()
                {};

                virtual void   PutKey (KeySym key);
};

void cShmXvRemote::PutKey(KeySym key) {
        if (ctl) 
        {
                SHMDEB("get lock for the key\n");
                sem_wait_lock(ctl->semid,KEY_MUT);
                SHMDEB("got lock for the key\n");
                
                ctl->key=(uint64_t) key;

                // release lock
                sem_sig_unlock(ctl->semid,KEY_MUT);
                // signal new key
                sem_sig_unlock(ctl->semid,KEY_SIG);
                SHMDEB("signal new key\n");
        };

};

int main(int argc, char **argv) {
        cSetupStore SetupStore;
        cXvVideoOut *vout=new cXvVideoOut(&SetupStore);
        xvRemote= new cShmXvRemote ("softdevice-xv",vout);
        
        signal(SIGINT,sig_handler);
        signal(SIGQUIT,sig_handler);
        
        int ctl_shmid;
        key_t ctl_key=CTL_KEY;
 
        int curr_pict_shmid;
        uint8_t *curr_pict;
        uint8_t *pixel[4];
       
        if ((ctl_shmid = shmget(ctl_key, sizeof( ShmCtlBlock ), 0666)) < 0) {
                fprintf(stderr,"ctl_shmid error in shmget!\n");
                exit(1);
        }
        
        if ( (ctl = (ShmCtlBlock *)shmat(ctl_shmid,NULL,0)) 
                        == (ShmCtlBlock *) -1 ) {
                fprintf(stderr,"ctl_shmid error attatching shm ctl!\n");
                exit(-1);
        };

        
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

        // request removeing after detaching
        if ( ctl->pict_shmid > 0)
                shmctl (ctl->pict_shmid, IPC_RMID, 0);
 
        pixel[0]=curr_pict;
        pixel[1]=curr_pict+ctl->height*ctl->stride0;
        pixel[2]=pixel[1]+ctl->height/2*ctl->stride1;

        ctl->attached=1;
        
        if ( !vout->Initialize() || !vout->Reconfigure(FOURCC_YV12) ) {
                fprintf(stderr,"Could not init video out!\n");
                exit(-1);
        };
        xvRemote->XvRemoteStart();

        ctl->osd_shmid= vout->osd_shminfo.shmid;
        ctl->osd_stride=vout->osd_image->bytes_per_line;
        ctl->osd_depth=vout->osd_image->bits_per_pixel;
        vout->GetOSDDimension(ctl->osd_width,ctl->osd_height);
        printf("osd_shmid %d stride %d\n",ctl->osd_shmid,ctl->osd_stride);
        
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

                        vout->CheckArea(width,height);
                        vout->CheckAspect(ctl->new_afd,ctl->new_asp);
                        vout->YUV(pixel[0],pixel[1],pixel[2],
                                        width,height,
                                        ctl->stride0,ctl->stride1);
                        ctl->new_pict=0;
                };
                if (ctl->new_osd) {
                        vout->CommitUnlockOsdSurface();
                        ctl->new_osd=0;
                };
                
                vout->GetOSDDimension(ctl->osd_width,ctl->osd_height);
                // consumed all pictures - set semaphore to 0
                sem_zero(ctl->semid,PICT_SIG);
                // unlock picture ctl
                sem_sig_unlock(ctl->semid,PICT_MUT,SEM_UNDO);
                //SHMDEB("released the lock\n");
                
        };
        ctl->attached=0;
        ctl->pict_shmid=-1;
};

