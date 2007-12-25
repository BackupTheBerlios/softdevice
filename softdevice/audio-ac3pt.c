/*
 * audio-ac3pt.c: alsa AC3 pass through
 *
 * See the README file for copyright information and how to reach the author.
 *
 * $Id: audio-ac3pt.c,v 1.3 2007/12/25 13:21:46 lucke Exp $
 */

#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <math.h>

#include <sys/time.h>

#define ALSA_PCM_NEW_HW_PARAMS_API
#define ALSA_PCM_NEW_SW_PARAMS_API
#include <alsa/asoundlib.h>
#include "audio-ac3pt.h"

typedef enum {SPDIF_CON, SPDIF_PRO} spdif_t;


#ifndef WORDS_BIGENDIAN
# define char2short(a,b)    ((((b) << 8) & 0xffff) ^ ((a) & 0xffff))
# define shorts(a)          (a)
#else
# define char2short(a,b)    ((((a) << 8) & 0xffff) ^ ((b) & 0xffff))
# define shorts(a)          char2short(((a) & 0xff),(((a) >> 8) & 0xff));
#endif

#define EINTR_RETRY(exp)                    \
    (__extension__ ({                       \
    int __result;                           \
    do __result = (int) (exp);              \
    while (__result < 0 && errno == EINTR); \
    __result; }))


unsigned int              rate = 48000;
unsigned int              iec958_aes3_con_fs_rate = IEC958_AES3_CON_FS_48000;
unsigned int              iec958_aes0_pro_fs_rate = IEC958_AES0_PRO_FS_48000;
static int                smin = 32;
unsigned int              frames  = 10;
static const char         head[4] = { 0x72, 0xf8, 0x1f, 0x4e };
static u_char             buf[BURST_SIZE];
static u_char             pbuf[BURST_SIZE];
static u_char             *sbuffer = &buf[10];
static unsigned int       sbuffer_size = 0;

#define AC3_BLOCK_SIZE 2048
static unsigned char      inbuf[2*AC3_BLOCK_SIZE];
typedef struct _source_t {
  unsigned char  *out, *end, *tail;
  size_t  framesize;
} source_t;

static source_t ac3 = {
  inbuf,                  // out
  inbuf + sizeof(inbuf),  // end
  inbuf,                  // tail
  AC3_BLOCK_SIZE,         // framesize
};

struct frmsize_s
{
  unsigned short bit_rate;
  unsigned short frm_size[3];
};

static const struct frmsize_s frmsizecod_tbl[64] =
{
  { 32  ,{64   ,69   ,96   } },
  { 32  ,{64   ,70   ,96   } },
  { 40  ,{80   ,87   ,120  } },
  { 40  ,{80   ,88   ,120  } },
  { 48  ,{96   ,104  ,144  } },
  { 48  ,{96   ,105  ,144  } },
  { 56  ,{112  ,121  ,168  } },
  { 56  ,{112  ,122  ,168  } },
  { 64  ,{128  ,139  ,192  } },
  { 64  ,{128  ,140  ,192  } },
  { 80  ,{160  ,174  ,240  } },
  { 80  ,{160  ,175  ,240  } },
  { 96  ,{192  ,208  ,288  } },
  { 96  ,{192  ,209  ,288  } },
  { 112 ,{224  ,243  ,336  } },
  { 112 ,{224  ,244  ,336  } },
  { 128 ,{256  ,278  ,384  } },
  { 128 ,{256  ,279  ,384  } },
  { 160 ,{320  ,348  ,480  } },
  { 160 ,{320  ,349  ,480  } },
  { 192 ,{384  ,417  ,576  } },
  { 192 ,{384  ,418  ,576  } },
  { 224 ,{448  ,487  ,672  } },
  { 224 ,{448  ,488  ,672  } },
  { 256 ,{512  ,557  ,768  } },
  { 256 ,{512  ,558  ,768  } },
  { 320 ,{640  ,696  ,960  } },
  { 320 ,{640  ,697  ,960  } },
  { 384 ,{768  ,835  ,1152 } },
  { 384 ,{768  ,836  ,1152 } },
  { 448 ,{896  ,975  ,1344 } },
  { 448 ,{896  ,976  ,1344 } },
  { 512 ,{1024 ,1114 ,1536 } },
  { 512 ,{1024 ,1115 ,1536 } },
  { 576 ,{1152 ,1253 ,1728 } },
  { 576 ,{1152 ,1254 ,1728 } },
  { 640 ,{1280 ,1393 ,1920 } },
  { 640 ,{1280 ,1394 ,1920 } }
};

static const unsigned int freqcod_tbl[4] =
{
  48000, 44100, 32000, 0
};

typedef struct syncinfo_s
{
  unsigned int    magic;
  unsigned short  syncword;         /* Sync word == 0x0B77 */
  /* uint_16   crc1; */             /* crc for the first 5/8 of the sync block */
  unsigned short  fscod;            /* Stream Sampling Rate (kHz)
                                        0 = 48,
                                        1 = 44.1,
                                        2 = 32,
                                        3 = reserved */
  unsigned short  frmsizecod;       /* Frame size code */
  unsigned short  frame_size;       /* Information not in the AC-3 bitstream, but derived */
                                    /* Frame size in 16 bit words */
  unsigned short  bit_rate;         /* Bit rate in kilobits */
  unsigned int    sampling_rate;    /* sampling rate in hertz */

} syncinfo_t;

static syncinfo_t syncinfo;


/* ----------------------------------------------------------------------------
 * Parse a syncinfo structure, minus the sync word
 */
void
parse_syncinfo(syncinfo_t *syncinfo, unsigned char *data)
{
  //
  // We need to read in the entire syncinfo struct (0x0b77 + 24 bits)
  // in order to determine how big the frame is
  //

  // Get the frequency and sampling code
  syncinfo->fscod  = (data[2] & 0xc0) >> 6;

  // Calculate the sampling rate
  syncinfo->sampling_rate = freqcod_tbl[syncinfo->fscod];
  if (syncinfo->fscod > 2) {
    syncinfo->frame_size = 0;
    syncinfo->bit_rate = 0;
    return;
  }

  // Get the frame size code
  syncinfo->frmsizecod = data[2] & 0x3f;

  // Calculate the frame size and bitrate
  syncinfo->frame_size =
        frmsizecod_tbl[syncinfo->frmsizecod].frm_size[syncinfo->fscod];
  syncinfo->bit_rate = frmsizecod_tbl[syncinfo->frmsizecod].bit_rate;
}

/* ----------------------------------------------------------------------------
 */
static bool
buffer_syncframe(syncinfo_t *syncinfo, source_t *ac3)
{
    unsigned short  syncword = syncinfo->syncword;
    unsigned int    payload_size;
    bool            play = false;

resync:
  //
  // Find an ac3 sync frame.
  //
  while(syncword != 0x0b77) {
    if (ac3->out >= ac3->tail)
      goto done;
    syncword = (syncword << 8) | *ac3->out++;
  }

  /* --------------------------------------------------------------------------
   * Need the next 3 bytes to decide how big the frame is
   */
  while(sbuffer_size < 3) {
    if (ac3->out >= ac3->tail)
      goto done;
    sbuffer[sbuffer_size++] = *ac3->out++;
  }

  /* --------------------------------------------------------------------------
   * Parse and check syncinfo
   */
  parse_syncinfo(syncinfo, sbuffer);
  if (!syncinfo->frame_size) {
    syncword = 0xffff;
    sbuffer_size = 0;
    fprintf(stderr, "ac3play: ** Invalid frame found - try to syncing **\n");
    if (ac3->out >= ac3->tail)
      goto done;
    else
      goto resync;
  }
  ac3->framesize = syncinfo->frame_size * 2;
  payload_size = (unsigned int)(ac3->framesize - 2);

  //
  // Maybe: Reinit spdif interface if sample rate is changed
  //
  if (syncinfo->sampling_rate != rate) {
    fprintf(stderr,
            "ac3play: Warning: sample rate of"
            " the current AC-3 stream (%d) does not\n",
            syncinfo->sampling_rate);
    fprintf(stderr, "	 fit the configured PCM rate (%d)!\n", rate);
  }

  while(sbuffer_size < payload_size) {
    if (ac3->out >= ac3->tail)
      goto done;
    sbuffer[sbuffer_size++] = *ac3->out++;
  }

  //
  // Check the crc over the entire frame
  //
#if 0
  if(!crc_check(sbuffer, payload_size))
  {
    syncword = 0xffff;
    sbuffer_size = 0;
    fprintf(stderr, "ac3play: ** CRC failed - try to syncing **\n");
    if (ac3->out >= ac3->tail)
      goto done;
    else
      goto resync;
  }
#endif
  //
  // If we reach this point, we found a valid AC3 frame to warp into PCM (IEC60958)
  //
  play = true;

  //
  // Reset the syncword for next time
  //
  syncword = 0xffff;
  sbuffer_size = 0;
done:
  syncinfo->syncword = syncword;
  return play;
}

/* ----------------------------------------------------------------------------
 */
cAlsaAC3pt::cAlsaAC3pt()
{
  ac3Status = NULL;
  buffer_size = 0;
}

/* ----------------------------------------------------------------------------
 */
cAlsaAC3pt::~cAlsaAC3pt()
{
  if (ac3Status)
    snd_pcm_status_free(ac3Status);
}

/* ----------------------------------------------------------------------------
 */
void
cAlsaAC3pt::XunderrunAC3(snd_pcm_t *handle)
{
    snd_pcm_sframes_t res;

  if ((res = snd_pcm_status(handle, ac3Status))<0) {
    fprintf(stderr, "ac3play: ac3Status error: %s\n", snd_strerror(res));
    return;
  }
  if (snd_pcm_status_get_state(ac3Status) == SND_PCM_STATE_XRUN) {
      struct timeval now, diff, tstamp;
    gettimeofday(&now, 0);
    snd_pcm_status_get_trigger_tstamp(ac3Status, &tstamp);
    timersub(&now, &tstamp, &diff);
    fprintf(stderr, "ac3play: xunderrun!!! (at least %.3f ms long)\n",
            diff.tv_sec * 1000 + diff.tv_usec / 1000.0);
    if ((res = snd_pcm_prepare(handle))<0) {
      fprintf(stderr, "ac3play: xunderrun: prepare error: %s\n",
              snd_strerror(res));
      return;
    }
    return;/* ok, data should be accepted again */
  }
}

/* ----------------------------------------------------------------------------
 */
void
cAlsaAC3pt::XsuspendAC3(snd_pcm_t *handle)
{
    snd_pcm_sframes_t res;

  if ((res = snd_pcm_status(handle, ac3Status))<0) {
    fprintf(stderr, "ac3play: ac3Status error: %s\n", snd_strerror(res));
    return;
  }
  if (snd_pcm_status_get_state(ac3Status) == SND_PCM_STATE_SUSPENDED) {
    fprintf(stderr, "ac3play: xsuspend!!! trying to resume\n");
    while ((res = snd_pcm_resume(handle)) == -EAGAIN)
      usleep(100000);
    if ((res = snd_pcm_prepare(handle))<0) {
      fprintf(stderr, "ac3play: xsuspend: prepare error: %s\n",
                      snd_strerror(res));
      return;
    }
    return;/* ok, data should be accepted again */
  }
}

/* ----------------------------------------------------------------------------
 */
int
cAlsaAC3pt::CheckDelayAC3(snd_pcm_t *handle)
{
    int grade = LOW;

  ac3Delay = 0;
  if (ac3First)    // Not running is LOW
    return grade;

  snd_pcm_delay(handle, &ac3Delay);
  if (ac3Delay < 4*PERIODSIZE)
    grade = LOW;
  else if (ac3Delay > (snd_pcm_sframes_t) (buffer_size - PERIODSIZE))
    grade = HIGH;
  else
    grade = OK;

  return grade;
}

/* ----------------------------------------------------------------------------
 */
void
cAlsaAC3pt::ShiftAC3(void)
{
  // Ringbuffer arithmetics
  if (ac3.out >= ac3.tail) {
    // empty, reset buffer
    ac3.out   = inbuf;
    ac3.tail  = inbuf;
    return;
  }
  if (ac3.out > &inbuf[0]) {
    // buffer not empty, move contens
    off_t len = ac3.tail - ac3.out;
    ac3.out   = (unsigned char *) memmove(&inbuf[0], ac3.out, len);
    ac3.tail  = ac3.out + len;
  }
}

/* ----------------------------------------------------------------------------
 */
void
cAlsaAC3pt::WriteBurstAC3(snd_pcm_t *handle, unsigned char *data, snd_pcm_sframes_t count)
{
    int grade;

  if ((grade = CheckDelayAC3(handle)) == HIGH)
    // High state, wait
    EINTR_RETRY(snd_pcm_wait(handle, -1));

  while(count > 0) {
    snd_pcm_sframes_t r = 0;
    if ((r = snd_pcm_writei (handle, (void *)data, count)) < 0) {
      switch(r) {
        case -EAGAIN:
        case -EBUSY:
          EINTR_RETRY(snd_pcm_wait(handle, -1));
        case -EINTR:
          continue;
        case -EPIPE:
          XunderrunAC3(handle);
          continue;
        case -ESTRPIPE:
          XsuspendAC3(handle);
          continue;
        default:
          fprintf(stderr, "ac3play: snd_pcm_writei returned error: %s\n",
                  snd_strerror(r));
          exit(1);
      }
    }

    if (r < count)
      EINTR_RETRY(snd_pcm_wait(handle, -1));

    count -= r;
    data += r * SAMPLE_CNT;
  }
}

/* ----------------------------------------------------------------------------
 */
void
cAlsaAC3pt::WritePauseBurstAC3(snd_pcm_t *handle, int leadin)
{
    unsigned short int *sbuf = (unsigned short int *) pbuf;

  // Don't set or overwrite spdif header,
  // this is done in spdif_init only once
  switch (leadin) {
    default:
    case 0:
      sbuf[2] = char2short(0x00, 7<<5);// null frame, stream = 7
      sbuf[3] = shorts(0x0000);// No data therein
      break;
    case 1:
      sbuf[2] = char2short(0x03, 0x00);// Audio ES Channel empty, wait for DD Decoder or pause
      sbuf[3] = char2short(0x20, 0x00);// Trailing frame size is 0x0020 aka 32 bits payload
      break;
    case -1:
      sbuf[2] = char2short(0x03, 0x01);// User stop, skip or error
      sbuf[3] = char2short(0x00, 0x08);// Trailing frame size is zero
      break;
  }

  // SAMPLE_BUF is BURST_SIZE/4
  WriteBurstAC3(handle, pbuf, SAMPLE_BUF);
}

/* ----------------------------------------------------------------------------
 */
void
cAlsaAC3pt::WriteFullBurstAC3(snd_pcm_t *handle)
{
    unsigned short int  *sbuf = (unsigned short int *)&buf[0];
    size_t              len = (syncinfo.frame_size * 2) - 2;

  if (ac3First) {
    // Send some trailing zeros to avoid misdetecting the
    // first frame as PCM (works for DTT2500 and VXAE07 at least)
    ac3First = false;
    WritePauseBurstAC3(handle, 0);
    WritePauseBurstAC3(handle, 0);
    WritePauseBurstAC3(handle, 1);
  }

  // Don't set or overwrite spdif header,
  // this is done in spdif_init only once

  sbuf[2] = char2short(0x01, buf[13]&7);		// AC3 data, bsmod: stream = 0
  sbuf[3] = shorts(syncinfo.frame_size * 16);	// Trailing AC3 frame size
  sbuf[4] = char2short(0x77, 0x0b);		// AC3 syncwork

#ifndef WORDS_BIGENDIAN
  // extract_ac3 seems to write swabbed data
  swab(&buf[10], &buf[10], len);
#endif
  // With padding zeros and do not overwrite IEC958 head
  memset(&buf[10] + len, 0, BURST_SIZE - 10 - len);

  // Write out the frames to S/P-DIF, SAMPLE_BUF is BURST_SIZE/4
  WriteBurstAC3(handle, (u_char *)buf, SAMPLE_BUF);
}

/* ----------------------------------------------------------------------------
 */
void
cAlsaAC3pt::SpdifBurstAC3 (snd_pcm_t **handle, unsigned char *Data, int length)
{
    int freeCount, nBytes, amount;

  while (length > 0)
  {
    amount = CheckDelayAC3(*handle);
    freeCount = ac3.end - ac3.tail;
    nBytes = (freeCount < length) ? freeCount : length;
    /* read */
    memcpy (ac3.tail, Data, nBytes);
    ac3.tail += nBytes;
    length -= nBytes;
    // so push out complete ac3 frames
    while (buffer_syncframe (&syncinfo, &ac3))
      WriteFullBurstAC3(*handle);
    ShiftAC3();
  }
}

/* ----------------------------------------------------------------------------
 */
unsigned int
cAlsaAC3pt::SpdifInitAC3(snd_pcm_t **handle, char *device, bool spdifPro)
{
    static snd_aes_iec958_t   spdif;
    unsigned int              rate = 48000;
    snd_pcm_info_t            *info;
    // Note that most alsa sound card drivers uses little endianess
    snd_pcm_format_t          format = SND_PCM_FORMAT_S16_LE;
    snd_pcm_access_t          access = SND_PCM_ACCESS_RW_INTERLEAVED;
    unsigned int              channels = 2;
    int                       err;//, c;

  if ((err = snd_pcm_open(handle, device, SND_PCM_STREAM_PLAYBACK, 0)) < 0)
  {
    fprintf(stderr, "ac3play: sound open: %s\n", snd_strerror(err));
    return 1;
  }

  if (ac3Status)
    snd_pcm_status_free(ac3Status);

  snd_pcm_info_alloca(&info);

  if ((err = snd_pcm_info(*handle, info)) < 0) {
    fprintf(stderr, "ac3play: sound info: %s\n", snd_strerror(err));
    snd_pcm_close(*handle);
    return 1;
  }
  {
      snd_ctl_elem_value_t  *ctl;
      snd_ctl_t             *ctl_handle;
      char                  ctl_name[12];
      int                   ctl_card;

    spdif.status[0] = IEC958_AES0_NONAUDIO;
    if (spdifPro)
    {
      spdif.status[0] |= (IEC958_AES0_PROFESSIONAL |
                          IEC958_AES0_PRO_EMPHASIS_NONE |
                          iec958_aes0_pro_fs_rate);
      spdif.status[1]  = (IEC958_AES1_PRO_MODE_NOTID |
                          IEC958_AES1_PRO_USERBITS_NOTID);
      spdif.status[2]  = (IEC958_AES2_PRO_WORDLEN_NOTID);
      spdif.status[3]  =  0;
    }
    else
    {
      spdif.status[0] |= (IEC958_AES0_CON_EMPHASIS_NONE);
      spdif.status[1]  = (IEC958_AES1_CON_ORIGINAL |
                          IEC958_AES1_CON_PCM_CODER);
      spdif.status[2]  =  0;
      spdif.status[3]  = (iec958_aes3_con_fs_rate);
    }

    snd_ctl_elem_value_alloca(&ctl);
    snd_ctl_elem_value_set_interface(ctl, SND_CTL_ELEM_IFACE_PCM);
    snd_ctl_elem_value_set_device(ctl, snd_pcm_info_get_device(info));
    snd_ctl_elem_value_set_subdevice(ctl, snd_pcm_info_get_subdevice(info));
    snd_ctl_elem_value_set_name(ctl, SND_CTL_NAME_IEC958("", PLAYBACK,PCM_STREAM));
    snd_ctl_elem_value_set_iec958(ctl, &spdif);
    ctl_card = snd_pcm_info_get_card(info);
    if (ctl_card < 0) {
      fprintf(stderr, "ac3play: Unable to setup the IEC958 (S/PDIF) interface - PCM has no assigned card\n");
      goto __diga_end;
    }
    sprintf(ctl_name, "hw:%d", ctl_card);
    if ((err = snd_ctl_open(&ctl_handle, ctl_name, 0)) < 0) {
      fprintf(stderr, "ac3play: Unable to open the control interface '%s': %s\n", ctl_name, snd_strerror(err));
      goto __diga_end;
    }
    if ((err = snd_ctl_elem_write(ctl_handle, ctl)) < 0) {
      fprintf(stderr, "ac3play: Unable to update the IEC958 control: %s\n", snd_strerror(err));
      goto __diga_end;
    }
    snd_ctl_close(ctl_handle);
    __diga_end:
      ;
  }
  {
      snd_pcm_hw_params_t *params;
      snd_pcm_sw_params_t *swparams;
      // set period size
      snd_pcm_uframes_t bufferSize;
      // Number of digital audio frames in hw buffer
      snd_pcm_uframes_t periodSize = 0;

      //snd_pcm_uframes_t tmp;
      snd_pcm_sframes_t err;

    snd_pcm_hw_params_alloca(&params);
    snd_pcm_sw_params_alloca(&swparams);

    if ((err = snd_pcm_hw_params_any(*handle, params)) < 0) {
      fprintf(stderr, "ac3play: Broken configuration for this PCM: no configurations available\n");
      return 2;
    }
    if ((err = snd_pcm_hw_params_set_access(*handle, params, access)) < 0) {
      fprintf(stderr, "ac3play: Access type not available\n");
      return 2;
    }
    if ((err = snd_pcm_hw_params_set_format(*handle, params, format)) < 0) {
      fprintf(stderr, "ac3play: Sample format non available\n");
      return 2;
    }

    if ((err = snd_pcm_hw_params_set_channels(*handle, params, channels)) < 0) {
      fprintf(stderr, "ac3play: Channels count non available\n");
      return 2;
    }

    err = snd_pcm_hw_params_set_rate_near(*handle, params, &rate, 0);
    assert(err >= 0);
    smin = ((SAMPLE_BUF * 1000) / rate);

    // function returns less than 0 on error or greater than 0
    if ((err = snd_pcm_hw_params_set_period_size(*handle, params, PERIODSIZE, 0)) < 0) {
      fprintf(stderr, "ac3play: Period size not available: %s\n", snd_strerror(err));
      periodSize = 0;
      return 2;
    }
    // function returns less than 0 on error or greater than 0
    if ((err = snd_pcm_hw_params_set_periods(*handle, params, frames, 0)) < 0) {
    fprintf(stderr, "ac3play: Period count not available: %s\n", snd_strerror(err));
    return 2;
    }

    // function returns less than 0 on error or greater than 0
    if ((err = snd_pcm_hw_params_get_period_size(params, &periodSize, 0)) < 0) {
      fprintf(stderr, "ac3play: Buffer size not set: %s\n", snd_strerror(err));
      return 2;
    }
    if (PERIODSIZE != periodSize) {
      fprintf(stderr, "ac3play: Period size not set: %s\n", snd_strerror(err));
      return 2;
    }
    //period_size = err;

    // function returns less than 0 on error or greater than 0
    if ((err = snd_pcm_hw_params_get_buffer_size(params, &bufferSize)) < 0) {
      fprintf(stderr, "ac3play: Buffer size not set: %s\n", snd_strerror(err));
      return 2;
    }
    if (periodSize * frames != bufferSize) {
      fprintf(stderr, "ac3play: Buffer size not set: %s\n", snd_strerror(err));
      return 2;
    }
    buffer_size = err;

//    snd_output_stdio_attach(&log, stderr, 0);

    if ((err = snd_pcm_hw_params(*handle, params)) < 0) {
      fprintf(stderr, "ac3play: Cannot set buffer size\n");
//      snd_pcm_hw_params_dump(params, log);
      return 2;
    }

    if ((err = snd_pcm_sw_params_current(*handle, swparams)) < 0) {
      fprintf(stderr, "ac3play: Cannot get soft parameters: %s\n", snd_strerror(err));
      return 2;
    }

// Set start timings
#if 0
    if ((err = snd_pcm_sw_params_set_sleep_min(*handle, swparams, 0)) < 0)
      fprintf(stderr, "ac3play: Minimal sleep time not available: %s\n", snd_strerror(err));
#endif

    if ((err = snd_pcm_sw_params_set_avail_min(*handle, swparams, SAMPLE_BUF)) < 0)
      fprintf(stderr, "ac3play: Minimal period size not available: %s\n", snd_strerror(err));

    if ((err =  snd_pcm_sw_params_set_xfer_align(*handle, swparams, SAMPLE_BUF/6)) < 0)
      fprintf(stderr, "ac3play: Aligned period size not available: %s\n", snd_strerror(err));

    if ((err = snd_pcm_sw_params_set_start_threshold(*handle, swparams, 2*SAMPLE_BUF)) < 0)
      fprintf(stderr, "ac3play: Start threshold not available: %s\n", snd_strerror(err));

    if ((err = snd_pcm_sw_params(*handle, swparams)) < 0) {
      fprintf(stderr, "ac3play: Cannot set soft parameters: %s\n", snd_strerror(err));
//      snd_pcm_sw_params_dump(swparams, log);
      return 2;
    }
#ifdef DEBUG2
    snd_pcm_sw_params_dump(swparams, log);
    snd_pcm_dump(*handle, log);
#endif
  }
  memcpy(buf, head, sizeof(head));	// Initialize IEC958 head of the buffer
  memset(&buf[4], 0, BURST_SIZE - 4);	// and zero out the rest
  memcpy(pbuf, head, sizeof(head));	// Initialize IEC958 head of the pause buffer
  memset(&pbuf[4], 0, BURST_SIZE - 4);	// and zero out the rest

  // Status informations, hold over the full session.
  snd_pcm_status_malloc(&ac3Status);

  // On next play we've to initialize the HW
  ac3First = true;
  return 0;
}
