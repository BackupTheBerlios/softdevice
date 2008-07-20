/*
 * audio-alsa.h: A plugin for the Video Disk Recorder
 *
 * See the README file for copyright information and how to reach the author.
 *
 * $Id: audio-alsa.h,v 1.6 2008/07/20 16:32:44 lucke Exp $
 */
#ifndef __AUDIO_ALSA_H__
#define __AUDIO_ALSA_H__

#define ALSA_PCM_NEW_HW_PARAMS_API
#define ALSA_PCM_NEW_SW_PARAMS_API

#include <sys/time.h>

#include <alsa/asoundlib.h>
#include <vdr/plugin.h>

#include "audio.h"
#include "audio-ac3pt.h"

/* ---------------------------------------------------------------------------
 */
class cAlsaAudioOut : public cAudioOut  {
private:
  cMutex            handleMutex;
  snd_pcm_t         *handle;
  char              *device,
                    *ac3Device,
                    chDevice [8];
  volatile bool     paused;
  bool              ac3PassThrough,
                    ac3SpdifPro,
                    useMmapWrite;
  SampleContext     oldContext;
  cAlsaAC3pt        ac3pt;

  bool  SetAC3PassThroughMode(bool on);
  void  Xrun(void);
  int scale_Factor;

protected:
public:
  cAlsaAudioOut();
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


#endif
