/*
 * softdevice.h: A plugin for the Video Disk Recorder
 *
 * See the README file for copyright information and how to reach the author.
 *
 * $Id: softdevice.h,v 1.8 2006/02/06 06:58:49 lucke Exp $
 */

#ifndef __SOFTDEVICE_H__
#define __SOFTDEVICE_H__
#include <vdr/interface.h>
#include <vdr/plugin.h>
#include <vdr/player.h>

#include "i18n.h"
#include "setup-softdevice.h"
#include "audio.h"
#include "mpeg2decoder.h"
#include "utils.h"

// ---- Service interface ----------------------------------------------------


typedef void (* fQueuePacket)(cDevice *Device, AVFormatContext *ic, AVPacket &pkt);

typedef int (* SoftdeviceHandle)(cDevice *Device,int Stream, int value);

struct PacketHandlesV100{
        fQueuePacket  QueuePacket;
        SoftdeviceHandle ResetDecoder;
        SoftdeviceHandle BufferFill;
        SoftdeviceHandle Freeze;
        // value = 0 Play, value = 1 Pause
};
        

static const char *const GET_PACKET_HANDEL_IDV100="softdevice-GetPacketHandles-v1.0";
// 

// --- cSoftDevice ------------------------------------------------------------
class cPluginSoftDevice : public cPlugin {
private:
  int   voutMethod;
  int   aoutMethod;
  char  *pluginPath;

public:
  cPluginSoftDevice(void);
  virtual ~cPluginSoftDevice();
  virtual const char *Version(void); 
  virtual const char *Description(void);
  virtual const char *CommandLineHelp(void);
  virtual bool Initialize(void);
  virtual bool ProcessArgs(int argc, char *argv[]);
  virtual bool Start(void);
  virtual void Housekeeping(void);
  virtual const char *MainMenuEntry(void);
  virtual cOsdObject *MainMenuAction(void);
  virtual cMenuSetupPage *SetupMenu(void);
  virtual bool SetupParse(const char *Name, const char *Value);
#if VDRVERSNUM >= 10330
  virtual bool Service(const char *Id, void *Data = NULL);
#endif
};


class cSoftDevice : public cDevice {
private:
  cMpeg2Decoder *decoder;

#if VDRVERSNUM < 10307
  cOsdBase *OSD;
#endif

  cVideoOut *videoOut;
  cAudioOut *audioOut;
  int       outMethod;

  bool      packetMode;
  AVFormatContext *ic;

public:
  cSoftDevice(int method, int audioMethod, char *pluginPath);
  ~cSoftDevice();
  
  inline void QueuePacket(AVFormatContext *ic, AVPacket &pkt) 
  { if (decoder) decoder->QueuePacket(ic,pkt,true); };
  inline int ResetDecoder(int Stream) 
  { if (decoder) decoder->ResetDecoder(Stream); return 0;};
  inline int BufferFill(int Stream)
  { if (decoder) return decoder->BufferFill(Stream); return 0;};
  inline int Freeze(int Stream, int Value)
  { if (decoder) decoder->Freeze(Stream,Value); return 0;};

  void LoadSubPlugin(char *outMethodName, int reconfigureArg, char *pluginPath);

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
  virtual bool Flush(int TimeoutMs = 0);
  virtual int64_t GetSTC(void);
  virtual int PlayVideo(const uchar *Data, int Length);
#if VDRVERSNUM < 10318
  virtual void PlayAudio(const uchar *Data, int Length);
#else
  virtual void SetAudioChannelDevice(int AudioChannel);
  virtual int  GetAudioChannelDevice(void);
  virtual void SetDigitalAudioDevice(bool On);
  virtual void SetAudioTrackDevice(eTrackType Type);

# if VDRVERSNUM >= 10342
  virtual int  PlayAudio(const uchar *Data, int Length, uchar Id);
# else
  virtual int  PlayAudio(const uchar *Data, int Length);
# endif

#endif
#if VDRVERSNUM >= 10307
  virtual int ProvidesCa(const cChannel *Channel) const;

private:
  cSpuDecoder *spuDecoder;

public:
  virtual cSpuDecoder *GetSpuDecoder(void);

  virtual void MakePrimaryDevice(bool On);
#else
  int ProvidesCa(int Ca);
  virtual cOsdBase *NewOsd(int x, int y);
#endif
};

#endif
