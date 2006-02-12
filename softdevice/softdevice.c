/*
 * softdevice.c: A plugin for the Video Disk Recorder
 *
 * See the README file for copyright information and how to reach the author.
 *
 * $Id: softdevice.c,v 1.54 2006/02/12 17:59:12 lucke Exp $
 */

#include "softdevice.h"

#include <getopt.h>
#include <stdlib.h>

#include <dlfcn.h>

#if VDRVERSNUM >= 10307
#include <vdr/osd.h>
#include <vdr/dvbspu.h>
#else
#include <vdr/osdbase.h>
#endif


#include <sys/mman.h>
#include <sys/ioctl.h>
#include <linux/fb.h>
#include "video-dummy.h"

#undef  VOUT_DEFAULT

#ifdef HAVE_CONFIG
# include "config.h"
#endif

#ifdef VIDIX_SUPPORT
#include "video-vidix.h"

#ifndef VOUT_DEFAULT
#define VOUT_DEFAULT  VOUT_VIDIX
#endif

#endif


#ifdef FB_SUPPORT
#include "video-fb.h"

#ifndef VOUT_DEFAULT
#define VOUT_DEFAULT  VOUT_FB
#endif

#endif

#ifdef SHM_SUPPORT
#include "video-shm.h"
#endif

#ifdef DFB_SUPPORT
#include "video-dfb.h"

#ifndef VOUT_DEFAULT
#define VOUT_DEFAULT  VOUT_DFB
#endif

#endif

#ifdef XV_SUPPORT
#include "video-xv.h"

#ifndef VOUT_DEFAULT
#define VOUT_DEFAULT  VOUT_XV
#endif

#endif

#include "audio.h"
#include "mpeg2decoder.h"
#include "utils.h"
static const char *VERSION        = "0.2.2";
static const char *DESCRIPTION    = "A software emulated MPEG2 device";
static const char *MAINMENUENTRY  = "Softdevice";

#define INBUF_SIZE 4096

#define AOUT_ALSA   1
#define AOUT_DUMMY  2

//#define SOFTDEB(out...) {printf("softdeb[%04d]:",(int)(getTimeMilis() % 10000));printf(out);}

#ifndef SOFTDEB
#define SOFTDEB(out...)
#endif

#if VDRVERSNUM >= 10307

#include "SoftOsd.h"
// --- cSoftOsdProvider -----------------------------------------------
class cSoftOsdProvider : public cOsdProvider {
private:
    cVideoOut *videoOut;
    cOsd *osd;
public:
    cSoftOsdProvider(cVideoOut *VideoOut);
    virtual cOsd *CreateOsd(int Left, int Top);
};

cSoftOsdProvider::cSoftOsdProvider(cVideoOut *VideoOut) : cOsdProvider()
{
    videoOut = VideoOut;
}

cOsd * cSoftOsdProvider::CreateOsd(int Left, int Top)
{
    osd = new cSoftOsd(videoOut, Left, Top);
    return osd;
}

#else

// --- cSoftOsd -----------------------------------------------
class cSoftOsd : public cOsdBase {
private:
  int xOfs, yOfs;
  cVideoOut *videoOut;
protected:
  virtual bool OpenWindow(cWindow *Window);
  virtual void CommitWindow(cWindow *Window);
  virtual void ShowWindow(cWindow *Window);
  virtual void HideWindow(cWindow *Window, bool Hide);
  virtual void MoveWindow(cWindow *Window, int X, int Y);
  virtual void CloseWindow(cWindow *Window);
public:
  cSoftOsd(cVideoOut *VideoOut, int XOfs, int XOfs);
  virtual ~cSoftOsd();
};

cSoftOsd::cSoftOsd(cVideoOut *VideoOut, int X, int Y)
    :cOsdBase(X, Y) {
    videoOut=VideoOut;
    xOfs=X; // this position should be recalculated
    yOfs=Y;
    //fprintf(stderr,"[softdevice] OSD-Position at %d x %d\n",X,Y);
    videoOut->OpenOSD(X, Y);
}
cSoftOsd::~cSoftOsd() {
    if (videoOut) {
      videoOut->CloseOSD();
      videoOut=0;
    }
    //fprintf(stderr,"[softdevice] OSD is off now\n");
}

bool cSoftOsd::OpenWindow(cWindow *Window) {
    return videoOut->OpenWindow(Window);
}

void cSoftOsd::CommitWindow(cWindow *Window) {
    videoOut->CommitWindow(Window);
}

void cSoftOsd::ShowWindow(cWindow *Window) {
    videoOut->ShowWindow(Window);
}
void cSoftOsd::HideWindow(cWindow *Window, bool Hide) {
    videoOut->HideWindow(Window, Hide);
}

void cSoftOsd::MoveWindow(cWindow *Window, int x, int y) {
    videoOut->MoveWindow(Window, x, y);
}

void cSoftOsd::CloseWindow(cWindow *Window) {
    videoOut->CloseWindow(Window);
}

#endif

/* ----------------------------------------------------------------------------
 */
cSoftDevice::cSoftDevice(int method,int audioMethod, char *pluginPath)
{
#if VDRVERSNUM >= 10307
    spuDecoder = NULL;
#endif
    fprintf(stderr,"[softdevice] Initializing Video Out\n");
    fprintf(stderr,
            "[softdevice] ffmpeg version(%s) build(%d)\n",
            FFMPEG_VERSION, LIBAVCODEC_BUILD);
    setupStore.outputMethod = method;
#ifdef USE_SUBPLUGINS
    switch (method) {
      case VOUT_XV:
#ifdef XV_SUPPORT
        LoadSubPlugin ("xv", FOURCC_YV12, pluginPath);
#endif
        break;
      case VOUT_FB:
#ifdef FB_SUPPORT
        LoadSubPlugin ("fb", 0, pluginPath);
#endif
        break;
      case VOUT_DFB:
#ifdef DFB_SUPPORT
        LoadSubPlugin ("dfb", 0, pluginPath);
#endif
        break;
      case VOUT_VIDIX:
#ifdef VIDIX_SUPPORT
        LoadSubPlugin ("vidix", 0, pluginPath);
#endif
        break;
      case VOUT_SHM:
#ifdef SHM_SUPPORT
        LoadSubPlugin ("shm", 0, pluginPath);
#endif
        break;
      case VOUT_DUMMY:
        videoOut=new cDummyVideoOut(&setupStore);
        break;
      default:
        esyslog("[softdevice] NO video out specified exiting\n");
        exit(1);
        break;
    }
#else
    switch (method) {
      case VOUT_XV:
#ifdef XV_SUPPORT
        videoOut = new cXvVideoOut (&setupStore);
        if (videoOut->Initialize () && videoOut->Reconfigure (FOURCC_YV12)) {
          fprintf (stderr, "[softdevice] Xv out OK !\n");
        } else {
          fprintf (stderr, "[softdevice] Xv out failure !\n");
          exit (1);
        }
#endif
        break;
      case VOUT_FB:
#ifdef FB_SUPPORT
        videoOut=new cFBVideoOut(&setupStore);
#endif
        break;
      case VOUT_SHM:
#ifdef SHM_SUPPORT
        videoOut=new cShmVideoOut(&setupStore);
#endif
        break;
      case VOUT_DFB:
#ifdef DFB_SUPPORT
        videoOut=new cDFBVideoOut(&setupStore);
        videoOut->Initialize();
#endif
        break;
      case VOUT_VIDIX:
#ifdef VIDIX_SUPPORT
        videoOut=new cVidixVideoOut(&setupStore);
#endif
        break;
      case VOUT_DUMMY:
        videoOut=new cDummyVideoOut(&setupStore);
        break;
      default:
        esyslog("[softdevice] NO video out specified exiting\n");
        exit(1);
        break;
    }
#endif
    fprintf(stderr,"[softdevice] Video Out seems to be OK\n");
    fprintf(stderr,"[softdevice] Initializing Audio Out\n");
    switch (audioMethod) {
      case AOUT_ALSA:
        audioOut=new cAlsaAudioOut(&setupStore);
        break;
      case AOUT_DUMMY:
        audioOut=new cDummyAudioOut(&setupStore);
        break;
    }
    fprintf(stderr,"[softdevice] Audio out seems to be OK\n");
    fprintf(stderr,"[softdevice] A/V devices initialized, now initializing MPEG2 Decoder\n");
    decoder= new cMpeg2Decoder(audioOut, videoOut);
}

cSoftDevice::~cSoftDevice()
{
    delete(decoder);
    delete(audioOut);
    delete(videoOut);
}

/* ----------------------------------------------------------------------------
 */
void cSoftDevice::LoadSubPlugin(char *outMethodName,
                                int reconfigureArg,
                                char *pluginPath)
{
    char  *subPluginFileName = NULL;

  asprintf (&subPluginFileName,
            "%s/%s%s.so.%s",
            pluginPath,
            "libsubvdr-softdevice-",
            outMethodName,
            VDRVERSION);
  void *handle = dlopen (subPluginFileName, RTLD_NOW);
  char *err = dlerror();
  if (!err)
  {
      void  *(*creator)(cSetupStore *store);

    creator = (void *(*)(cSetupStore *))dlsym(handle, "SubPluginCreator");
    err = dlerror();
    if (!err)
    {
      videoOut = (cVideoOut *) creator (&setupStore);
    }
    else
    {
      esyslog("[softdevice] could not load (%s)[%s] exiting\n",
              "SubPluginCreator", err);
      esyslog("[softdevice] Did you use the -L option?\n");
      fprintf(stderr,"[softdevice] could not load (%s)[%s] exiting\n",
              "SubPluginCreator", err);
      fprintf(stderr,"[softdevice] Did you use the -L option?\n");
      exit(1);
    }
    if (videoOut->Initialize () && videoOut->Reconfigure (reconfigureArg))
    {
      fprintf(stderr,"[softdevice] Subplugin successfully opend\n");
      dsyslog("[softdevice] videoOut OK !\n");
    }
    else
    {
      esyslog("[softdevice] videoOut failure exiting\n");
      fprintf(stderr,"[softdevice] videoOut failure exiting\n");
      exit (1);
    }

  }
  else
  {
    esyslog("[softdevice] could not load (%s)[%s] exiting\n",
            subPluginFileName, err);
    exit(1);
  }
}

int64_t cSoftDevice::GetSTC(void) {
  return (decoder?decoder->GetSTC():(int64_t)NULL);
};

#if VDRVERSNUM >= 10307

void cSoftDevice::MakePrimaryDevice(bool On)
{
  //fprintf(stderr,"cSoftDevice::MakePrimaryDevice\n");

  new cSoftOsdProvider(videoOut);
}

int cSoftDevice::ProvidesCa(const cChannel *Channel) const
{
    return 0;
}

cSpuDecoder *cSoftDevice::GetSpuDecoder(void) 
{
  //printf("GetSpuDecoder %x\n",spuDecoder);
  if (IsPrimaryDevice() && !spuDecoder)
    spuDecoder = new cDvbSpuDecoder();
  return spuDecoder;
};

#else

cOsdBase *cSoftDevice::NewOsd(int X, int Y)
{
    return new cSoftOsd(videoOut,X,Y);
}

int cSoftDevice::ProvidesCa(int Ca)
{
    return 0;
}

#endif

bool cSoftDevice::HasDecoder(void) const
{
    return true; // We can decode MPEG2
}

bool cSoftDevice::CanReplay(void) const
{
    return true;  // We can replay
}

bool cSoftDevice::SetPlayMode(ePlayMode PlayMode)
{
    if (!decoder)
       return false;
    
    packetMode=PlayMode < 0;
    PlayMode=(ePlayMode) abs(PlayMode);
    ic=NULL;
    switch(PlayMode) {
      // FIXME - Implement audio or video only Playmode (is this really needed?)
      case pmAudioVideo:
          decoder->SetPlayMode(cMpeg2Decoder::PmAudioVideo,packetMode);
          decoder->Start();
          break;
      case pmAudioOnly:
      case pmAudioOnlyBlack:
          decoder->SetPlayMode(cMpeg2Decoder::PmAudioOnly,packetMode);
          decoder->Start();
          break;
#if VDRVERSNUM > 10310
      case pmVideoOnly:
          decoder->SetPlayMode(cMpeg2Decoder::PmVideoOnly,packetMode);
          decoder->Start();
          break;
#endif
      case pmNone:
          decoder->Stop();
          break;
      default:
          printf("playmode not implemented... %d\n",PlayMode);
          break;
    }
    return true;
}

void cSoftDevice::TrickSpeed(int Speed)
{
    SOFTDEB("Trickspeed(%d) ...\n",Speed);
    if (decoder)
      decoder->TrickSpeed(Speed);
}
void cSoftDevice::Clear(void)
{
    SOFTDEB("Clear ...\n");
    if ( ! decoder )
      return;
      
    if ( !packetMode ) {
      cDevice::Clear();
      decoder->Clear();
    } else decoder->ResetDecoder();
}
void cSoftDevice::Play(void)
{
    SOFTDEB("Play...\n");
    cDevice::Play();
    if (decoder) {
      decoder->TrickSpeed(1);
      decoder->Play();
    };
}

void cSoftDevice::Freeze(void)
{
    SOFTDEB("Freeze...\n");
    cDevice::Freeze();
    if (decoder)
      decoder->Freeze();
}

void cSoftDevice::Mute(void)
{
    SOFTDEB("Mute not implemented yet...\n");
    cDevice::Mute();
}

void cSoftDevice::SetVolumeDevice(int Volume)
{
  SOFTDEB("should set volume to %d\n", Volume);
  audioOut->SetVolume(Volume);
}

void cSoftDevice::StillPicture(const uchar *Data, int Length)
{
    SOFTDEB("StillPicture...\n");
    if (decoder)
      decoder->StillPicture((uchar *)Data,Length);
}

bool cSoftDevice::Poll(cPoller &Poller, int TimeoutMs)
{
  SOFTDEB("[softdevice] Poll TimeoutMs: %d ....\n",TimeoutMs);

  if ( decoder->BufferFill() > 90 ) {
     //fprintf(stderr,"[softdevice] Buffer filled, TimeoutMs %d, fill %d\n",
     //  TimeoutMs,decoder->BufferFill());
     int64_t TimeoutUs=TimeoutMs*1000;
     cRelTimer Timer;
     Timer.Reset();
  
     while ( TimeoutUs > 0 ) {
       usleep(10000);
       TimeoutUs-=Timer.GetRelTime();
     };
     
     return decoder->BufferFill() < 99;
  }

  return true;
}

bool cSoftDevice::Flush(int TimeoutMs)
{
  int64_t TimeoutUs=TimeoutMs*1000;
  cRelTimer Timer;
  Timer.Reset();
  
  while ( TimeoutUs > 0 && decoder->BufferFill() > 0 ) {
       usleep(10000);
       TimeoutUs-=Timer.GetRelTime();
  };
      
  return  decoder->BufferFill() == 0;
};

#if VDRVERSNUM < 10318
/* ----------------------------------------------------------------------------
 */
void cSoftDevice::PlayAudio(const uchar *Data, int Length)
{
}
#else

# if VDRVERSNUM >= 10342
/* ----------------------------------------------------------------------------
 */
int cSoftDevice::PlayAudio(const uchar *Data, int Length, uchar Id)
# else
/* ----------------------------------------------------------------------------
 */
int cSoftDevice::PlayAudio(const uchar *Data, int Length)
# endif
{
  //fprintf(stderr,"PlayAudio...\n");
  if (! packetMode)
    return decoder->Decode(Data, Length);

  if (Length==-1) {
     // Length = -1 : pass pointer to format context
     ic=(AVFormatContext *) Data;
     return -1;
  };
  if ( packetMode && ic && Length == -2 ) {
     // Length = -2 : pass pointer to packet
     decoder->QueuePacket(ic,( AVPacket &) *Data,true);
     return -2;
  };
  return 0;
}

/* ----------------------------------------------------------------------------
 */
void cSoftDevice::SetAudioTrackDevice(eTrackType Type)
{
  SOFTDEB("SetAudioTrackDevice (%d)\n",Type);
}

/* ----------------------------------------------------------------------------
 */
void cSoftDevice::SetDigitalAudioDevice(bool On)
{
  SOFTDEB("SetDigitalAudioDevice (%s)\n",(On)? "TRUE":"FALSE");
}

/* ----------------------------------------------------------------------------
 */
void cSoftDevice::SetAudioChannelDevice(int AudioChannel)
{
  if (decoder)
     decoder->SetAudioMode(AudioChannel);
}

/* ----------------------------------------------------------------------------
 */
int  cSoftDevice::GetAudioChannelDevice(void)
{
  if (decoder)
    return decoder->GetAudioMode();
  else return 0;
}

#endif

/* ----------------------------------------------------------------------------
 */
int cSoftDevice::PlayVideo(const uchar *Data, int Length)
{
   if (! packetMode)
    return decoder->Decode(Data, Length);

  if (Length==-1) {
     // Length = -1 : pass pointer to format context
     ic=(AVFormatContext *) Data;
     return -1;
  };
  if ( packetMode && ic && Length == -2 ) {
     // Length = -2 : pass pointer to packet
     decoder->QueuePacket(ic,( AVPacket &) *Data,true);
     return -2;
  };
  return 0;
}

// --- cPluginSoftDevice ----------------------------------------------------------

cPluginSoftDevice::cPluginSoftDevice(void)
{
  // Initialize any member variables here.
  // DON'T DO ANYTHING ELSE THAT MAY HAVE SIDE EFFECTS, REQUIRE GLOBAL
  // VDR OBJECTS TO EXIST OR PRODUCE ANY OUTPUT!
#ifdef VOUT_DEFAULT
  voutMethod = VOUT_DEFAULT;
#else
  voutMethod = 0;
#endif
  aoutMethod = AOUT_ALSA;
  pluginPath = PLUGINLIBDIR;
}

cPluginSoftDevice::~cPluginSoftDevice()
{
  // Clean up after yourself!
}

const char *cPluginSoftDevice::Version(void) 
{ return VERSION; }

const char *cPluginSoftDevice::Description(void) 
{ return tr(DESCRIPTION); }

const char *cPluginSoftDevice::MainMenuEntry(void)
{
  if (setupStore.mainMenu)
    return tr(MAINMENUENTRY);
  return NULL;
}

const char *cPluginSoftDevice::CommandLineHelp(void)
{
  // Return a string that describes all known command line options.
  return
  "  -ao alsa:pcm=dev_name#   alsa output device for analog and PCM out\n"
  "  -ao alsa:ac3=dev_name#   alsa output device for AC3 passthrough\n"
  "  -ao dummy:               dummy output device\n"
#ifdef XV_SUPPORT
  "  -vo xv:                  enable output via X11-Xv\n"
  "  -vo xv:aspect=wide       use a 16:9 display area (1024x576)\n"
  "  -vo xv:aspect=normal     use a  4:3 display area (768x576)\n"
  "  -vo xv:max-area          use maximum available area\n"
  "  -vo xv:full              startup fullscreen\n"
#endif
#ifdef FB_SUPPORT
  "  -vo fb:                  enable output via framebuffer\n"
#endif
#ifdef DFB_SUPPORT
  "  -vo dfb:                 enable output via directFB\n"
  "  -vo dfb:mgatv                   output via MATROX TV-out\n"
  "  -vo dfb:viatv                   output via Unichrome TV-out\n"
  "  -vo dfb:triple                  enables triple buffering on back end scaler\n"
#endif
#ifdef VIDIX_SUPPORT
  "  -vo vidix:               enable output via vidix driver\n"
#endif
  "  -vo dummy:               enable output to dummy device\n"
  "  -L <plugin_path_name>    search path for loading subplugins\n"
  "\n";
}

bool cPluginSoftDevice::ProcessArgs(int argc, char *argv[])
{
  int i = 0;

  // Implement command line argument processing here if applicable.
  fprintf (stderr, "[softdevice] processing args\n");
  while (argc > 0) {
    fprintf (stderr, "[softdevice]   argv [%d] = %s\n", i, argv [i]);
    if ( !strcmp(argv[i], "softdevice") ) {
        i++;
        argc--;
	continue;
    };
    if (!strcmp (argv[i], "-vo")) {
      ++i;
      --argc;
      if (argc > 0) {
          char *vo_argv = argv[i];

	  printf("vo_argv: %s \n",vo_argv);
        if (!strncmp (vo_argv, "xv:", 3)) {
          vo_argv += 3;
          setupStore.voArgs = vo_argv;
#ifdef XV_SUPPORT
          voutMethod = VOUT_XV;
          while (strlen(vo_argv) > 1) {
            if (*vo_argv == ':')
               ++vo_argv;

            if (!strncmp (vo_argv, "aspect=", 7)) {
              vo_argv += 7;
              if (!strncmp (vo_argv, "wide", 4)) {
                fprintf (stderr,
                         "[ProcessArgs] xv: startup aspect ratio set to wide (16:9)\n");
                setupStore. xvAspect = XV_FORMAT_WIDE;
                vo_argv += 4;
              } else if (!strncmp (vo_argv, "normal", 6)) {
                fprintf (stderr,
                         "[ProcessArgs] xv: startup aspect ratio set to normal (4:3)\n");
                setupStore. xvAspect = XV_FORMAT_NORMAL;
                vo_argv += 6;
              } else {
                fprintf (stderr,
                         "[ProcessArgs] xv: illegal value for sub option aspect (%s)\n",
                         vo_argv);
                break;
              }
            } else if (!strncmp (vo_argv, "max-area", 8)) {
              setupStore.xvMaxArea = 1;
              fprintf (stderr,
                       "[ProcessArgs] xv: using max available area\n");
              vo_argv += 8;
            } else if (!strncmp (vo_argv, "full", 4)) {
              setupStore.xvFullscreen = 1;
              fprintf (stderr,
                       "[ProcessArgs] xv: start up fullscreen\n");
              vo_argv += 4;
            } else {  
                    fprintf(stderr,"[softdevice] ignoring unrecognized option \"%s\"!\n",argv[i]);
                    esyslog("[softdevice] ignoring unrecognized option \"%s\"\n",argv[i]);
              break;
            }
          }
#else
          fprintf(stderr,"[softdevice] xv support not compiled in\n");
#endif
        } else if (!strncmp (vo_argv, "fb:", 3)) {
          vo_argv += 3;
          setupStore.voArgs = vo_argv;
#ifdef FB_SUPPORT
          voutMethod = VOUT_FB;
#else
          fprintf(stderr,"[softdevice] fb support not compiled in\n");
#endif
        } else if (!strncmp (vo_argv, "shm:", 3)) {
          vo_argv += 4;
          setupStore.voArgs = vo_argv;
#ifdef SHM_SUPPORT
          voutMethod = VOUT_SHM;
#else
          fprintf(stderr,"[softdevice] shm support not compiled in\n");
#endif
        } else if (!strncmp (vo_argv, "dfb:", 4)) {
          vo_argv += 4;
          setupStore.voArgs = vo_argv;
#ifdef DFB_SUPPORT
          voutMethod = VOUT_DFB;
          if (!strncmp (vo_argv, "mgatv", 5))
            setupStore.useMGAtv = 1;
          else if (!strncmp (vo_argv, "viatv", 5)) {
            setupStore.viaTv = 1;
            fprintf(stderr,"[softdevice] enabling field parity\n");
          } else if (!strncmp (vo_argv, "triple", 6)) {
            setupStore.tripleBuffering = 1;
            fprintf(stderr,"[softdevice] enabling triple buffering\n");
          }
#else
          fprintf(stderr,"[softdevice] dfb support not compiled in\n");
#endif
        } else if (!strncmp (vo_argv, "vidix:", 6)) {
          vo_argv += 6;
          setupStore.voArgs = vo_argv;
#ifdef VIDIX_SUPPORT
          voutMethod = VOUT_VIDIX;
#else
          fprintf(stderr,"[softdevice] vidix support not compiled in\n");
#endif
        } else if (!strncmp (vo_argv, "dummy:", 6)) {
          vo_argv += 6;
          setupStore.voArgs = vo_argv;
          voutMethod = VOUT_DUMMY;
          fprintf(stderr,"[softdevice] using dummy video out\n");
        } else {
                fprintf(stderr,"[softdevice] ignoring unrecognized option \"%s\"!\n",argv[i]);
                esyslog("[softdevice] ignoring unrecognized option \"%s\"\n",argv[i]);
        };


      }
    } else if (!strcmp (argv[i], "-ao")) {
      ++i; --argc;
      if (argc > 0) {
          char *ao_argv = argv[i];
        if (!strncmp(ao_argv, "alsa:", 5)) {
          ao_argv += 5;
          setupStore.aoArgs = ao_argv;
          aoutMethod = AOUT_ALSA;
          while (strlen(ao_argv) > 1) {
              char  *sep;
              int   len;

            if (*ao_argv == ':' || *ao_argv == '#')
              ++ao_argv;

            sep = ao_argv;
            while (*sep && *sep != '#')
              ++sep;
            if (!*sep || *sep != '#')
              sep = NULL;

            if (!strncmp(ao_argv, "pcm=", 4)) {
              ao_argv += 4;
              len = (sep) ? sep - ao_argv : 0;
              len = (len > ALSA_DEVICE_NAME_LENGTH) ?
                      ALSA_DEVICE_NAME_LENGTH : len;
              if (len) {
                strncpy(setupStore.alsaDevice, ao_argv, len);
                setupStore.alsaDevice [len] = 0;
                fprintf (stderr, "[softdevice] using PCM alsa device %s\n",
                         setupStore.alsaDevice);
              }
              ao_argv += len;
            } else if (!strncmp(ao_argv, "ac3=", 4)) {
              ao_argv += 4;
              len = (sep) ? sep - ao_argv : 0;
              len = (len > ALSA_DEVICE_NAME_LENGTH) ?
                      ALSA_DEVICE_NAME_LENGTH : len;
              if (len) {
                strncpy(setupStore.alsaAC3Device, ao_argv, len);
                setupStore.alsaAC3Device [len] = 0;
                fprintf (stderr, "[softdevice] using AC3 alsa device %s\n",
                         setupStore.alsaAC3Device);
              }
              ao_argv += len;
            } else {
              fprintf(stderr, "[softdevice] using alsa device %s\n", ao_argv);
              strncpy(setupStore.alsaDevice, ao_argv, ALSA_DEVICE_NAME_LENGTH);
              ao_argv += strlen(ao_argv);
            }

          }
        } else if (!strncmp(ao_argv, "dummy:", 6)) {
          ao_argv += 6;
          setupStore.aoArgs = ao_argv;
          aoutMethod = AOUT_DUMMY;
        }     else {
                fprintf(stderr,"[softdevice] ignoring unrecognized option \"%s\"!\n",argv[i]);
                esyslog("[softdevice] ignoring unrecognized option \"%s\"\n",argv[i]);
        };

      }
    } else if (!strcmp (argv[i], "-L")) {
      ++i; --argc;
      if (argc > 0) {
        pluginPath = argv[i];
      }
    } else {
            fprintf(stderr,"[softdevice] ignoring unrecognized option \"%s\"!\n",argv[i]);
            esyslog("[softdevice] ignoring unrecognized option \"%s\"\n",argv[i]);
    };
    ++i;
    --argc;
  }
  return true;
}

#if VDRVERSNUM >= 10330
static void QueuePacketFct(cDevice *Device, AVFormatContext *ic, AVPacket &pkt) {
        cSoftDevice *device = dynamic_cast <cSoftDevice*>(Device);

        if (device)
                device->QueuePacket(ic,pkt);
        else printf("Device is not the softdevice!!\n");
};

static int BufferFillFct(cDevice *Device, int Stream, int value =-1) {
        cSoftDevice *device = dynamic_cast <cSoftDevice*>(Device);

        if (device)
                return device->BufferFill(Stream);
        else printf("Device is not the softdevice!!\n");
        return 0;
};

static int FreezeFct(cDevice *Device, int Stream, int value =-1) {
        cSoftDevice *device = dynamic_cast <cSoftDevice*>(Device);

        if (device)
                return device->Freeze(Stream,value);
        else printf("Device is not the softdevice!!\n");
        return 0;
};

static int ResetDecoderFct(cDevice *Device, int Stream, int value =-1) {
        cSoftDevice *device = dynamic_cast <cSoftDevice*>(Device);

        if (device)
                device->ResetDecoder(Stream);
        else printf("Device is not the softdevice!!\n");
        return 0;
};
 
bool cPluginSoftDevice::Service(const char *Id, void *Data ) {
        printf("Service '%s'\n",Id);
        struct PacketHandlesV100 *Handles=(struct PacketHandlesV100 *) Data;
        if ( strcmp(Id,GET_PACKET_HANDEL_IDV100) == 0 ) {
                if ( Data ) {
                        Handles->QueuePacket=&QueuePacketFct;
                        Handles->ResetDecoder = &ResetDecoderFct;
                        Handles->BufferFill = &BufferFillFct;
                        Handles->Freeze = &FreezeFct;
                };
                return true;
        };
        return false;
};
#endif


bool cPluginSoftDevice::Start(void)
{
  // Start any background activities the plugin shall perform.
  RegisterI18n(Phrases);
  return true;
}

bool cPluginSoftDevice::Initialize(void)
{
  // Start any background activities the plugin shall perform.
  fprintf(stderr,"[softdevice] initializing Plugin\n");
  new cSoftDevice(voutMethod,aoutMethod,pluginPath);
  return true;
}

void cPluginSoftDevice::Housekeeping(void)
{
  // Perform any cleanup or other regular tasks.
}

cOsdObject *cPluginSoftDevice::MainMenuAction(void)
{
  // Perform the action when selected from the main VDR menu.
  SOFTDEB("MainMenuAction\n");
  return new cMenuSetupSoftdevice(this);
//  return NULL;
}

cMenuSetupPage *cPluginSoftDevice::SetupMenu(void)
{
  // Return our setup menu
  return new cMenuSetupSoftdevice;
}

bool cPluginSoftDevice::SetupParse(const char *Name, const char *Value)
{
  // Parse your own setup parameters and store their values.
  return setupStore.SetupParse(Name, Value);
}

VDRPLUGINCREATOR(cPluginSoftDevice); // Don't touch this!
