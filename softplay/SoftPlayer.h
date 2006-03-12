/*
 * Media Player plugin for VDR
 *
 * Copyright (C) 2005 Martin Wache
 *
 * This code is distributed under the terms and conditions of the
 * GNU GENERAL PUBLIC LICENSE. See the file COPYING for details.
 *
 * $Id: SoftPlayer.h,v 1.6 2006/03/12 20:28:52 wachm Exp $
 */

#ifndef __SOFTPLAYER_H
#define __SOFTPLAYER_H

#include <vdr/device.h>
#include <vdr/player.h>

#include <avformat.h>

#include "SoftHandles.h"
#include "PlayList.h"
#include "Receiver.h"

class cSoftPlayer : public cPlayer, cThread {
 private:
       friend class cSoftControl;
       bool running;
       bool reading;
       bool pause;
       bool forward;
       bool new_forward;
       int speed;
       int new_speed;
       int skip;
       int AudioIdx;
       int VideoIdx;
       int64_t fast_STC;
       
       int pollTimeouts;
       cDevice *SoftDevice;
       PacketHandlesV100 SoftHandles;
       AVFormatContext *ic;
       AVFormatParameters ap;
       cSoftplayReceiver *Receiver;

       char curr_filename[200];
       char title[60];
       char author[60];
       char album[60];
       int duration;
       int start_time;
       
       char filename[200];
       bool newFile;
       int Streams;
 public:
       cSoftPlayer();  
       ~cSoftPlayer();

       virtual void Activate(bool On);

       virtual void Action();

       inline bool IsRunning() {return reading || newFile ;};
	
       inline void SkipSeconds(int Skip) 
       {skip=Skip;};

       inline void SetSpeed(int Speed)
       {new_speed=Speed;};
       inline void SetForward(bool Forward)
       {new_forward=Forward;};

       inline void FastForward()
       {new_speed=0;new_forward=true;};

       inline void FastBackward()
       {new_speed=0;new_forward=false;};

       void OpenFile(const char *filename);
       void PlayFile(const char *filename);
      
       void FileReplay();

       void RemuxAndQueue( AVPacket &pkt);

       int GetPlayMode(AVFormatContext *IC); 
       
       void Stop();

       inline void TogglePause()
       { pause=!pause; };
       inline void Pause()
       { pause=true; };

       inline void Play()
       { pause=false; new_speed=-1;new_forward=true; };

       inline const char * GetTitle()
       { return title; };
       inline const char * GetAuthor()
       { return author; };
       inline const char * GetAlbum()
       { return album; };
       inline const char * GetFilename()
       { return curr_filename; };
      
       inline int GetDuration()
       { return duration/AV_TIME_BASE; };
       
       int GetCurrPos();
       
       virtual bool GetIndex(int &Current, int &Total, 
        	bool SnapToIFrame = false);
		
       virtual bool GetReplayMode(bool &Play, bool &Forward, int &Speed)
       {Play=!pause;Forward=forward;Speed=speed;return true;};

       inline bool GetPlay() 
       { return !pause; };

       inline bool GetForward()
       { return forward; };

       inline int GetSpeed()
       { return speed; };

       // -- Receiver controls ----------------------------------
       void ChannelUpDown(int i);

 protected:
       void ResetDevice( int Streams);
       bool PollDevice( int Streams);
       void FlushDevice( int Streams);
       void FreezeDevice( int Streams, bool Freeze);

 };

class cSoftControl: public cControl {
  private:
      cSoftPlayer *SoftPlayer;
      cSoftplayReceiver *Receiver;
      
      cSkinDisplayReplay *displayReplay;
      cOsdMenu *privateMenu;
      enum eOsdType {
      	OsdNone,
	OsdProgress,
	OsdPrivMenu,
	} OsdActive;

      bool shouldStop;
      cPlayList *playList;

      // to notice when the next file starts
      bool newFile; 
  public:
     cSoftControl( const char * filename );
     cSoftControl( cPlayList *PlayList );
     virtual ~cSoftControl();
     virtual void Hide();
     virtual eOSState ProcessKey(eKeys Key);
     void ShowProgress();
     void SendStatus();
};


#endif
