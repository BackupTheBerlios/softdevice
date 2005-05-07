/*
 * Media Player plugin for VDR
 *
 * Copyright (C) 2005 Martin Wache
 *
 * This code is distributed under the terms and conditions of the
 * GNU GENERAL PUBLIC LICENSE. See the file COPYING for details.
 *
 * $Id: SoftPlayer.h,v 1.3 2005/05/07 20:05:42 wachm Exp $
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
       int speed;
       int skip;
       int AudioIdx;
       int VideoIdx;
       
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

       void OpenFile(const char *filename);
       
       ePlayMode GetPlayMode(AVFormatContext *IC); 
       
       void Stop();

       inline void Pause()
       { pause=true; };

       inline void Play()
       { pause=false; };

       char * GetTitle(); 
       virtual bool GetIndex(int &Current, int &Total, 
        	bool SnapToIFrame = false);
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
  public:
     cSoftControl( const char * filename );
     cSoftControl( cPlayList *PlayList );
     virtual ~cSoftControl();
     virtual void Hide();
     virtual eOSState ProcessKey(eKeys Key);
     void ShowProgress();
};


#endif
