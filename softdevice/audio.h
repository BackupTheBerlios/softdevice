/*
 * audio.h: A plugin for the Video Disk Recorder
 *
 * See the README file for copyright information and how to reach the author.
 *
 * $Id: audio.h,v 1.4 2005/03/10 21:05:56 lucke Exp $
 */
#ifndef AUDIO_H
#define AUDIO_H

#define ALSA_PCM_NEW_HW_PARAMS_API
#define ALSA_PCM_NEW_SW_PARAMS_API
#include <alsa/asoundlib.h>
#include <vdr/plugin.h>
#include "setup-softdevice.h"

/* ---------------------------------------------------------------------------
 * Abstract class for an audio device
 */
class cAudioOut  {
private:
protected:
public:
  virtual ~cAudioOut();
  virtual void Write(uchar *Data, int Length)=0;
  // length should always be a multiple of 4
  virtual int SetParams(int channels, unsigned int samplerate)=0;
  virtual int GetDelay(void)=0; // returns delay in ms
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
  void Xrun(void);
  unsigned int rate;
  int chn;
  snd_pcm_uframes_t bufferSize;
  snd_pcm_uframes_t periodSize;
  snd_pcm_t *handle;
  char *device;
  volatile bool paused;
protected:
public:
  cAlsaAudioOut(cSetupStore *setupStore);
  virtual ~cAlsaAudioOut();
  virtual void Write(uchar *Data, int Length);
  virtual int SetParams(int channels, unsigned int samplerate);
  virtual int GetDelay(void);
  virtual void Pause(void);
  virtual void Play(void);
  virtual void SetVolume(int vol);
  virtual void Suspend(void);
  virtual bool Resume(void);
};

/* ---------------------------------------------------------------------------
 */
class cDummyAudioOut : public cAudioOut  {
private:
  unsigned int rate;
  int chn;
  volatile bool paused;
public:
  cDummyAudioOut(cSetupStore *setupStore);
  virtual ~cDummyAudioOut() { return; };
  virtual void Write(uchar *Data, int Length);
  virtual int SetParams(int channels, unsigned int samplerate);
  virtual int GetDelay(void);
  virtual void Pause(void);
  virtual void Play(void);
  virtual void SetVolume(int vol) { return; };
};
#endif
