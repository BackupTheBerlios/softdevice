/*
 * softdevice.c: A plugin for the Video Disk Recorder
 *
 * See the README file for copyright information and how to reach the author.
 *
 * $Id: softdevice.c,v 1.33 2005/05/22 07:21:12 lucke Exp $
 */

#include "softdevice.h"

#include <getopt.h>
#include <stdlib.h>

#ifdef USE_SUBPLUGINS
#include <dlfcn.h>
#endif

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
static const char *VERSION        = "0.1.2";
static const char *DESCRIPTION    = "A software emulated MPEG2 device";
static const char *MAINMENUENTRY  = "Softdevice";

#define INBUF_SIZE 4096

#define AOUT_ALSA   1
#define AOUT_DUMMY  2

#if VDRVERSNUM >= 10307

// --- cSoftOsd -----------------------------------------------

/* ---------------------------------------------------------------------------
 */
class cSoftOsd : public cOsd {
private:
    cVideoOut *videoOut;
protected:
public:
    cSoftOsd(cVideoOut *VideoOut, int XOfs, int XOfs);
    virtual ~cSoftOsd();
    virtual eOsdError CanHandleAreas(const tArea *Areas, int NumAreas);
    virtual void Flush(void);
};

/* ---------------------------------------------------------------------------
 */
cSoftOsd::cSoftOsd(cVideoOut *VideoOut, int X, int Y) : cOsd(X, Y)
{
  videoOut = VideoOut;
  videoOut->OpenOSD(X, Y);
}

/* ---------------------------------------------------------------------------
 */
cSoftOsd::~cSoftOsd()
{
  if (videoOut)
  {
    videoOut->CloseOSD();
    videoOut=0;
  }
}

/* ---------------------------------------------------------------------------
 */
eOsdError cSoftOsd::CanHandleAreas(const tArea *Areas, int NumAreas)
{
    eOsdError Result = cOsd::CanHandleAreas(Areas, NumAreas);

    return Result;
}

/* ---------------------------------------------------------------------------
 */
void cSoftOsd::Flush(void)
{
    cBitmap *Bitmap;

  videoOut->Size(Width(),Height());
  videoOut->OSDStart();
  for (int i = 0; (Bitmap = GetBitmap(i)) != NULL; i++)
  {
    videoOut->Refresh(Bitmap);
  }
  videoOut->OSDCommit();
}

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
    videoOut->SetOsd(osd);
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
    fprintf(stderr,"[softdevice] OSD-Position at %d x %d\n",X,Y);
    videoOut->OpenOSD(X, Y);
}
cSoftOsd::~cSoftOsd() {
    if (videoOut) {
      videoOut->CloseOSD();
      videoOut=0;
    }
    fprintf(stderr,"[softdevice] OSD is off now\n");
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
  {
      char  *subPluginFileName = NULL;
      char  *outMethodName  = NULL;
      int   reconfigureArg  = 0;
    switch (method) {
    case VOUT_XV:
#ifdef XV_SUPPORT
      outMethodName = "xv";
      reconfigureArg = FOURCC_YV12;
#endif
      break;
    case VOUT_FB:
#ifdef FB_SUPPORT
      outMethodName = "fb";
#endif
      break;
    case VOUT_DFB:
#ifdef DFB_SUPPORT
      outMethodName = "dfb";
#endif
      break;
    case VOUT_VIDIX:
#ifdef VIDIX_SUPPORT
      outMethodName = "vidix";
#endif
      break;
    default:
      break;
    }

    asprintf (&subPluginFileName,
              "%s/%s%s.so.%s",
              pluginPath,
              "libvdr-softdevice-",
              outMethodName,
              VDRVERSION);
    void *handle = dlopen (subPluginFileName, RTLD_NOW);
    char *err = dlerror();
    if (!err)
    {
        void  *(*creator)(cSetupStore *store);

      creator = (void *(*)(cSetupStore *))dlsym(handle,
                                                "SubPluginCreator");
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
      if (videoOut->Initialize () &&
          videoOut->Reconfigure (reconfigureArg))
      {
          printf("Subplugin successfully opend\n");
          dsyslog("[softdevice] videoOut OK !\n");
      }
      else
      {
        esyslog("[softdevice] videoOut failure exiting\n");
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
      default:
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

int64_t cSoftDevice::GetSTC(void) {
  return decoder->GetSTC();
};

#if VDRVERSNUM >= 10307

void cSoftDevice::MakePrimaryDevice(bool On)
{
  fprintf(stderr,"cSoftDevice::MakePrimaryDevice\n");

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
    packetMode=PlayMode < 0;
    PlayMode=(ePlayMode) abs(PlayMode);
    ic=NULL;
    switch(PlayMode) {
      // FIXME - Implement audio or video only Playmode (is this really needed?)
      case pmAudioVideo:
          decoder->SetPlayMode(cMpeg2Decoder::PmAudioVideo);
          decoder->Start();
          break;
      case pmAudioOnly:
      case pmAudioOnlyBlack:
          decoder->SetPlayMode(cMpeg2Decoder::PmAudioOnly);
          decoder->Start();
          break;
#if VDRVERSNUM > 10310
      case pmVideoOnly:
          decoder->SetPlayMode(cMpeg2Decoder::PmVideoOnly);
          decoder->Start();
          break;
#endif
      case pmNone:
          decoder->Stop();
          break;
      default:
          printf("playmode not implemented... %d\n",PlayMode);
          decoder->Stop();
          break;
    }
    return true;
}

void cSoftDevice::TrickSpeed(int Speed)
{
    //fprintf(stderr,"[softdevice] Trickspeed(%d) ...\n",Speed);
    decoder->TrickSpeed(Speed);
}
void cSoftDevice::Clear(void)
{
    //fprintf(stderr,"[softdevice] Clear ...\n");
    if ( ! decoder )
      return;
      
    if ( !packetMode ) {
      cDevice::Clear();
      decoder->Clear();
    } else decoder->ClearPacketQueue();
}
void cSoftDevice::Play(void)
{
    //fprintf(stderr,"[softdevice] Play...\n");
    cDevice::Play();
    decoder->TrickSpeed(1);
    decoder->Play();
}

void cSoftDevice::Freeze(void)
{
    //fprintf(stderr,"[softdevice] Freeze...\n");
    cDevice::Freeze();
    decoder->Freeze();
}

void cSoftDevice::Mute(void)
{
    //fprintf(stderr,"[softdevice] Mute not implemented yet...\n");
    cDevice::Mute();
}

void cSoftDevice::SetVolumeDevice(int Volume)
{
  //fprintf (stderr, "[softdevice] should set volume to %d\n", Volume);
  audioOut->SetVolume(Volume);
}

void cSoftDevice::StillPicture(const uchar *Data, int Length)
{
    //fprintf(stderr,"[softdevice] StillPicture...\n");
    decoder->StillPicture((uchar *)Data,Length);
}

bool cSoftDevice::Poll(cPoller &Poller, int TimeoutMs)
{
   //fprintf(stderr,"[softdevice] Poll TimeoutMs: %d ....\n",TimeoutMs);

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
/* ----------------------------------------------------------------------------
 */
int cSoftDevice::PlayAudio(const uchar *Data, int Length)
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
     decoder->QueuePacket(ic,( AVPacket &) *Data);
     return -2;
  };
  return 0;
}

/* ----------------------------------------------------------------------------
 */
void cSoftDevice::SetAudioTrackDevice(eTrackType Type)
{
  //fprintf (stderr, "[SetAudioTrackDevice] (%d)\n",Type);
}

/* ----------------------------------------------------------------------------
 */
void cSoftDevice::SetDigitalAudioDevice(bool On)
{
  //fprintf (stderr, "[SetDigitalAudioDevice] (%s)\n",(On)? "TRUE":"FALSE");
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
     decoder->QueuePacket(ic,( AVPacket &) *Data);
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
{ return tr(MAINMENUENTRY); }

const char *cPluginSoftDevice::CommandLineHelp(void)
{
  // Return a string that describes all known command line options.
  return
  "  -ao alsa:devicename      alsa output device\n"
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
#endif
#ifdef VIDIX_SUPPORT
  "  -vo vidix:               enable output via vidix driver\n"
#endif
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
    };
    if (!strcmp (argv[i], "-vo")) {
      ++i;
      --argc;
      if (argc > 0) {
          char *vo_argv = argv[i];

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
        } else if (!strncmp (vo_argv, "dfb:", 4)) {
          vo_argv += 4;
          setupStore.voArgs = vo_argv;
#ifdef DFB_SUPPORT
          voutMethod = VOUT_DFB;
          if (!strncmp (vo_argv, "mgatv", 5))
            setupStore.useMGAtv = 1;
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
          fprintf(stderr, "[softdevice] using alsa device %s\n", ao_argv);
          strncpy(setupStore.alsaDevice, ao_argv, ALSA_DEVICE_NAME_LENGTH);
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
  fprintf (stderr, "[MainMenuAction]\n");
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
