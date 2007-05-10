/*
 * audio.c: A plugin for the Video Disk Recorder
 *
 * See the README file for copyright information and how to reach the author.
 *
 * $Id: audio.c,v 1.28 2007/05/10 19:54:44 wachm Exp $
 */

#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>

#include "audio.h"
#include "utils.h"


//#define AUDIODEB(out...) {printf("AUDIO[%04d]:",(int)(getTimeMilis() % 10000));printf(out);}

#ifndef AUDIODEB
#define AUDIODEB(out...)
#endif

cAudioOut::~cAudioOut() {
}

/* ----------------------------------------------------------------------------
 */

void Scale(int16_t *Data, int size,int scale_Factor) 
{
  if (scale_Factor == 0x7FFF) // max. volume, don't change anything
          return;

  while (size>0) {
    register int32_t tmp=(int32_t)(*Data) * scale_Factor;
    *Data=(int16_t) (tmp>>15);
    Data++;
    size--;
  };
};

/* ---------------------------------------------------------------------------
 */
cDummyAudioOut::cDummyAudioOut()
{
  paused=false;
  dsyslog("[softdevice-audio-dummy] Device opened! Using dummy device -> no audio!");
}

/* ---------------------------------------------------------------------------
 */
void cDummyAudioOut::Write(uchar *Data, int Length)
{
}

/* ----------------------------------------------------------------------------
 */
void cDummyAudioOut::WriteAC3(uchar *Data, int Length)
{
}

/* ---------------------------------------------------------------------------
 */
void cDummyAudioOut::Pause(void)
{
  dsyslog("[softdevice-audio-dummy]: Should pause now");
  paused=true;
}

/* ---------------------------------------------------------------------------
 */
void cDummyAudioOut::Play(void)
{
  paused=false;
}

/* ---------------------------------------------------------------------------
 */
int cDummyAudioOut::GetDelay(void)
{
  return -1;
}

/* ---------------------------------------------------------------------------
 */
int cDummyAudioOut::SetParams(SampleContext &context)
{
  // not needed to set again
  if (currContext.samplerate == context.samplerate &&
      currContext.channels == context.channels ) {
    context=currContext;
    return 0;
  }
  currContext=context;

  return 0;
}

