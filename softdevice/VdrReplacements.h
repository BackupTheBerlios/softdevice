/*
 * softdevice plugin for VDR
 *
 * Copyright (C) 2005 Martin Wache
 *
 * This code is distributed under the terms and conditions of the
 * GNU GENERAL PUBLIC LICENSE. See the file COPYING for details.
 *
 * $Id: VdrReplacements.h,v 1.3 2006/12/03 18:26:21 lucke Exp $
 */

#ifndef __VDRREPLACEMENTS_H__
#define __VDRREPLACEMENTS_H__

#include <pthread.h>
#include <stdio.h>
#include <stdint.h>

#define VDRVERSNUM 10308

#define esyslog(out...) do { fprintf(stderr,"esyslog:"); \
                                    fprintf(stderr,out);\
                                    fprintf(stderr,"\n"); } while (0)
#define dsyslog(out...) do { fprintf(stderr,"dsyslog:"); \
                                    fprintf(stderr,out);\
                                    fprintf(stderr,"\n"); } while (0)
#define isyslog(out...) do { fprintf(stdout,"isyslog:"); \
                                    fprintf(stdout,out);\
                                    fprintf(stdout,"\n"); } while (0)

#define tr(out) out

typedef int eKeys;

class cSoftRemote {
  public: 
          cSoftRemote(const char *Name) {};
          virtual ~cSoftRemote()
          {};
          const char *Name() 
          {return NULL; };
          virtual bool PutKey(uint64_t Code, bool Repeat = false, 
                          bool Release = false)
          {return true;};
};

class cMutex {
        private:
                pthread_mutex_t mutex;

        public:
                cMutex();
                ~cMutex();

                void Lock();
                void Unlock();

};

class cThread {
        friend void StartThread(cThread *);
        private:
                bool active;
                pthread_t childTid;
        public:
                cThread();
                virtual ~cThread();

                bool Start();
                void Cancel( int Timeout = 0 );

                virtual void Action()
                {};
};

        

#endif
