/*
 * audio.c: A plugin for the Video Disk Recorder
 *
 * See the README file for copyright information and how to reach the author.
 *
 * $Id: audio.c,v 1.11 2005/03/19 22:01:21 lucke Exp $
 */

#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <math.h>

#define ALSA_PCM_NEW_HW_PARAMS_API
#define ALSA_PCM_NEW_SW_PARAMS_API
#include <alsa/asoundlib.h>
#include "audio.h"


#define PCM_FMT SND_PCM_FORMAT_S16_LE

cAudioOut::~cAudioOut() {
}

cAlsaAudioOut::cAlsaAudioOut(cSetupStore *setupStore) {
    if (strlen(setupStore->alsaDevice) == 0)
      strcpy (setupStore->alsaDevice, "default");
    dsyslog("[softdevice-audio] Opening alsa device %s",setupStore->alsaDevice);
    device = setupStore->alsaDevice;
    int err;
    paused=false;
    if ((err = snd_pcm_open(&handle, device, SND_PCM_STREAM_PLAYBACK, 0)) < 0) {
      dsyslog("[softdevice-audio] Playback open error: %s, %s FATAL exiting",
              device, snd_strerror(err));
      exit(1);
    }
    currContext.channels=0;
    currContext.samplerate=48000;
    dsyslog("[softdevice-audio] Device opened! Ready to play");
}


cAlsaAudioOut::~cAlsaAudioOut() {
    snd_pcm_close(handle);
}

void cAlsaAudioOut::Suspend() {
  snd_pcm_close(handle);
}

bool cAlsaAudioOut::Resume() {
   int err;
   printf("Device %s\n",device);
   if ((err = snd_pcm_open(&handle, device, SND_PCM_STREAM_PLAYBACK, 0)) < 0) {
     dsyslog("[softdevice-audio] Playback open error: %s, %s FATAL exiting",
             device, snd_strerror(err));
     return false;
   }
   //force setting of the parameters after resume 
   currContext.channels=0;
   return true;
};

void cAlsaAudioOut::Write(uchar *Data, int Length)
{
    int err;
    size_t size;
    size = Length/(2*currContext.channels);
  while (size) {
    while (paused) usleep(1000); // block
    err = snd_pcm_mmap_writei(handle,Data, size);
    if (err == -EAGAIN || (err >= 0 && (size_t)err < size)) {
      snd_pcm_wait(handle, 10000);
    } else if (err == -EPIPE) {
      Xrun();
      dsyslog("[softdevice-audio]: xrun");
    } else if (err == -ESTRPIPE) {
      //suspend();
      dsyslog("[softdevice-audio]: Suspend");
    } else if (err == -EINTR) {
      dsyslog ("[softdevice-audio]: EINTR");
      return;
    } else if (err < 0) {
      dsyslog("[softdevice-audio]: write error: %s FATAL exiting",
              snd_strerror(err));
      exit(EXIT_FAILURE);
    }
    size -=err;
  }
}

void cAlsaAudioOut::Pause(void) {
    //    snd_pcm_pause(handle,1);
    dsyslog("[softdevice-audio]: Should pause now");
    paused=true;
}

void cAlsaAudioOut::Play(void) {
//    snd_pcm_pause(handle,0);
    paused=false;
}

int cAlsaAudioOut::GetDelay(void) {
	snd_pcm_status_t *status;
	snd_pcm_status_alloca(&status);
	int res;
	if ((res = snd_pcm_status(handle, status))<0) {
    dsyslog("[softdevice-audio]: GetDelay status error: %s FATAL exiting",
            snd_strerror(res));
		exit(EXIT_FAILURE);
	}
    return snd_pcm_status_get_delay(status) *1000 / currContext.samplerate;
}

/* I/O error handler */
void cAlsaAudioOut::Xrun(void)
{
	snd_pcm_status_t *status;
	int res;
	snd_pcm_status_alloca(&status);
        printf("alsa-audio: Xrun\n");
	if ((res = snd_pcm_status(handle, status))<0) {
    dsyslog("[softdevice-audio]: Xrun status error: %s FATAL exiting",
            snd_strerror(res));
		exit(EXIT_FAILURE);
	}
	if (snd_pcm_status_get_state(status) == SND_PCM_STATE_XRUN) {
//		snd_pcm_status_get_trigger_tstamp(status, &tstamp);
		if ((res = snd_pcm_prepare(handle))<0) {
      dsyslog("[softdevice-audio]: Xrun prepare error: %s FATAL exiting",
              snd_strerror(res));
			exit(EXIT_FAILURE);
		}
		return;		/* ok, data should be accepted again */
	}

//	error("read/write error, state = %s", snd_pcm_state_name(snd_pcm_status_get_state(status)));
  dsyslog("[softdevice-audio]: read/write error FATAL exiting");
	exit(EXIT_FAILURE);
}

int cAlsaAudioOut::SetParams(SampleContext &context)
//int cAlsaAudioOut::SetParams(int channels, unsigned int samplerate)
{
      int err;

    // not needed to set again
    //if ((chn == channels) && (rate == samplerate)) return 0;
    if (currContext.samplerate == context.samplerate &&
        currContext.channels == context.channels ) {
      context=currContext;
      return 0;
    };
    printf("alsa-audio: SetParams\n");
    currContext=context;
    
    snd_pcm_close(handle);
    if ((err = snd_pcm_open(&handle,
                            device,
                            SND_PCM_STREAM_PLAYBACK, 0)) < 0) {
      dsyslog("[softdevice-audio] Playback reopen error: %s, %s FATAL exiting",
              device, snd_strerror(err));
      exit(1);
    }

    dsyslog ("[softdevice-audio] samplerate: %dHz, channels: #%d",
             currContext.samplerate, currContext.channels);
    //rate=samplerate;
    //chn=channels;
    snd_pcm_hw_params_t *params;
    snd_pcm_sw_params_t *swparams;
    snd_pcm_uframes_t xfer_align;
    snd_pcm_hw_params_alloca(&params);
    snd_pcm_sw_params_alloca(&swparams);

    err = snd_pcm_hw_params_any(handle, params);
    if (err < 0) {
      dsyslog("[softdevice-audio] Broken config for this PCM: no configurations available");
      exit(EXIT_FAILURE);
    }

    snd_pcm_access_mask_t *mask = (snd_pcm_access_mask_t *)alloca(snd_pcm_access_mask_sizeof());
    snd_pcm_access_mask_none(mask);
    snd_pcm_access_mask_set(mask, SND_PCM_ACCESS_MMAP_INTERLEAVED);
    snd_pcm_access_mask_set(mask, SND_PCM_ACCESS_MMAP_NONINTERLEAVED);
    snd_pcm_access_mask_set(mask, SND_PCM_ACCESS_MMAP_COMPLEX);
    err = snd_pcm_hw_params_set_access_mask(handle, params, mask);
    if (err < 0) {
      dsyslog("[softdevice-audio] Access type not available FATAL exiting");
      exit(EXIT_FAILURE);
    }

    err = snd_pcm_hw_params_set_format(handle, params, PCM_FMT);
    if (err < 0) {
      dsyslog("[softdevice-audio] Sample format non available FATAL exiting");
      exit(EXIT_FAILURE);
    }

    err = snd_pcm_hw_params_set_channels(handle, params,currContext.channels);
    if (err < 0) {
      dsyslog("[softdevice-audio] Channels count non available FATAL exiting");
      exit(EXIT_FAILURE);
    }

    err = snd_pcm_hw_params_set_rate_near(handle, params, &currContext.samplerate, 0);
    assert(err >= 0);
    /*
    if (rate != samplerate ) {
      dsyslog("[softdevice-audio] Rate %d Hz is not possible (and using instead %d Hz is not implemented) FATAL exiting",samplerate,rate);
      exit(1);
    }
*/
    // set period size
    snd_pcm_uframes_t bufferSize;
    snd_pcm_uframes_t periodSize;
    //periodSize = 8;
    periodSize = 4608 / 4;
    err = snd_pcm_hw_params_set_period_size_near(handle, params, &periodSize, 0);
    if ( err < 0)  {
      dsyslog("[softdevice-audio] Failed to set period size!");
      exit(1);
    }
    
    snd_pcm_uframes_t buffersize = 2 * 4608; 
    err = snd_pcm_hw_params_set_buffer_size_near(handle, params, &buffersize);
    if ( err < 0 ) {
      dsyslog("[softdevice-audio] Failed to set buffer size!");
      exit(1);
    }

    err = snd_pcm_hw_params(handle, params);
    if (err < 0) {
      dsyslog("[softdevice-audio] Unable to install hw params: FATAL exiting");
      exit(EXIT_FAILURE);
    }

    snd_pcm_hw_params_get_period_size(params, &periodSize, 0);
    snd_pcm_hw_params_get_buffer_size(params, &bufferSize);
    if (periodSize == bufferSize) {
      dsyslog("[softdevice-audio] Can't use period equal to buffer size (%lu == %lu) FATAL exiting",
              periodSize, bufferSize);
      exit(EXIT_FAILURE);
    }
    dsyslog("[softdevice-audio] Period size %lu Buffer size %lu",
            periodSize, bufferSize);

    currContext.buffer_size=bufferSize;
    currContext.period_size=periodSize;  
    
    snd_pcm_sw_params_current(handle, swparams);
    err = snd_pcm_sw_params_get_xfer_align(swparams, &xfer_align);
    if (err < 0) {
      dsyslog("[softdevice-audio] Unable to obtain xfer align FATAL exiting");
      exit(EXIT_FAILURE);
    }
    dsyslog("[softdevice-audio] Hardware initialized");

    context=currContext;
    return 0;
}

/* ----------------------------------------------------------------------------
 */
void cAlsaAudioOut::SetVolume (int vol)
{
    int                   err;
    long                  mixerMin, mixerMax,
                          setVol;
    double                mixerRange,
                          volPercent;
    char                  *mName = "PCM",
                          *cardName = "default";
    snd_mixer_t           *mHandle;
    snd_mixer_elem_t      *mElem;
    snd_mixer_selem_id_t  *sId;

  snd_mixer_selem_id_alloca(&sId);
  snd_mixer_selem_id_set_name(sId, mName);
  if ((err = snd_mixer_open(&mHandle, 0)) < 0) {
      static int once = 0;
    if (!once) {
      dsyslog("[softdevice-audio]: cannot open mixer: %s (%s)",
              mName,snd_strerror(err));
      once = 1;
    }
    return;
  }

  snd_mixer_attach(mHandle, cardName);
  snd_mixer_selem_register(mHandle, NULL, NULL);
  snd_mixer_load(mHandle);
  mElem = snd_mixer_find_selem(mHandle,sId);
  /* --------------------------------------------------------------------------
   * some cards don't have any master volume. so check return value
   */
  if (mElem)
  {
    snd_mixer_selem_get_playback_volume_range(mElem,&mixerMin,&mixerMax);
    mixerRange = mixerMax - mixerMin;
    volPercent = (double) vol / 255.0;
    setVol = (int) (((double)mixerMin+(mixerRange*volPercent))+0.5);
    snd_mixer_selem_set_playback_volume(mElem,SND_MIXER_SCHN_FRONT_LEFT,setVol);
    snd_mixer_selem_set_playback_volume(mElem,SND_MIXER_SCHN_FRONT_RIGHT,setVol);
  }

  snd_mixer_close(mHandle);
}

/* ---------------------------------------------------------------------------
 */
cDummyAudioOut::cDummyAudioOut(cSetupStore *setupStore)
{
  paused=false;
  dsyslog("[softdevice-audio-dummy] Device opened! Ready to play");
}

/* ---------------------------------------------------------------------------
 */
void cDummyAudioOut::Write(uchar *Data, int Length)
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
  return 0;
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

