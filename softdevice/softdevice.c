/*
 * softdevice.c: A plugin for the Video Disk Recorder
 *
 * See the README file for copyright information and how to reach the author.
 *
 * $Id: softdevice.c,v 1.7 2004/11/14 16:24:38 wachm Exp $
 */

#include <getopt.h>
#include <stdlib.h>
#include <vdr/interface.h>
#include <vdr/plugin.h>
#include <vdr/player.h>

#if VDRVERSNUM >= 10307
#include <vdr/osd.h>
#else
#include <vdr/osdbase.h>
#endif


#include <sys/mman.h>
#include <sys/ioctl.h>
#include <linux/fb.h>
#include "i18n.h"
#include "setup-softdevice.h"

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
static const char *VERSION        = "0.0.7";
static const char *DESCRIPTION    = "A software emulated MPEG2 device";
static const char *MAINMENUENTRY  = "Softdevice";

#define INBUF_SIZE 4096



#if VDRVERSNUM >= 10307

// --- cSoftOsd -----------------------------------------------
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

cSoftOsd::cSoftOsd(cVideoOut *VideoOut, int X, int Y) : cOsd(X, Y)
{
    videoOut = VideoOut;
    videoOut->OpenOSD(X, Y);
}

cSoftOsd::~cSoftOsd()
{
    if (videoOut) {
	videoOut->CloseOSD();
	videoOut=0;
    }
}

eOsdError cSoftOsd::CanHandleAreas(const tArea *Areas, int NumAreas)
{
    eOsdError Result = cOsd::CanHandleAreas(Areas, NumAreas);

    return Result;
}

void cSoftOsd::Flush(void)
{
    cBitmap *Bitmap;

    for (int i = 0; (Bitmap = GetBitmap(i)) != NULL; i++)
    {
	videoOut->Refresh(Bitmap);
    }
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

// --- cSoftDevice ------------------------------------------------------------
class cPluginSoftDevice : public cPlugin {
private:
  int voutMethod;

public:
  cPluginSoftDevice(void);
  virtual ~cPluginSoftDevice();
  virtual const char *Version(void) { return VERSION; }
  virtual const char *Description(void) { return tr(DESCRIPTION); }
  virtual const char *CommandLineHelp(void);
  virtual bool Initialize(void);
  virtual bool ProcessArgs(int argc, char *argv[]);
  virtual bool Start(void);
  virtual void Housekeeping(void);
  virtual const char *MainMenuEntry(void) { return tr(MAINMENUENTRY); }
  virtual cOsdObject *MainMenuAction(void);
  virtual cMenuSetupPage *SetupMenu(void);
  virtual bool SetupParse(const char *Name, const char *Value);
  };

// Global variables that control the overall behaviour:


// --- cSoftDevice --------------------------------------------------
class cSoftDevice : public cDevice {
private:
  cMpeg2Decoder *decoder;

#if VDRVERSNUM < 10307
  cOsdBase *OSD;
#endif

  cVideoOut *videoOut;
  cAudioOut *audioOut;
  int       outMethod;

  bool      freezeModeEnabled;
  cMutex    playMutex;
  cCondVar  readyForPlayCondVar;

public:
  cSoftDevice(int method = 0);
  ~cSoftDevice();
  virtual bool HasDecoder(void) const;
  virtual bool CanReplay(void) const;
  virtual bool SetPlayMode(ePlayMode PlayMode);
  virtual void TrickSpeed(int Speed);
  virtual void Clear(void);
  virtual void Play(void);
  virtual void Freeze(void);
  virtual void Mute(void);
  virtual void SetVolumeDevice (int Volume);
  virtual void StillPicture(const uchar *Data, int Length);
  virtual bool Poll(cPoller &Poller, int TimeoutMs = 0);
  virtual int64_t GetSTC(void);
  virtual int PlayVideo(const uchar *Data, int Length);
#if VDRVERSNUM >= 10307
  virtual int ProvidesCa(const cChannel *Channel) const;
  virtual void MakePrimaryDevice(bool On);
#else
  int ProvidesCa(int Ca);
  virtual cOsdBase *NewOsd(int x, int y);
#endif
};

cSoftDevice::cSoftDevice(int method)
{
    freezeModeEnabled = false;

    fprintf(stderr,"[softdevice] Initializing Video Out\n");
    fprintf(stderr,
            "[softdevice] ffmpeg version(%s) build(%d)\n",
            FFMPEG_VERSION, LIBAVCODEC_BUILD);
    setupStore.outputMethod = method;
    switch (method) {
      case VOUT_XV:
#ifdef XV_SUPPORT
        videoOut = new cXvVideoOut (setupStore. xvAspect);
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
        videoOut=new cFBVideoOut();
#endif
        break;
      case VOUT_DFB:
#ifdef DFB_SUPPORT
        videoOut=new cDFBVideoOut();
        videoOut->Initialize();
#endif
        break;
      case VOUT_VIDIX:
#ifdef VIDIX_SUPPORT
        videoOut=new cVidixVideoOut();
#endif
        break;
      default:
        break;
    }
    fprintf(stderr,"[softdevice] Video Out seems to be OK\n");
    fprintf(stderr,"[softdevice] Initializing Audio Out\n");
    audioOut=new cAlsaAudioOut(setupStore.alsaDevice);
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
    switch(PlayMode) {
      // FIXME - Implement audio or video only Playmode (is this really needed?)
      case pmAudioVideo:
          decoder->Start();
          playMutex.Lock();
          freezeModeEnabled = false;
          playMutex.Unlock();
	    break;
	default:
	    decoder->Stop();
	    break;

    }
//    printf("setting Playmode not implemented yet... %d\n",PlayMode);
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
    cDevice::Clear();
    decoder->Clear();
}
void cSoftDevice::Play(void)
{
    //fprintf(stderr,"[softdevice] Play...\n");
    cDevice::Play();
    decoder->TrickSpeed(1);
    decoder->Play();
    playMutex.Lock();
    freezeModeEnabled = false;
    playMutex.Unlock();
    readyForPlayCondVar.Broadcast();
}

void cSoftDevice::Freeze(void)
{
    //fprintf(stderr,"[softdevice] Freeze...\n");
    cDevice::Freeze();
    decoder->Freeze();
    playMutex.Lock();
    freezeModeEnabled = true;
    playMutex.Unlock();
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
  // fprintf(stderr,"[softdevice] Poll TimeoutMs: %d ....\n",TimeoutMs);
  playMutex.Lock();
  if (freezeModeEnabled)
    readyForPlayCondVar.TimedWait(playMutex,TimeoutMs);
  playMutex.Unlock();

  if (decoder->BufferFilled()) {
     //fprintf(stderr,"[softdevice] Buffer filled, sleeping\n");
     usleep(TimeoutMs*1000);
     return decoder->BufferFilled();
  }

  return true;
}


int cSoftDevice::PlayVideo(const uchar *Data, int Length)
{
    bool freezeMode;

    playMutex.Lock();
    freezeMode = freezeModeEnabled;
    playMutex.Unlock();

    if (freezeMode)
      return 0;

    int result=decoder->Decode(Data, Length);
    // restart the decoder
    if (result == -1) {
        delete(decoder);
        decoder= new cMpeg2Decoder(audioOut,videoOut);
        return 0;
    }
    return Length;
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
}

cPluginSoftDevice::~cPluginSoftDevice()
{
  // Clean up after yourself!
}

const char *cPluginSoftDevice::CommandLineHelp(void)
{
  // Return a string that describes all known command line options.
  return
  "  -ao alsa:devicename      alsa output device\n"
#ifdef XV_SUPPORT
  "  -vo xv:                  enable output via X11-Xv\n"
  "  -vo xv:aspect=wide       use a 16:9 display area (1024x576)\n"
  "  -vo xv:aspect=normal     use a  4:3 display area (768x576)\n"
#endif
#ifdef FB_SUPPORT
  "  -vo fb:                  enable output via framebuffer\n"
#endif
#ifdef DFB_SUPPORT
  "  -vo dfb:                 enable output via directFB\n"
#endif
#ifdef VIDIX_SUPPORT
  "  -vo vidix:               enable output via vidix driver\n"
#endif
  "\n";
}

bool cPluginSoftDevice::ProcessArgs(int argc, char *argv[])
{
  int i = 0;

  // Implement command line argument processing here if applicable.
  fprintf (stderr, "[softdevice] processing args\n");
  while (argc > 0) {
    fprintf (stderr, "[softdevice]   argv [%d] = %s\n", i, argv [i]);
    if (!strcmp (argv[i], "-vo")) {
      ++i;
      --argc;
      if (argc > 0) {
          char *vo_argv = argv[i];

        if (!strncmp (vo_argv, "xv:", 3)) {
          vo_argv += 3;
#ifdef XV_SUPPORT
          voutMethod = VOUT_XV;
          if (!strncmp (vo_argv, "aspect=", 7)) {
            vo_argv += 7;
            if (!strncmp (vo_argv, "wide", 4)) {
              fprintf (stderr,
                       "[ProcessArgs] xv: startup aspect ratio set to wide (16:9)\n");
              setupStore. xvAspect = XV_FORMAT_WIDE;
            } else if (!strncmp (vo_argv, "normal", 6)) {
              fprintf (stderr,
                       "[ProcessArgs] xv: startup aspect ratio set to normal (4:3)\n");
              setupStore. xvAspect = XV_FORMAT_NORMAL;
            } else {
              fprintf (stderr,
                       "[ProcessArgs] xv: illegal value for sub option aspect (%s)\n",
                       vo_argv);
            }
          }
#else
          fprintf(stderr,"[softdevice] xv support not compiled in\n");
#endif
        } else if (!strncmp (vo_argv, "fb:", 3)) {
          vo_argv += 3;
#ifdef FB_SUPPORT
          voutMethod = VOUT_FB;
#else
          fprintf(stderr,"[softdevice] fb support not compiled in\n");
#endif
        } else if (!strncmp (vo_argv, "dfb:", 4)) {
          vo_argv += 4;
#ifdef DFB_SUPPORT
          voutMethod = VOUT_DFB;
#else
          fprintf(stderr,"[softdevice] dfb support not compiled in\n");
#endif
        } else if (!strncmp (vo_argv, "vidix:", 6)) {
          vo_argv += 6;
#ifdef VIDIX_SUPPORT
          voutMethod = VOUT_VIDIX;
#else
          fprintf(stderr,"[softdevice] vidix support not compiled in\n");
#endif
        }

      }
    } else if (!strcmp (argv[i], "-ao")) {
      ++i; --argc;
      if (argc > 0) {
          char *ao_argv = argv[i];
        if (!strncmp(ao_argv, "alsa:", 5)) {
          ao_argv += 5;
          fprintf(stderr, "[softdevice] using alsa device %s\n", ao_argv);
          strncpy(setupStore.alsaDevice, ao_argv, ALSA_DEVICE_NAME_LENGTH);
        }
      }
    }
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
  new cSoftDevice(voutMethod);
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
  return new cMenuSetupSoftdevice;
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
