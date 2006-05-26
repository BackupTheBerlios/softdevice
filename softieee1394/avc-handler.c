/*
 * avc-handler.c: handle optionally present AVC (firewire) devices.
 *
 * See the README file for copyright information and how to reach the author.
 *
 * $Id: avc-handler.c,v 1.4 2006/05/26 19:59:21 lucke Exp $
 */

#include <sys/mman.h>
#include <sys/ioctl.h>
#include <sys/poll.h>

#include <fcntl.h>
#include <vdr/plugin.h>
#include <avformat.h>

#include "avc-handler.h"
#include "../softdevice/softdevice.h"

/* ---------------------------------------------------------------------------
 * MIC helper functions. should be in libavc1394, but for easier development,
 *
 */
/* ---------------------------------------------------------------------------
 */
static int
avc1394_mic_atn(unsigned char *p)
{
    int atn = 0;

  if (*(p + 4) >> 3 == 0x1f) {
    atn = (*(p+1) | *(p+2) << 8 | *(p+3) << 16) >> 1;
  } else if (*(p + 4) >> 3 == 0x1f) {
    atn = (*(p+3) << 1 | *(p+2) << 9 | (*(p+1) & 0x3f) << 17);
  }

  return atn;
}

/* ---------------------------------------------------------------------------
 */
static void
avc1394_atn2str(int atn, char *strP, char *strN)
{
  sprintf (strP,
           "%02d:%02d:%02d.%02d",
           (atn / (12 * 25 * 60 * 60)) % 24,
           (atn / (12 * 25 * 60)) % 60,
           (atn / (12 * 25)) % 60,
           (atn / 12) % 25);
  sprintf (strN,
           "%02d:%02d:%02d.%02d",
           (atn / (10 * 30 * 60 * 60)) % 24,
           (atn / (10 * 30 * 60)) % 60 ,
           (atn / (10 * 30) % 60),
           (atn / 10) % 30);
}

/* ---------------------------------------------------------------------------
 */
static void
avc1394_mic_atn2str(unsigned char *p, char *strP, char *strN)
{
    int atn = 0;

  if (*(p + 4) >> 3 == 0x1f) {
    atn = (*(p+1) | *(p+2) << 8 | *(p+3) << 16) >> 1;
  } else if (*(p + 4) >> 3 == 0x1f) {
    atn = (*(p+3) << 1 | *(p+2) << 9 | (*(p+1) & 0x3f) << 17);
  }
  sprintf (strP,
           "%02d:%02d:%02d.%02d",
           (atn / (12 * 25 * 60 * 60)) % 24,
           (atn / (12 * 25 * 60)) % 60,
           (atn / (12 * 25)) % 60,
           (atn / 12) % 25);
  sprintf (strN,
           "%02d:%02d:%02d.%02d",
           (atn / (10 * 30 * 60 * 60)) % 24,
           (atn / (10 * 30 * 60)) % 60 ,
           (atn / (10 * 30) % 60),
           (atn / 10) % 30);
}

/* ---------------------------------------------------------------------------
 */
static void
avc1394_mic_datetime(t_mic_jump_mark *p_mark, unsigned char *p)
{
  p_mark->year   = (*(p+3) & 0xE0) >> 1 | (*(p+4) & 0xF0) >> 4;
  p_mark->month  = *(p+4) & 0x0f;
  p_mark->day    = *(p+3) & 0x1f;
  p_mark->hour   = *(p+2) & 0x1F;
  p_mark->minute = *(p+1) & 0x3F;
}

/* ---------------------------------------------------------------------------
 */
static void
avc1394_mic_datetime2str(unsigned char *p, char *d, char *t)
{
  sprintf (d,
           "%02d-%02d-%02d",
           (*(p+3) & 0xE0) >> 1 | (*(p+4) & 0xF0) >> 4,
           *(p+4) & 0x0f,
           *(p+3) & 0x1f);
  sprintf (t,
           "%02d:%02d",
           *(p+2) & 0x1F,
           *(p+1) & 0x3F);
}

/* ---------------------------------------------------------------------------
 */
static void
avc1394_dump_micdirectoy(t_mic_directory *mic_dir)
{
    int   i;
    char  bufPAL[16],
          bufNTSC[16];

  if (!mic_dir)
    return;

  fprintf (stderr, "-- tape name:          (%s)\n", mic_dir->tape_name);
  if (mic_dir->eom_atn) {
    avc1394_atn2str(mic_dir->eom_atn, bufPAL, bufNTSC);
    fprintf (stderr, "-- tape end position:  %s\n", bufPAL);
  }
  fprintf (stderr, "-- tape has # markers: %d\n", mic_dir->num_marks);
  for (i = 0; i < mic_dir->num_marks; ++i) {
    fprintf (stderr, "-- (%2d) ", i);
    avc1394_atn2str(mic_dir->jump_mark[i]. atn, bufPAL, bufNTSC);
    fprintf (stderr, "%s ", bufPAL);
    fprintf (stderr,
             "%02d-%02d-%02d %02d:%02d",
             mic_dir->jump_mark[i]. year,
             mic_dir->jump_mark[i]. month,
             mic_dir->jump_mark[i]. day,
             mic_dir->jump_mark[i]. hour,
             mic_dir->jump_mark[i]. minute);
    fprintf (stderr, "\n");
  }
}

/* ---------------------------------------------------------------------------
 */
static int
avc1394_micinfo2micdirectory (t_mic_directory **mic_dir,
                              unsigned char *mic_data, int len)
{
    unsigned char   *p;
    int             l,
                    marker;
    t_mic_directory *tmp_dir = NULL;

  if (!mic_dir || !mic_data || !len)
    return 0;

  if (*mic_dir) {
    free (*mic_dir);
    *mic_dir = NULL;
  }

  if (!(tmp_dir = (t_mic_directory *) calloc (1, sizeof (t_mic_directory))))
    return 0;

  p = mic_data;
  l = len;
  /* check header */

  p += 11;
  l -= 11;
  /* get tape end ATN */
  if (l < len && *p == MIC_TAG_TAPEEOM) {
    if ((len - l) > 5)
      tmp_dir->eom_atn = avc1394_mic_atn(p);

    p += 5; l -= 5;
  }

  p += 5; l -= 5;
  marker = 0;
  while (l < len && *p == MIC_TAG_ATN) {
    if ((len-l) > 10) {
      tmp_dir->jump_mark[marker].atn = avc1394_mic_atn(p);

      p += 5; l -= 5;
      if (*p == MIC_TAG_DATETIME)
        avc1394_mic_datetime(&tmp_dir->jump_mark[marker], p);

      marker++;
      tmp_dir->num_marks = marker;
      p += 5; l -= 5;
      continue;
    }
    p++; l--;
  }
  if (l < len && *p == MIC_TAG_TAPENAME)
    memcpy(tmp_dir->tape_name, p+4, *(p+1));

  *mic_dir = tmp_dir;
  return 1;
}

/* ---------------------------------------------------------------------------
 */
static int
avc1394_vcr_parse_mic_info (unsigned char *mic_data, int len, int prt)
{
    unsigned char *p;
    int           l,
                  marker;
    char          buf1 [16],
                  buf2 [16];

  if (prt)
    fprintf(stderr, "[AVCHandler] MIC info start\n");
  p = mic_data;
  l = len;
  /* check header */

  p += 11;
  l -= 11;
  /* get tape end ATN */
  if (l < len && *p == 0x1F) {
    if ((len - l) > 5) {
      if (prt) {
        avc1394_mic_atn2str (p, buf1, buf2);
        fprintf (stderr, "  tape end : %s\n", buf1);
      }
    }
    p += 5; l -= 5;
  }

  p += 5; l -= 5;

  marker = 1;
  while (l < len && *p == 0x0b) {
    if ((len-l) > 10) {
      if (prt) {
        avc1394_mic_atn2str (p, buf1, buf2);
        fprintf (stderr, "  %2d.      : %s ", marker, buf1);
      }
      p += 5; l -= 5;
      if (*p == 0x42) {
        if (prt) {
          avc1394_mic_datetime2str(p, buf1, buf2);
          fprintf (stderr, "%s %s\n", buf1, buf2);
        }
      } else {
        if (prt) {
          fprintf(stderr,"\n");
        }
      }
      marker++;
      p += 5; l -= 5;
      continue;
    }
    p++; l--;
  }
  if (l < len && *p == 0x18) {
    if (prt) {
      fprintf (stderr,"  tape name: %*.*s\n", *(p+1),*(p+1), p+4);
    }
  }
  if (prt)
    fprintf(stderr, "[AVCHandler] MIC info done!\n");
  return 0;
}
/* ---------------------------------------------------------------------------
 * MIC helper END
 */

/* ---------------------------------------------------------------------------
 */
cAVCHandler::cAVCHandler()
{
  /* --------------------------------------------------------------------------
   * check if there is ieee1394 subsystem on this box. If so start our
   * monitor thread.
   */
  dsyslog("[AVCHandler]: get raw1394 version: %s\n",
          raw1394_get_libversion());

#ifdef RAW1394_V_0_8
  handle = raw1394_get_handle();
#else
  handle = raw1394_new_handle();
#endif
  numDevices = 0;
  for (int i = 0; i < MAX_IEEE1394_DEVICES; ++i) {
    deviceId[i] = 0;
    micSize[i] = 0;
    micData[i] = NULL;
    micDirectories[i] = NULL;
  }

  if (!handle) {
    if (!errno) {
      esyslog("[AVCHandler]: Incompatible IEEE1394 subsystem!\n");
    } else {
      esyslog("[AVCHandler]: IEEE1394 driver not loaded!\n");
    }
  } else {
    if (raw1394_set_port(handle, 0) < 0) {
      esyslog("[AVCHandler]: Couldn't set port");
      raw1394_destroy_handle(handle);
    } else {
      Start();
    }
  }
}

/* ---------------------------------------------------------------------------
 */
cAVCHandler::~cAVCHandler()
{
  active=false;
  Cancel(3);
  for (int i = 0; i < MAX_IEEE1394_DEVICES; ++i) {
    free(micData[i]);
    free(micDirectories[i]);
  }
  raw1394_destroy_handle(handle);
  dsyslog("[AVCHandler]: Good bye");
}

/* ---------------------------------------------------------------------------
 */
void
cAVCHandler::checkDevices(void)
{
    int               i, j;
    int               newDevices = 0;
    rom1394_directory newRomDirs[MAX_IEEE1394_DEVICES];
    octlet_t          newDeviceId[MAX_IEEE1394_DEVICES];

  //dsyslog(" ++ checkDevices ()\n");

  for (i=0; i < raw1394_get_nodecount(handle); ++i) {
    if (rom1394_get_directory(handle, i, &newRomDirs[i]) < 0) {
      continue;
    }

    if ((rom1394_get_node_type(&newRomDirs[i]) == ROM1394_NODE_TYPE_AVC) &&
        avc1394_check_subunit_type(handle, i, AVC1394_SUBUNIT_TYPE_VCR)) {
      newDevices++;
      newDeviceId[i] = rom1394_get_guid(handle,i);
      for (j = 0; j < numDevices; ++j) {
        if (newDeviceId[i] == deviceId[j]) {
          deviceId[j] = 0;
          break;
        }
      }
      /* ----------------------------------------------------------------------
       * report new device found
       */
      if (j >= numDevices) {
          unsigned char *tmpMicData = NULL;
          int           tmpMicSize = 0;
#if 0
// disabled since it may cause segfaults Skins.Message()
          char msg[100];

        snprintf (msg, 100, "%s: %s",
                  tr("New FireWire Device"),newRomDirs[i].label);
        Skins.Message(mtInfo,msg);
#endif
        avc1394_vcr_get_mic_info (handle, i, &tmpMicData, &tmpMicSize);

        avcMutex.Lock();
        micData[i] = tmpMicData;
        micSize[i] = tmpMicSize;
        avc1394_micinfo2micdirectory(&micDirectories[i], micData[i], micSize[i]);
        avc1394_dump_micdirectoy(micDirectories[i]);
        avcMutex.Unlock();
      }
    }
  }

#if 0 // disabled since it may cause segfaults Skins.Message()
  for (j = 0; j < numDevices; j++) {
    if (deviceId[j]) {
        char msg[100];

      snprintf (msg, 100, "%s: %s",
                tr("FireWire Device has been removed"),romDirs[j].label);
      Skins.Message(mtWarning,msg);
    }
  }
#endif

  if (newDevices != numDevices) {
    dsyslog("[AVCHandler] number of connected devices changed %d -> %d\n",
            numDevices, newDevices);

  }

  avcMutex.Lock();
  for (i = 0; i < newDevices; ++i) {
    romDirs[i] = newRomDirs[i];
    deviceId[i] = newDeviceId[i];
  }

  numDevices = newDevices;
  avcMutex.Unlock();
}

/* ---------------------------------------------------------------------------
 */
const char *
cAVCHandler::GetDeviceNameAt(int i)
{
    const char *name = NULL;

  avcMutex.Lock();

  if (numDevices && i < numDevices)
    name = romDirs[i].label;

  avcMutex.Unlock();
  return name;
}

/* ---------------------------------------------------------------------------
 */
octlet_t
cAVCHandler::GetGuidForName(const char *name)
{
    octlet_t  ret = 0;

  avcMutex.Lock();
  for (int i = 0; i < numDevices;++i)
  {
    if (!strcmp (romDirs[i].label, name))
    {
      ret = deviceId[i];
      break;
    }
  }
  avcMutex.Unlock();
  return ret;
}

/* ---------------------------------------------------------------------------
 */
bool
cAVCHandler::HasMicInfo(int i)
{
  return micSize[i] > 0;
}

/* ---------------------------------------------------------------------------
 */
const char *
cAVCHandler::TapePosition(int i, char *pos)
{
    int atn, m, mt, sfbf;

  if (avc1394_vcr_get_atn (handle, i, &atn, &m, &mt, &sfbf) == 0) {
    snprintf(pos, 16,
            "%02d:%02d:%02d.%02d",
            (atn / (12 * 25 * 60 * 60)) % 24,
            (atn / (12 * 25 * 60)) % 60,
            (atn / (12 * 25)) % 60,
            (atn / 12) % 25);
  }
  return pos;
}

/* ---------------------------------------------------------------------------
 */
int
cAVCHandler::NumDevices(void)
{
  return numDevices;
}

/* ---------------------------------------------------------------------------
 */
void
cAVCHandler::Action()
{
    int               fd, pollRc;
    struct pollfd     pfds[1];

  active = true;
  dsyslog("[AVCHandler]: monitor thread STARTUP\n");

  fd = raw1394_get_fd(handle);
  pfds[0].fd = fd;
  pfds[0].events = POLLIN|POLLPRI|POLLERR|POLLHUP;

  /* --------------------------------------------------------------------------
   * do and initial scan for connected devices before waiting for event.
   */
  checkDevices();

  while(active) {
    pollRc = poll(pfds, 1, 2000);
    if (pollRc == 0)
      continue;

    if (pollRc < 0) {
      //dsyslog("pollRc = %d\n", pollRc);
      continue;
    }

    usleep(30000);
    checkDevices();
    //usleep(500000);
    usleep(5000);
  }
  dsyslog("[AVCHandler]: monitor thread SHUTDOWN\n");
}

/* ----------------------------------------------------------------------------
 */
/* ----------------------------------------------------------------------------
 */
cAVCPlayer::cAVCPlayer(octlet_t deviceId)
{
  dsyslog("[cAVCPlayer()]");

  deviceOK = false;
  deviceNum = -1;
  paused = false;
  winding = false;
  playing = false;

#ifdef RAW1394_V_0_8
  handle = raw1394_get_handle();
#else
  handle = raw1394_new_handle();
#endif

  this->deviceId = deviceId;
  ap = (AVFormatParameters *) calloc (1, sizeof(AVFormatParameters));

  if (raw1394_set_port(handle, 0) >= 0) {
      rom1394_directory romDir;

    deviceOK = true;
    for (int i=0; i < raw1394_get_nodecount(handle); ++i) {
      if (rom1394_get_directory(handle, i, &romDir) < 0) {
        continue;
      }

      if ((rom1394_get_node_type(&romDir) == ROM1394_NODE_TYPE_AVC) &&
          avc1394_check_subunit_type(handle, i, AVC1394_SUBUNIT_TYPE_VCR) &&
          rom1394_get_guid(handle,i) == deviceId) {
        deviceNum = i;
        break;
      }
    }
  }

  /* --------------------------------------------------------------------------
   * Preset our well known format parameters. Standart should be taken from
   * somewhere else, but ..
   */
  ap->standard = "PAL";
  ap->video_codec_id = CODEC_ID_DVVIDEO;
  ap->audio_codec_id = CODEC_ID_DVAUDIO;

}

/* ----------------------------------------------------------------------------
 */
cAVCPlayer::~cAVCPlayer()
{
  running = false;
  Cancel(3);
  Detach();
  free(ap);
  raw1394_destroy_handle(handle);
  dsyslog("[~cAVCPlayer()]");
}

/* ----------------------------------------------------------------------------
 */
void
cAVCPlayer::Stop()
{
  dsyslog("[AVCPlayer-Stop]\n");
  running = false;
  Cancel(3);
}

/* ----------------------------------------------------------------------------
 */
void
cAVCPlayer::Activate(bool On)
{
  dsyslog("[AVCPlayer-Activate]: %s", (On) ? "ON" : "OFF");
  if (On) {
    Start();
  } else if (active) {
    running = false;
    Cancel(3);
    active = false;
  }
}

/* ---------------------------------------------------------------------------
 */
void
cAVCPlayer::Action(void)
{
    int               rc, packetCount = 0;
    cSoftDevice       *pDevice;
    AVPacket          pkt;
    int               nStreams=0;

  dsyslog("[AVCPlayer-Action] started");
  pDevice = (cSoftDevice *) cDevice::PrimaryDevice();

  fmt = av_find_input_format("dv1394");
  if (!fmt) {
    esyslog("[AVCPlayer-Action] inputformat dv1394 "
            "not supported by your ffmpeg version");
    return;
  }

  if ((rc = av_open_input_stream(&ic, NULL, "/dev/zero", fmt, ap)) < 0) {
    esyslog("[AVCPlayer-Action] open input stream failed (%d)", rc);
    return;
  }

  active = running = true;

  // limitation in cPlayer: we have to know the play mode at
  // start time. Workaraound set play mode again and
  // put the softdevice in packet mode
  pDevice->SetPlayMode( (ePlayMode) -pmAudioVideo );
  pDevice->PlayVideo((uchar *) ic,-1);

  while (running) {
      cPoller Poller;

    if ( ! DevicePoll(Poller, 100) ) {
      pollTimeouts++;
      if (pollTimeouts > 100) {
        cPlayer::DeviceClear();
        pollTimeouts=0;
      }
      continue;
    } else {
      pollTimeouts=0;
    }

    rc = av_read_frame(ic, &pkt);
    packetCount++;
    if (rc < 0) {
      fprintf(stderr,"Error (%d) reading packet!\n",rc);
      break;
    }
    if (paused) {
      DeviceFreeze();
      cPlayer::DeviceClear();
      while (paused && running)
        usleep(10000);
      DevicePlay();
      continue;
    }

    av_dup_packet(&pkt);

#if LIBAVFORMAT_BUILD > 4623
      AVRational time_base;

    time_base = ic->streams[pkt.stream_index]->time_base;
    if (pkt.pts != (int64_t) AV_NOPTS_VALUE) {
#if 0
      if (ic->streams[pkt.stream_index]->codec->codec_type == CODEC_TYPE_AUDIO)
        fprintf (stderr, "Au: %lld ->",pkt.pts);
      else if (ic->streams[pkt.stream_index]->codec->codec_type == CODEC_TYPE_VIDEO)
        fprintf (stderr, "Vi: %lld ->",pkt.pts);
      else
        fprintf (stderr, "??: %lld ->",pkt.pts);
#endif
      pkt.pts = av_rescale(pkt.pts,
                           AV_TIME_BASE * (int64_t)time_base.num,
                           time_base.den) / 100;
#if 0
      fprintf (stderr, " %lld\n",pkt.pts);
#endif
    }
#else
    if ( pkt.pts != (int64_t) AV_NOPTS_VALUE ) {
      pkt.pts/=100;
    }
#endif

    pDevice->PlayVideo((uchar *)&pkt,-2);

    if (nStreams!=ic->nb_streams ){
      packetCount=0;
      nStreams=ic->nb_streams;
      fprintf(stderr,"Streams: %d\n",nStreams);
      for (int i=0; i <nStreams; i++ ) {
#if LIBAVFORMAT_BUILD > 4628
          printf("Codec %d ID: %d\n",i,ic->streams[i]->codec->codec_id);
#else
          printf("Codec %d ID: %d\n",i,ic->streams[i]->codec.codec_id);
#endif
      }
    }

    if (packetCount % 100) {
      usleep(100);
    }

    if (packetCount == 200)
    dump_format(ic, 0, "test", 0);

  }

  if (running)
    DeviceFlush(20000);

  DeviceClear();

  running = active = false;
  dsyslog("[AVCPlayer-Action] stopped");
}

/* ---------------------------------------------------------------------------
 */
void
cAVCPlayer::Command(eKeys key)
{

  if (!deviceOK || deviceNum == -1)
    return;

  switch (key) {
    case kPause:
      if (winding)
        avc1394_vcr_stop(handle, deviceNum);

      if (paused)
        avc1394_vcr_play(handle, deviceNum);
      else
        avc1394_vcr_pause(handle, deviceNum);

      paused = !paused;
      winding = false;
      playing = false;
      break;
    case kPlay:
      if (winding)
        avc1394_vcr_stop(handle, deviceNum);

      if (!playing)
      {
        avc1394_vcr_play(handle, deviceNum);
        playing = true;
      }
      else
      {
        avc1394_vcr_stop(handle, deviceNum);
        playing = false;
      }

      paused = false;
      winding = false;
      break;
    case kFastFwd:
      if (winding)
        avc1394_vcr_stop(handle, deviceNum);

      avc1394_vcr_forward(handle, deviceNum);

      paused = false;
      winding = true;
      playing = false;
      break;
    case kFastRew:
      if (winding)
        avc1394_vcr_stop(handle, deviceNum);

      avc1394_vcr_rewind(handle, deviceNum);

      paused = false;
      winding = true;
      playing = false;
      break;
    case kStop:
      avc1394_vcr_stop(handle, deviceNum);
      paused = false;
      winding = false;
      playing = false;
      break;
    default:
      break;
  }
}

/* ---------------------------------------------------------------------------
 pcm_s16le*/
/* ---------------------------------------------------------------------------
 */
cAVCControl::cAVCControl(octlet_t deviceId) :
              cControl(player = new cAVCPlayer(deviceId))
{
  dsyslog("[cAVCControl()]");
}

/* ---------------------------------------------------------------------------
 */
cAVCControl::~cAVCControl()
{
  dsyslog("[~cAVCControl()]");
  delete player;
  player = NULL;
}

/* ---------------------------------------------------------------------------
 */
void
cAVCControl::Hide(void)
{
  dsyslog("[AVCControl-Hide]");
}

/* ---------------------------------------------------------------------------
 */
eOSState
cAVCControl::ProcessKey(eKeys key)
{
    eOSState state = cOsdObject::ProcessKey(key);

  if ( state != osUnknown) {
    return state;
  }

  if ( !player->running) {
    return osEnd;
  }

  if(state==osUnknown) {
    state = osContinue;

    switch (key) {
      case kPause: case kPlay: case kFastFwd: case kFastRew: case kStop:
        player->Command(key);
        break;
      default:
        break;
    }
  }
  return osContinue;
}
