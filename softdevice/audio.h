/*
 * audio.h: A plugin for the Video Disk Recorder
 *
 * See the README file for copyright information and how to reach the author.
 *
 * $Id: audio.h,v 1.9 2005/10/06 21:28:11 lucke Exp $
 */
#ifndef AUDIO_H
#define AUDIO_H

#define ALSA_PCM_NEW_HW_PARAMS_API
#define ALSA_PCM_NEW_SW_PARAMS_API
#include <alsa/asoundlib.h>
#include <vdr/plugin.h>
#include "setup-softdevice.h"
#include "audio-ac3pt.h"

struct SampleContext {
   uint32_t channels;
   uint32_t samplerate;
   uint32_t pcm_fmt;
   uint32_t period_size;
   uint32_t buffer_size;
};

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
class cAlsaAudioOut : public cAudioOut  {
private:
  cMutex            handleMutex;
  snd_pcm_t         *handle;
  char              *device,
                    *ac3Device;
  volatile bool     paused;
  bool              ac3PassThrough,
                    ac3SpdifPro;
  SampleContext     oldContext;
  cAlsaAC3pt        ac3pt;

  bool  SetAC3PassThroughMode(bool on);
  void  Xrun(void);

protected:
public:
  cAlsaAudioOut(cSetupStore *setupStore);
  virtual ~cAlsaAudioOut();

  virtual void  Write(uchar *Data, int Length);
  virtual void  WriteAC3(uchar *Data, int Length);
  //virtual int SetParams(int channels, unsigned int samplerate);
  virtual int   SetParams(SampleContext &context);
  virtual int   GetDelay(void);
  virtual void  Pause(void);
  virtual void  Play(void);
  virtual void  SetVolume(int vol);
  virtual void  Suspend(void);
  virtual bool  Resume(void);
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
