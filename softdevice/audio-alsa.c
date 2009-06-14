/*
 * audio-alsa.c: A plugin for the Video Disk Recorder
 *
 * See the README file for copyright information and how to reach the author.
 *
 * $Id: audio-alsa.c,v 1.11 2009/06/14 18:02:48 lucke Exp $
 */
#include "audio-alsa.h"

#define ALSA_PCM_NEW_HW_PARAMS_API
#define ALSA_PCM_NEW_SW_PARAMS_API
#include <alsa/asoundlib.h>

#define PCM_FMT SND_PCM_FORMAT_S16_LE

//#define AUDIODEB(out...) {printf("AUDIO-ALSA[%04d]:",(int)(getTimeMilis() % 10000));printf(out);}

#ifndef AUDIODEB
#define AUDIODEB(out...)
#endif


/*--------------------------------------------------------------------------
*/
cAlsaAudioOut::cAlsaAudioOut() {
    int err;

    if (strlen(setupStore->alsaDevice) == 0)
      strcpy (setupStore->alsaDevice, "default");
    dsyslog("[softdevice-audio] Opening alsa device %s",setupStore->alsaDevice);
    device = setupStore->alsaDevice;

    if (strlen(setupStore->alsaAC3Device) == 0)
      strcpy (setupStore->alsaAC3Device, "hw:0,1");
    dsyslog("[softdevice-audio] Using alsa AC3 device %s",
            setupStore->alsaAC3Device);
    ac3Device = setupStore->alsaAC3Device;

    paused=false;
    ac3PassThrough = false;
    ac3SpdifPro = false;

    if ((err = snd_pcm_open(&handle, device, SND_PCM_STREAM_PLAYBACK, 0)) < 0) {
      esyslog("[softdevice-audio] Playback open error: %s, %s FATAL exiting",
              device, snd_strerror(err));
      exit(1);
    }
    oldContext.channels = currContext.channels=0;
    oldContext.samplerate = currContext.samplerate=48000;
    dsyslog("[softdevice-audio] Device opened! Ready to play");
    scale_Factor=0x7FFF;
}

/* ----------------------------------------------------------------------------
 */
cAlsaAudioOut::~cAlsaAudioOut()
{
  if (handle)
    snd_pcm_close(handle);
  handle = NULL;
}

/* ----------------------------------------------------------------------------
 */
void cAlsaAudioOut::Suspend()
{
  if (handle)
    snd_pcm_close(handle);
  handle = NULL;
}

/* ----------------------------------------------------------------------------
 */
bool cAlsaAudioOut::Resume() {
   int err;
   AUDIODEB("Device %s\n",device);
   if ((err = snd_pcm_open(&handle, device, SND_PCM_STREAM_PLAYBACK,SND_PCM_NONBLOCK )) < 0) {
     esyslog("[softdevice-audio] Playback open error: %s, %s ",
             device, snd_strerror(err));
     return false;
   }
   //force setting of the parameters after resume
   currContext.channels=0;
   return true;
};

/* ----------------------------------------------------------------------------
 */

void cAlsaAudioOut::Write(uchar *Data, int Length)
{
    int     err;
    size_t  size;

  AUDIODEB("Write %p %d\n",Data,Length);

  if (!handle)
          return;

  handleMutex.Lock();
  if (ac3PassThrough)
  {
    ac3PassThrough = false;
    SetAC3PassThroughMode(ac3PassThrough);
  }
  handleMutex.Unlock();

  if (!currContext.channels)
    size = Length / (2 * 2);
  else
    size = Length/(2*currContext.channels);

  // change the volume without mixer
  if (!setupStore->useMixer)
    Scale((int16_t*)Data,Length/2,scale_Factor);

  while (size) {
    while (paused) usleep(1000); // block

    AUDIODEB("pcm%s_writei  handle %p Data %p size %d\n",
             (useMmapWrite) ? "_mmap" : "",
             handle, Data, size);

    if (useMmapWrite)
      err = snd_pcm_mmap_writei(handle, Data, size);
    else
      err = snd_pcm_writei(handle, Data, size);

    AUDIODEB("pcm%s_writei return value %d Data %p size %d \n",
             (useMmapWrite) ? "_mmap" : "",
             err, Data, size);

    if (err == -EAGAIN || (err >= 0 && (size_t)err < size)) {
      AUDIODEB("wait \n");
      snd_pcm_wait(handle, 1000);
    } else if (err == -EPIPE) {
      AUDIODEB("Xrun \n");
      Xrun();
      dsyslog("[softdevice-audio]: xrun");
    } else if (err == -ESTRPIPE) {
      //suspend();
      AUDIODEB("Suspend \n");
      dsyslog("[softdevice-audio]: Suspend");
    } else if (err == -EINTR) {
      dsyslog ("[softdevice-audio]: EINTR");
      AUDIODEB("EINTR \n");
      return;
    } else if (err < 0) {
      dsyslog("[softdevice-audio]: write error: %s FATAL exiting",
              snd_strerror(err));
      exit(EXIT_FAILURE);
    }

    if (err > 0 ) {
      if (!currContext.channels)
        Data += err * 2 * 2;
      else
        Data += err * 2 * currContext.channels;
      size -=err;
    };
  }
  AUDIODEB("Write end\n");
}

/* ----------------------------------------------------------------------------
 */
bool
cAlsaAudioOut::SetAC3PassThroughMode(bool on)
{
  if (on)
  {
    oldContext = currContext;

    if (handle)
      snd_pcm_close(handle);
    handle = NULL;

    // open S/P-DIF
    if (ac3pt.SpdifInitAC3(&handle, ac3Device, ac3SpdifPro) == 0)
    {
      currContext.channels = 0;
      return true;
    }
    currContext = oldContext;
    esyslog("[softdevice-audio] AC3 open error:");
  }
  return false;
}

/* ----------------------------------------------------------------------------
 */
void cAlsaAudioOut::WriteAC3(uchar *Data, int Length)
{
  handleMutex.Lock();
  if (!ac3PassThrough)
  {
    ac3PassThrough = true;
    SetAC3PassThroughMode(ac3PassThrough);
  }
  handleMutex.Unlock();

  ac3pt.SpdifBurstAC3 (&handle, Data, Length);
}

/* ----------------------------------------------------------------------------
 */
void cAlsaAudioOut::Pause(void) {
    //    snd_pcm_pause(handle,1);
    dsyslog("[softdevice-audio]: Should pause now");
    paused=true;
}

/* ----------------------------------------------------------------------------
 */
void cAlsaAudioOut::Play(void) {
//    snd_pcm_pause(handle,0);
    paused=false;
}

/* ----------------------------------------------------------------------------
 */
int cAlsaAudioOut::GetDelay(void) {
    int               res = 0;
    snd_pcm_sframes_t r;

  handleMutex.Lock();
  if (!handle) {
          handleMutex.Unlock();
          return 0;
  };

  if ( snd_pcm_state(handle) != SND_PCM_STATE_RUNNING ) {
          handleMutex.Unlock();
          return 0;
  };

  if (!snd_pcm_delay(handle, &r) &&
      currContext.samplerate) {
    // successfully got delay
    res = (long) r * 10000 / currContext.samplerate;
    AUDIODEB("GetDelay %d\n",res);
  }
  handleMutex.Unlock();
  return res;
}

/* ----------------------------------------------------------------------------
 * I/O error handler
 */
void cAlsaAudioOut::Xrun(void)
{
    snd_pcm_status_t  *status;
    int               res;

  snd_pcm_status_alloca(&status);
  //printf("alsa-audio: Xrun\n");
  if ((res = snd_pcm_status(handle, status))<0) {
    esyslog("[softdevice-audio]: Xrun status error: %s FATAL exiting",
    snd_strerror(res));
    exit(EXIT_FAILURE);
  }
  if (snd_pcm_status_get_state(status) == SND_PCM_STATE_XRUN) {
      struct timeval now, diff, tstamp;
    gettimeofday(&now, 0);
    snd_pcm_status_get_trigger_tstamp(status, &tstamp);
    timersub(&now, &tstamp, &diff);
    softlog->Log(SOFT_LOG_DEBUG, 0,
                 "[softdevice-audio]: Xrun (at least %.3f ms long)\n",
                 diff.tv_sec * 1000 + diff.tv_usec / 1000.0);
    if ((res = snd_pcm_prepare(handle))<0) {
      esyslog("[softdevice-audio]: Xrun prepare error: %s FATAL exiting",
      snd_strerror(res));
      exit(EXIT_FAILURE);
    }
    return;		/* ok, data should be accepted again */
  }

  //  error("read/write error, state = %s", snd_pcm_state_name(snd_pcm_status_get_state(status)));
  esyslog("[softdevice-audio]: read/write error FATAL exiting");
  exit(EXIT_FAILURE);
}

/* ----------------------------------------------------------------------------
 */
int cAlsaAudioOut::SetParams(SampleContext &context) {
      int   err;

    // not needed to set again
    if (currContext.samplerate == context.samplerate &&
        currContext.channels == context.channels ) {
      context=currContext;
      return 0;
    };
    //printf("alsa-audio: SetParams\n");
    currContext=context;

    handleMutex.Lock();

    if (handle)
      snd_pcm_close(handle);
    handle = NULL;

    if (context.channels < 10) {
      sprintf (chDevice, "CH%d", currContext.channels);
      device = chDevice;
      if ((err = snd_pcm_open(&handle,
                              device,
                              SND_PCM_STREAM_PLAYBACK, 0)) < 0) {
        esyslog("[softdevice-audio] Playback reopen error: %s, %s",
                chDevice, snd_strerror(err));
        handle = NULL;
      }
    }

    if (!handle) {
      device = setupStore->alsaDevice;
      if ((err = snd_pcm_open(&handle,
                              device,
                              SND_PCM_STREAM_PLAYBACK, 0)) < 0) {
        esyslog("[softdevice-audio] Playback reopen error: "
                "%s, %s FATAL exiting",
                device, snd_strerror(err));

        handle = NULL;
        handleMutex.Unlock();
        return -1;
      }
    }

    dsyslog ("[softdevice-audio] samplerate: %dHz, channels: #%d",
             currContext.samplerate, currContext.channels);
    snd_pcm_hw_params_t *params;
    snd_pcm_sw_params_t *swparams;
    snd_pcm_hw_params_alloca(&params);
    snd_pcm_sw_params_alloca(&swparams);

    err = snd_pcm_hw_params_any(handle, params);
    if (err < 0) {
      esyslog("[softdevice-audio] Broken config for this PCM: no configurations available");
      handleMutex.Unlock();
      exit(EXIT_FAILURE);
    }

    useMmapWrite = 1;
    snd_pcm_access_mask_t *mask = (snd_pcm_access_mask_t *)alloca(snd_pcm_access_mask_sizeof());
    snd_pcm_access_mask_none(mask);
    snd_pcm_access_mask_set(mask, SND_PCM_ACCESS_MMAP_INTERLEAVED);
    snd_pcm_access_mask_set(mask, SND_PCM_ACCESS_MMAP_NONINTERLEAVED);
    snd_pcm_access_mask_set(mask, SND_PCM_ACCESS_MMAP_COMPLEX);
    err = snd_pcm_hw_params_set_access_mask(handle, params, mask);
    if (err < 0) {
      snd_pcm_access_mask_none(mask);
      snd_pcm_access_mask_set(mask, SND_PCM_ACCESS_RW_INTERLEAVED);
      err = snd_pcm_hw_params_set_access_mask(handle, params, mask);
      if (err < 0) {
        esyslog("[softdevice-audio] Access type not available NO AUDIO!");
        handleMutex.Unlock();
        Suspend();
        return -1;
      }
      useMmapWrite = 0;
    }

    err = snd_pcm_hw_params_set_format(handle, params, PCM_FMT);
    if (err < 0) {
      esyslog("[softdevice-audio] Sample format non available NO AUDIO!");
      handleMutex.Unlock();
      Suspend();
      return -1;
    }

    err = snd_pcm_hw_params_set_channels(handle, params,currContext.channels);
    if (err < 0) {
      esyslog("[softdevice-audio] Channels count non available NO AUDIO");
      handleMutex.Unlock();
      Suspend();
      return -1;
    }

    err = snd_pcm_hw_params_set_rate_near(handle, params, &currContext.samplerate, 0);
    assert(err >= 0);
    if (currContext.samplerate != context.samplerate ) {
      esyslog("[softdevice-audio] Rate %d Hz is not possible (and using instead %d Hz is not implemented) NO AUDIO!",context.samplerate,currContext.samplerate);
      //put back requested samplerate, so that we don't try again
      currContext.samplerate = context.samplerate;
      handleMutex.Unlock();
      Suspend();
      return -1;
    }

    // set period size
    snd_pcm_uframes_t bufferSize;
    snd_pcm_uframes_t periodSize;
    //periodSize = 8;
    periodSize = 4608 / 4;
    err = snd_pcm_hw_params_set_period_size_near(handle, params, &periodSize, 0);
    if ( err < 0)  {
      esyslog("[softdevice-audio] Failed to set period size! NO AUDIO!");
      handleMutex.Unlock();
      Suspend();
      return -1;
    }

    snd_pcm_uframes_t buffersize = 2 * 4608;

    err = snd_pcm_hw_params_set_buffer_size_near(handle, params, &buffersize);
    if ( err < 0 ) {
      esyslog("[softdevice-audio] Failed to set buffer size! NO AUDIO");
      handleMutex.Unlock();
      Suspend();
      return -1;
    }

    err = snd_pcm_hw_params(handle, params);
    if (err < 0) {
      esyslog("[softdevice-audio] Unable to install hw params: FATAL exiting");
      handleMutex.Unlock();
      exit(EXIT_FAILURE);
    }

    snd_pcm_hw_params_get_period_size(params, &periodSize, 0);
    snd_pcm_hw_params_get_buffer_size(params, &bufferSize);
    if (periodSize == bufferSize) {
      esyslog("[softdevice-audio] Can't use period equal to buffer size (%lu == %lu) FATAL exiting",
              periodSize, bufferSize);
      handleMutex.Unlock();
      exit(EXIT_FAILURE);
    }
    dsyslog("[softdevice-audio] Device '%s', Samplerate %d Channels %d",
            device,currContext.samplerate, currContext.channels);
    dsyslog("[softdevice-audio] Period size %lu Buffer size %lu",
            periodSize, bufferSize);

    currContext.buffer_size=bufferSize;
    currContext.period_size=periodSize;

    snd_pcm_sw_params_current(handle, swparams);
    dsyslog("[softdevice-audio] Hardware initialized");
    handleMutex.Unlock();

    context=currContext;
    return 0;
}

/* ----------------------------------------------------------------------------
 */
void cAlsaAudioOut::SetVolume (int vol)
{
  if (!setupStore->useMixer) {
    scale_Factor = CalcScaleFactor(vol);
    //printf("vol %d scale_Factor 0x%04x\n",vol,scale_Factor);
    //scale_Factor = vol;
  } else {
      int                   err;
      long                  mixerMin, mixerMax,
                            setVol;
      double                mixerRange,
                            volPercent;
      const char            *mName = "PCM",
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
    /* ------------------------------------------------------------------------
     * some cards don't have any master volume. so check return value
     */
    if (mElem) {
      snd_mixer_selem_get_playback_volume_range(mElem,&mixerMin,&mixerMax);
      mixerRange = mixerMax - mixerMin;
      volPercent = (double) vol / 255.0;
      setVol = (int) (((double)mixerMin+(mixerRange*volPercent))+0.5);
      snd_mixer_selem_set_playback_volume(mElem,
                                          SND_MIXER_SCHN_FRONT_LEFT,
                                          setVol);
      snd_mixer_selem_set_playback_volume(mElem,
                                          SND_MIXER_SCHN_FRONT_RIGHT,
                                          setVol);
      if (snd_mixer_selem_has_playback_switch(mElem)) {
        snd_mixer_selem_set_playback_switch_all(mElem, (vol) ? 1 : 0);
      }
    }

    snd_mixer_close(mHandle);
  }
}

