/*
 * softdevice plugin for VDR
 *
 * Copyright (C) 2005 Martin Wache
 *
 * This code is distributed under the terms and conditions of the
 * GNU GENERAL PUBLIC LICENSE. See the file COPYING for details.
 *
 * $Id: VdrReplacements.c,v 1.2 2007/04/03 19:21:10 wachm Exp $
 */

#include "VdrReplacements.h"
#include <stdio.h>
#include <unistd.h>

cMutex::cMutex() {
        pthread_mutexattr_t attr;
        pthread_mutexattr_init(&attr);
#ifdef __APPLE__
        pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_ERRORCHECK);
#else
        pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_ERRORCHECK_NP);
#endif
        pthread_mutex_init(&mutex, &attr);
};

cMutex::~cMutex() {
        pthread_mutex_destroy(&mutex);
};

void cMutex::Lock() {
        pthread_mutex_lock(&mutex);
};

void cMutex::Unlock() {
        pthread_mutex_unlock(&mutex);
};

//-------------------------------------------------------------------------

void StartThread(cThread *thread) {
        thread->active=true;
        thread->Action();
        thread->active=false;
};

cThread::cThread() {
        active=false;
        childTid=0;
};

cThread::~cThread() {
        Cancel();
}; 

bool cThread::Start() {
        if (active)
                return false;

        active=true;
        if ( pthread_create(&childTid, NULL, (void *(*) (void*))&StartThread,
                                        (void *)this)!=0 ) {
                fprintf(stderr,"Error starting thread\n");
                active=false;
                return false;
        };

        pthread_detach(childTid);
        return true;
};

void cThread::Cancel(int TimeOut) {
        int timeoutms=TimeOut*1000;
        while (active && TimeOut>0) {
                usleep(5000);
                timeoutms-=5000;
        };

        if (active) {
                fprintf(stderr,"Thread %d won't end. Canceling it.\n",childTid);
                pthread_cancel(childTid);
                childTid=0;
                active=false;
        };
};

