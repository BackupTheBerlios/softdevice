/*
 * softdevice.h: A plugin for the Video Disk Recorder
 *
 * See the README file for copyright information and how to reach the author.
 *
 * $Id: softdevice.h,v 1.2 2005/03/20 12:21:27 wachm Exp $
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

  bool      freezeModeEnabled;
  cMutex    playMutex;
  cCondVar  readyForPlayCondVar;

public:
  cSoftDevice(int method, int audioMethod, char *pluginPath);
  ~cSoftDevice();
  
  void QueuePacket(const AVFormatContext *ic, AVPacket &pkt) 
  { if (decoder) decoder->QueuePacket(ic,pkt); };
  void ClearPacketQueue() 
  { if (decoder) decoder->ClearPacketQueue(); };


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
  virtual int  PlayAudio(const uchar *Data, int Length);
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
