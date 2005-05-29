/*
 * sync-timer.h: A plugin for the Video Disk Recorder
 *
 * See the README file for copyright information and how to reach the author.
 *
 * $Id: sync-timer.h,v 1.2 2005/05/29 10:13:59 wachm Exp $
 */
#ifndef SYNCTIMER_H
#define SYNCTIMER_H
#include <sys/time.h>

//-------------------------cRelTimer-----------------------------------
class cRelTimer {
   private:
      int64_t lastTime;
      inline int64_t GetTime()
      {
        struct timeval tv;
        struct timezone tz;
        gettimeofday(&tv,&tz);
        return tv.tv_sec*1000000+tv.tv_usec;
      };

   public:
      cRelTimer() {lastTime=GetTime();};
      virtual ~cRelTimer() {};

      int32_t TimePassed();
      virtual int32_t GetRelTime();
      inline void Reset() { lastTime=GetTime(); };
};

//-------------------------cSigTimer-----------------------------------
class cSigTimer : public cRelTimer {
   private:
     pthread_mutex_t mutex;
     pthread_cond_t cond;
   protected:
     bool got_signal;

   public:
      cSigTimer() : cRelTimer()
      {
        pthread_mutex_init(&mutex, NULL);
        pthread_cond_init(&cond, NULL);
      };
      ~cSigTimer()
      {
        pthread_cond_broadcast(&cond); // wake up any sleepers
        pthread_cond_destroy(&cond);
        pthread_mutex_destroy(&mutex);
      };

      int Sleep( int timeoutUS );

      virtual void Signal(void);
};

enum eSyncMode { emUsleepTimer, emRtcTimer, emSigTimer };

//-------------------------cSyncTimer----------------------------------
class cSyncTimer : public cSigTimer {
  private:
    eSyncMode syncMode;
    int       rtcFd;

  public:
    cSyncTimer(eSyncMode mode);
    virtual ~cSyncTimer();
    
    virtual void Signal(void);      

    virtual void Sleep(int *timeoutUS);
};


#endif // SYNCTIMER_H
