/*
 * avc-handler.c: handle optionally present AVC (firewire) devices.
 *
 * See the README file for copyright information and how to reach the author.
 *
 * $Id: avc-handler.c,v 1.2 2005/09/12 12:17:07 lucke Exp $
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
  for (int i = 0; i < MAX_IEEE1394_DEVICES; ++i)
  {
    deviceId[i] = 0;
    micSize[i] = 0;
    micData[i] = NULL;
  }

  if (!handle)
  {
    if (!errno)
    {
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
#if 0
// disabled since it may cause segfaults Skins.Message()
          char msg[100];
          //static int once = 1;

        snprintf (msg, 100, "%s: %s",
                  tr("New FireWire Device"),newRomDirs[i].label);
        Skins.Message(mtInfo,msg);
#endif
        avc1394_vcr_get_mic_info (handle, i, NULL, NULL);
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
    static int64_t    lastPTS=0;


  dsyslog("[AVCPlayer-Action] started");
  active = running = true;
  pDevice = (cSoftDevice *) cDevice::PrimaryDevice();

  fmt = av_find_input_format("dv1394");
  rc = av_open_input_stream(&ic, NULL, "/dev/zero", fmt, ap);

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

    lastPTS=pkt.pts;
    if ( pkt.pts != (int64_t) AV_NOPTS_VALUE ) {
      pkt.pts/=100;
    }

    pDevice->PlayVideo((uchar *)&pkt,-2);

    if (nStreams!=ic->nb_streams ){
      packetCount=0;
      nStreams=ic->nb_streams;
      fprintf(stderr,"Streams: %d\n",nStreams);
      for (int i=0; i <nStreams; i++ ) {
          printf("Codec %d ID: %d\n",i,ic->streams[i]->codec.codec_id);
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
