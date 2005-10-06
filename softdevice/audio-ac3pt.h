/*
 * audio-ac3pt.h: A plugin for the Video Disk Recorder
 *
 * See the README file for copyright information and how to reach the author.
 *
 * $Id: audio-ac3pt.h,v 1.1 2005/10/06 21:16:47 lucke Exp $
 */
#ifndef AUDIO_AC3PT_H
#define AUDIO_AC3PT_H

enum _sync { LOW = -1, OK = 0, HIGH = 1};
// AC3 frames are translated into 4*(6*256) bytes (zero padded)
// embedded into PCM with 2 channels both with 2 bytes in on word,
// 256 frames in one block, 6 blocks in one sample
#define BURST_SIZE    6144
#define SAMPLE_CNT    4
#define SAMPLE_BUF    (BURST_SIZE/SAMPLE_CNT)
// Alsa Buffer and fragmentation
#define PERIODSIZE    SAMPLE_BUF

/* ---------------------------------------------------------------------------
 */
class cAlsaAC3pt {
private:
  snd_pcm_uframes_t burst_size,
                    buffer_size,
                    periods;
  snd_pcm_status_t  *ac3Status;
  snd_pcm_sframes_t ac3Delay;
  bool              ac3First;

  int           CheckDelayAC3(snd_pcm_t *handle);
  void          ShiftAC3(void),
                WriteFullBurstAC3(snd_pcm_t *handle),
                WriteBurstAC3(snd_pcm_t *handle,
                              unsigned char *data,
                              snd_pcm_sframes_t count),
                WritePauseBurstAC3(snd_pcm_t *handle, int leadin),
                XunderrunAC3(snd_pcm_t *handle),
                XsuspendAC3(snd_pcm_t *handle);

public:
  cAlsaAC3pt();
  ~cAlsaAC3pt();

  unsigned int  SpdifInitAC3(snd_pcm_t **handle,
                             char *device,
                             bool spdifPro);
  void          SpdifBurstAC3(snd_pcm_t **handle,
                              unsigned char *Data,
                              int Length);

};

#endif
