/*
 * sync-timer.h: A plugin for the Video Disk Recorder
 *
 * See the README file for copyright information and how to reach the author.
 *
 * $Id: sync-timer.h,v 1.5 2007/03/25 08:54:12 wachm Exp $
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
        gettimeofday(&tv,NULL);
        return (int64_t)tv.tv_sec*1000000+(int64_t)tv.tv_usec;
      };

   public:
      cRelTimer() {lastTime=GetTime();};
      virtual ~cRelTimer() {};

      virtual int32_t GetRelTime( bool updateNow=true );
      // to avoid overflows the max. time is limited to 10 min

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

      int Sleep(int timeoutUS, int lowLimitUS = 0);

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

    virtual void Sleep(int *timeoutUS, int lowLimitUS = 0);
};


#endif // SYNCTIMER_H
