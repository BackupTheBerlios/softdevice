/*
 * audio.h: A plugin for the Video Disk Recorder
 *
 * See the README file for copyright information and how to reach the author.
 *
 * $Id: audio.h,v 1.13 2007/03/12 20:12:48 wachm Exp $
 */
#ifndef AUDIO_H
#define AUDIO_H

#include <math.h>

#include <vdr/plugin.h>
#include "setup-softdevice.h"

struct SampleContext {
   uint32_t channels;
   uint32_t samplerate;
   uint32_t pcm_fmt;
   uint32_t period_size;
   uint32_t buffer_size;
};


void Scale(int16_t *Data, int size,int scale_Factor);
// scaling of auido data (changing the volume)

static inline int CalcScaleFactor(int vol) 
{ return (vol <= 0 ? 0 
          : (int) (pow(10.0, -2.5+2.5*vol/256.0)*0x7FFF)); };

/* ---------------------------------------------------------------------------
 * Abstract class for an audio device
 */
class cAudioOut  {
private:
protected:
  SampleContext currContext;
public:
  virtual ~cAudioOut();
  virtual void Write(uchar *Data, int Length)=0;
  virtual void WriteAC3(uchar *Data, int Length)=0;
  // length should always be a multiple of 4
  virtual int SetParams(SampleContext &context)=0;
  virtual int GetDelay(void)=0; // returns delay in 0.1 ms
  virtual void Pause(void)=0;
  virtual void Play(void)=0;
  virtual void SetVolume(int vol)=0;
  virtual void Suspend(void)
  {return;};
  virtual bool Resume(void)
  {return true;};
};

/* ---------------------------------------------------------------------------
 */
class cDummyAudioOut : public cAudioOut  {
private:
  volatile bool paused;
public:
  cDummyAudioOut(cSetupStore *setupStore);
  virtual ~cDummyAudioOut() { return; };
  virtual void Write(uchar *Data, int Length);
  virtual void WriteAC3(uchar *Data, int Length);
  virtual int SetParams(SampleContext &context);
  virtual int GetDelay(void);
  virtual void Pause(void);
  virtual void Play(void);
  virtual void SetVolume(int vol) { return; };
};
#endif
