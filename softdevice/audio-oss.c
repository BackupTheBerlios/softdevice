/*
 * audio-oss.c: A plugin for the Video Disk Recorder
 *
 * See the README file for copyright information and how to reach the author.
 *
 * Support for the Open Sound System contributed by Lubos Novak
 * 
 * $Id: audio-oss.c,v 1.1 2006/12/03 20:38:18 wachm Exp $
 */
#include "audio-oss.h"

#include <sys/fcntl.h>
#include <sys/soundcard.h>
#include <sys/ioctl.h>
#include <sys/stat.h>

#define DSP_DEVICE "/dev/dsp"
#define MIXER_DEVICE "/dev/mixer"

cOSSAudioOut::cOSSAudioOut(cSetupStore *setupStore)
{
    struct stat file_info;

    if (-1 == stat(DSP_DEVICE, &file_info))
    {
	esyslog("[softdevice-audio-oss] Stat info FAIL");
	exit(-1);
    }

    if (!S_ISCHR(file_info.st_mode))
    {
	esyslog("[softdevice-audio-oss] %s is not char device", DSP_DEVICE);
	exit(-1);
    }

    if (-1 == (fdDSP = open(DSP_DEVICE, O_WRONLY)))
    {
	esyslog("[softdevice-audio-oss] Device open for write FAIL\n");
	exit(-1);
    }
    
    paused=false;
    isyslog("[softdevice-audio-oss] Device open!");
#ifdef NO_MIXER    
    scale_Factor = 0x7FFF;
#endif
}

cOSSAudioOut::~cOSSAudioOut()
{
    ioctl(fdDSP, SNDCTL_DSP_RESET, 0);
    close(fdDSP);
}


/* ---------------------------------------------------------------------------
 */
void cOSSAudioOut::Write(uchar *Data, int Length)
{
    ssize_t done = 0;
    ssize_t wsize;

#ifdef NO_MIXER
    Scale((int16_t*)Data, Length / 2, scale_Factor);
#endif
    
    while (Length > done)
    {
	while (paused)
	    usleep(1000);
	    
	if ((-1 == (wsize = write(fdDSP, Data, Length - done))) && (errno != EINTR))
	{
	    esyslog("[softdevice-audio-oss] write FAIL");
	    exit(-1);
	}

	done += wsize;
    }

    return;
}

/* ----------------------------------------------------------------------------
 */
void cOSSAudioOut::WriteAC3(uchar *Data, int Length)
{
}

/* ---------------------------------------------------------------------------
 */
void cOSSAudioOut::Pause(void)
{
  dsyslog("[softdevice-audio-oss]: Should pause now");
  paused=true;
}

/* ---------------------------------------------------------------------------
 */
void cOSSAudioOut::Play(void)
{
  paused=false;
}

/* ---------------------------------------------------------------------------
 */
int cOSSAudioOut::GetDelay(void)
{
    return 0;
/*    audio_buf_info buf_info;
    int res = 0;
    
    handleMutex.Lock();
    
    if (-1 == ioctl(fdDSP, SNDCTL_DSP_GETOSPACE, &buf_info))
    {
	esyslog("OSS buffer info FAIL");
	handleMutex.Unlock();
	exit(1);
    }

    int used_buffer = buf_info.fragstotal * buf_info.fragsize - buf_info.bytes;
    res = (long)((double)used_buffer / (double)(currContext.samplerate * currContext.channels) * 1000);

    isyslog("Pouzito %d B, postaci na %d/10 ms\n", used_buffer, res);
    
    handleMutex.Unlock();
    return res;*/
}

/* ---------------------------------------------------------------------------
 */
int cOSSAudioOut::SetParams(SampleContext &context)
{
    // not needed to set again
    if ((currContext.samplerate == context.samplerate) && 
	(currContext.channels == context.channels))
    {
	context=currContext;
	return 0;
    }

    handleMutex.Lock();

    isyslog("[softdevice-audio-oss]: channels %d, samplerate %d, fmt: %d, period: %d, buffer: %d\n",
	    context.channels, context.samplerate, context.pcm_fmt,
	    context.period_size, context.buffer_size );

    if (-1 == ioctl(fdDSP, SNDCTL_DSP_RESET, 0) )
    {
	esyslog("[softdevice-audio-oss]:Nepovedlo se resetovat /dev/dsp\n");
	handleMutex.Unlock();
	exit(-1);
    }

    int playFormat = AFMT_S16_LE;
    int playFormatIoctl = playFormat;
    if ((-1 == ioctl(fdDSP, SNDCTL_DSP_SETFMT, &playFormatIoctl)) || (playFormat != playFormatIoctl))
    {
	esyslog("[softdevice-audio-oss]: Sound format orig: %d, ioct: %d\n", playFormat, playFormatIoctl);
	esyslog("[softdevice-audio-oss]: Set sound format FAIL\n");
	handleMutex.Unlock();
	exit(-1);
    }

    uint32_t channels = context.channels;
    if ((-1 == ioctl(fdDSP, SNDCTL_DSP_CHANNELS, &channels)) || (context.channels != channels))
    {
	esyslog("[softdevice-audio-oss]: Set channels FAIL");
	handleMutex.Unlock();
	exit(-1);
    }

    uint32_t sampleFrequency = context.samplerate;
    if ((-1 == ioctl(fdDSP, SNDCTL_DSP_SPEED, &sampleFrequency)) || (context.samplerate != sampleFrequency))
    {
	esyslog("[softdevice-audio-oss]: Set frequency FAIL");
	handleMutex.Unlock();
	exit(-1);
    }

    currContext=context;

    handleMutex.Unlock();

    return 0;
}

/* ---------------------------------------------------------------------------
 */
void cOSSAudioOut::SetVolume (int vol)
{
#ifdef NO_MIXER
    scale_Factor = CalcScaleFactor(vol);
#else
    struct stat file_info;

    if (-1 == stat(MIXER_DEVICE, &file_info))
    {
	esyslog("[softdevice-audio-oss] Stat info FAIL(%s)", MIXER_DEVICE);
	return;
    }

    if (!S_ISCHR(file_info.st_mode))
    {
	esyslog("[softdevice-audio-oss] %s is not char device", MIXER_DEVICE);
	return;
    }


    int fdMixer, devs;
    if (-1 == ( fdMixer = open (MIXER_DEVICE, O_RDONLY)))
    {
	esyslog("[softdevice-audio-oss]: Mixer %s open FAIL", MIXER_DEVICE);
	return;
    }
    ioctl(fdMixer, SOUND_MIXER_READ_DEVMASK, &devs);

    int cmd;
    if (devs & SOUND_MASK_PCM)
	cmd = SOUND_MIXER_WRITE_PCM;
    else if(devs & SOUND_MASK_VOLUME)
	cmd = SOUND_MIXER_WRITE_VOLUME;
    else
    {
	esyslog("[softdevice-audio-oss]: Volume set FAIL\n");
	close (fdMixer);
	return;
    }

    int ChVolume = (int)(((float)vol / (float)MAXVOLUME) * 100);
    ChVolume = (ChVolume < 0 ? 0 : ChVolume);
    ChVolume = (ChVolume > 100 ? 100 : ChVolume);
    int ioctlVolume = (ChVolume << 8) | ChVolume;
    if(-1 == ioctl(fdMixer, cmd, &ioctlVolume))
	esyslog("[softdevice-audio-oss]: Volume set FAIL\n");

    close (fdMixer);
#endif
    return;
}
