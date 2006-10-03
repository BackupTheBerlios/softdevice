/*
 * sync-timer.c: A plugin for the Video Disk Recorder
 *
 * See the README file for copyright information and how to reach the author.
 *
 * $Id: sync-timer.c,v 1.5 2006/10/03 12:24:31 lucke Exp $
 */

#include <math.h>
#include <sched.h>

#include <vdr/plugin.h>

#include <sys/ioctl.h>
#include <linux/rtc.h>

#include "sync-timer.h"

//#define TIMDEB(out...) {fprintf(stderr,"sync-timer[%04d]:",(int)(getTimeMilis() % 10000));fprintf(stderr,out);}

#ifndef TIMDEB
#define TIMDEB(out...)
#endif

/* --- cRelTimer --------------------------------------------------------------
 */
int32_t cRelTimer::TimePassed()
{
  int64_t now;
  int32_t ret;

  now=GetTime();
  if ( now < lastTime ) {
    ret = (uint32_t) (now - lastTime + 60 *1000000); // untested
    TIMDEB("now %lld kleiner als lastTime %lld\n",now,lastTime);
  }
  else ret = now - lastTime;
  return ret;
}

/* ----------------------------------------------------------------------------
 */
int32_t cRelTimer::GetRelTime()
{
  int64_t now;
  int32_t ret;

  now=GetTime();
  if ( now < lastTime ) {
    ret = (uint32_t) (now - lastTime + 60 *1000000); // untested
    TIMDEB("now %lld kleiner als lastTime %lld\n",now,lastTime);
  }
  else ret = now - lastTime;
  lastTime=now;
  return ret;
}

/* --- cSigTimer --------------------------------------------------------------
 */
int cSigTimer::Sleep(int timeoutUS, int lowLimitUS)
{
  if ( timeoutUS < lowLimitUS ) {
    got_signal=false;
    return GetRelTime();
  }

  struct timeval tv;
  gettimeofday(&tv,NULL);
  struct timespec timeout;
  timeout.tv_nsec=(tv.tv_usec+timeoutUS-lowLimitUS);//*1000;
  timeout.tv_sec=tv.tv_sec + timeout.tv_nsec / 1000000;
  timeout.tv_nsec%=1000000;
  timeout.tv_nsec*=1000;
  pthread_mutex_lock(&mutex);
  int retcode=0;
  while ( retcode != ETIMEDOUT && !got_signal ) {
    retcode = pthread_cond_timedwait(&cond, &mutex, &timeout);
  }

  got_signal = false;
  pthread_mutex_unlock(&mutex);
  return GetRelTime();
}

/* ----------------------------------------------------------------------------
 */
void cSigTimer::Signal()
{
  pthread_mutex_lock(&mutex);
  got_signal=true;
  pthread_cond_broadcast(&cond);
  pthread_mutex_unlock(&mutex);
}

/* --- cSyncTimer -------------------------------------------------------------
 */
cSyncTimer::cSyncTimer(eSyncMode mode)
{
  syncMode = mode;
  rtcFd = -1;
  switch (mode)
  {
    case emRtcTimer:
      if ( (rtcFd = open("/dev/rtc",O_RDONLY)) < 0 )
        fprintf(stderr,"Could not open /dev/rtc \n");
      else
      {
          uint64_t irqp = 1024;

        if ( ioctl(rtcFd, RTC_IRQP_SET, irqp) < 0)
        {
          //fprintf(stderr,"Could not set irq period\n");
          close(rtcFd);
          rtcFd = -1;
        }
        else if ( ioctl( rtcFd, RTC_PIE_ON, 0 ) < 0)
        {
          //fprintf(stderr,"Error in rtc_pie on \n");
          close(rtcFd);
          rtcFd = -1;
        }// else fprintf(stderr,"Set up to use linux RTC\n");
      }
      if (rtcFd < 0)
        syncMode = emUsleepTimer;
      break;
    default:
      break;
  }
}

/* ----------------------------------------------------------------------------
 */
cSyncTimer::~cSyncTimer()
{
  if (rtcFd>=0)
    close(rtcFd);
}
/* ----------------------------------------------------------------------------
 */
void cSyncTimer::Signal()
{
  if ( syncMode==emSigTimer ) 
    cSigTimer::Signal();
  else got_signal=true;
};
/* ----------------------------------------------------------------------------
 */
void cSyncTimer::Sleep(int *timeoutUS, int lowLimitUS)
{
  switch(syncMode)
  {
    case emUsleepTimer: // usleep timer mode
      while ((*timeoutUS - lowLimitUS) > 2200 && !got_signal)
      {
        usleep (2200);
        *timeoutUS -= GetRelTime ();
      }
      got_signal=false;
      break;
    case emRtcTimer: // rtc timer mode
      while ((*timeoutUS - lowLimitUS) > 15000 && !got_signal)
      {
        usleep (10000);
        *timeoutUS -= GetRelTime();
      }
      while ((*timeoutUS - lowLimitUS) > 1200 && !got_signal)
      {
          uint32_t  ts;

        if (read(rtcFd, &ts, sizeof(ts)) <= 0)
        {
          close(rtcFd);
          rtcFd = -1;
          syncMode = emUsleepTimer;
        }
        *timeoutUS -= GetRelTime();
      }
      got_signal=false;
      break;
    case emSigTimer: // signal timer mode
      *timeoutUS -= cSigTimer::Sleep(*timeoutUS, lowLimitUS);
      break;
  }
}
