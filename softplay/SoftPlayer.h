/*
 * Media Player plugin for VDR
 *
 * Copyright (C) 2005 Martin Wache
 *
 * This code is distributed under the terms and conditions of the
 * GNU GENERAL PUBLIC LICENSE. See the file COPYING for details.
 *
 * $Id: SoftPlayer.h,v 1.5 2005/05/16 19:07:54 wachm Exp $
 */

#ifndef __SOFTPLAYER_H
#define __SOFTPLAYER_H

#include <vdr/device.h>
#include <vdr/player.h>

#include <avformat.h>

#include "../softdevice/softdevice.h"
#include "PlayList.h"

class cSoftPlayer : public cPlayer, cThread {
 private:
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
	cSoftDevice *SoftDevice;
       AVFormatContext *ic;
       AVFormatParameters ap;
   	char title[120];
   
       ePlayMode softPlayMode;
 public:
       cSoftPlayer();  
       ~cSoftPlayer();

       virtual void Activate(bool On);

       virtual void Action();

       inline bool IsRunning() {return running;};
	
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
       
       ePlayMode GetPlayMode(AVFormatContext *IC); 
       
       void Stop();

       inline void TogglePause()
       { pause=!pause; };
       inline void Pause()
       { pause=true; };

       inline void Play()
       { pause=false; new_speed=-1;new_forward=true; };

       char * GetTitle(); 
       virtual bool GetIndex(int &Current, int &Total, 
        	bool SnapToIFrame = false);
       virtual bool GetReplayMode(bool &Play, bool &Forward, int &Speed)
       {Play=!pause;Forward=forward;Speed=speed;return true;};
       int GetDuration(); 
       int GetCurrPos();
 };

class cSoftControl: public cControl {
  private:
      cSoftPlayer *SoftPlayer;
      
      cSkinDisplayReplay *displayReplay;
      cOsdMenu *privateMenu;
      enum eOsdType {
      	OsdNone,
	OsdProgress,
	OsdPrivMenu,
	} OsdActive;

      bool shouldStop;
      cPlayList *playList;
      int32_t currTitleHash;
  public:
     cSoftControl( const char * filename );
     cSoftControl( cPlayList *PlayList );
     virtual ~cSoftControl();
     virtual void Hide();
     virtual eOSState ProcessKey(eKeys Key);
     void ShowProgress();
};


#endif
