/*
 * softdevice plugin for VDR
 *
 * Copyright (C) 2005 Martin Wache
 *
 * This code is distributed under the terms and conditions of the
 * GNU GENERAL PUBLIC LICENSE. See the file COPYING for details.
 *
 * $Id: shm-common.h,v 1.3 2006/02/04 10:25:39 wachm Exp $
 */
#ifndef __SHM_COMMON_H__
#define __SHM_COMMON_H__


#include <sys/types.h>
#include <sys/ipc.h> 
#include <sys/shm.h> 
#include <sys/sem.h> 

#include "vdr/keys.h"

#define CTL_KEY 5678 

union semun {
        int val;                  
        struct semid_ds *buf;     
        unsigned short *array;    
};


#define PICT_SIG 0
#define PICT_MUT 1

#define KEY_SIG 2
#define KEY_MUT 3

#define NO_KEY 0x000000

struct ShmCtlBlock {
        /* semaphores */
        int semid;
        
        /* picture control */
        int pict_shmid;
        
        int max_width;
        int max_height;        
        int width;
        int height;
        int new_afd;
        double new_asp;
        int stride0;
        int stride1;
        int stride2;
        int new_pict;

        /* osd layer */
        int osd_shmid;
        int osd_width;
        int osd_height;
        int osd_max_width;
        int osd_max_height;
        int osd_depth;
        int osd_stride;
        int new_osd;
        
        /* is a client attached */
        int attached;

        /* keypress events */
        uint64_t key;
};

inline void sem_wait_lock(int semid, int idx, int flag=0) 
{
        struct sembuf sem_op = { idx, -1, flag };
        
        semop(semid, &sem_op,1);
};

inline void sem_sig_unlock(int semid, int idx,int flag=0) 
{
        struct sembuf sem_op = { idx, 1, flag };
        
        semop(semid, &sem_op,1);
};
 
inline void sem_zero(int semid,int idx) {
        semun sem_val;
        sem_val.val = 0; 
        semctl(semid,idx, SETVAL, sem_val);
};

#endif // SHM_COMMON
