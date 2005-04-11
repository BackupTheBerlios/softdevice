/*
 * Media Player plugin for VDR
 *
 * Copyright (C) 2005 Martin Wache
 *
 * This code is distributed under the terms and conditions of the
 * GNU GENERAL PUBLIC LICENSE. See the file COPYING for details.
 *
 * $Id: SoftPlayer.h,v 1.1 2005/04/11 16:03:32 wachm Exp $
 */

#ifndef __SOFTPLAYER_H
#define __SOFTPLAYER_H

#include <vdr/device.h>
#include <vdr/player.h>

#include <avformat.h>

#include "../softdevice/softdevice.h"
//#include "../softdevice/mpeg2decoder.h"

class cSoftPlayer : public cPlayer, cThread {
 private:
       bool running;
       bool reading;
       int pollTimeouts;
       AVFormatContext *ic;
       AVFormatParameters ap;
    
       int skip;
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
 };

class cSoftControl: public cControl {
  private:
      cSoftPlayer *SoftPlayer;
  public:
     cSoftControl( const char * filename );
     virtual ~cSoftControl();
     virtual void Hide() {};
     virtual eOSState ProcessKey(eKeys Key);
};


#endif
