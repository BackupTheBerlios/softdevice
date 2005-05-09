/*
 * Media Player plugin for VDR
 *
 * Copyright (C) 2005 Martin Wache
 *
 * This code is distributed under the terms and conditions of the
 * GNU GENERAL PUBLIC LICENSE. See the file COPYING for details.
 *
 * $Id: SoftPlayer.c,v 1.4 2005/05/09 21:40:05 wachm Exp $
 */

#include "SoftPlayer.h"
#include "softplay.h"

#define PLDBG(out...) { printf("PLDBG: ");printf(out);}
//#define PKTDBG(out...) {printf("PKTDBG: ");printf(out);}

#ifndef PLDBG
#define PLDBG(out...)
#endif

#ifndef PKTDBG
#define PKTDBG(out...)
#endif

// -------------------cSoftPlayer---------------------------------------
cSoftPlayer::cSoftPlayer() : cPlayer() {
	running=false;
	skip=0;
	SoftDevice=NULL;
	av_register_all();
};

cSoftPlayer::~cSoftPlayer() {
	PLDBG("~cSoftPlayer \n");	
	running=false;
	Cancel(3);
	Detach();
};

void cSoftPlayer::Stop() {
	running=false;
	Cancel(3);
};

void cSoftPlayer::Activate(bool On) {
	PLDBG("Activate %d \n",On);	
	if (On) {
	 	Start();
	} else {
	        // stop here
		Stop();
	};
};

void cSoftPlayer::Action() {
  	AVPacket  pkt;
  	int ret;
  	int nStreams=0;
	int PacketCount=0;
	static int64_t lastPTS=0;

	PLDBG("Thread started: SoftPlayer\n");
	running=true;
	reading=true;
	pollTimeouts=0;
	pause=false;
	forward=true;
	speed=1;
        AudioIdx=-1;
        VideoIdx=-1;

	if (!ic) {
	   printf("ic is null!!\n");
	   running= false;
	   reading= false;
	   return;
	};

	//SoftDevice=dynamic_cast<cSoftDevice *> (cDevice::PrimaryDevice());
	SoftDevice=(cSoftDevice *) (cDevice::PrimaryDevice());
	
	if (!SoftDevice) {
	   printf("the Softdevice has to be primary device!!\n");
	   exit(-1);
	};

	// limitation in cPlayer: we have to know the play mode at
	// start time. Workaraound set play mode again and
	// put the softdevice in packet mode
 	SoftDevice->SetPlayMode( (ePlayMode) -softPlayMode );
	// Length = -1: set format context 
	SoftDevice->PlayVideo((uchar *) ic,-1);
	while ( running  && reading ) {
		cPoller Poller;
      		if ( ! DevicePoll(Poller, 100) ) {
			//PLDBG("poll timeout!!!\n");
			pollTimeouts++;
			if (pollTimeouts > 100) {
				PLDBG("Too many poll timeouts! Reseting device.\n");
				cPlayer::DeviceClear();
				pollTimeouts=0;
			};
			continue;
		} else pollTimeouts=0;

                // seek forward / backward
		if (skip) {
		        PLDBG("player skip %d curr pts: %lld, lastPTS %lld\n",skip,
			      SoftDevice->GetSTC()/9*1000,lastPTS);
		 	//av_seek_frame(ic,-1,
			//   (SoftDevice->GetSTC()/9+skip*100)*100);
#if LIBAVFORMAT_BUILD > 4616
			av_seek_frame(ic,-1,
			   (SoftDevice->GetSTC()/9+skip*10000)*100,
			   AVSEEK_FLAG_BACKWARD);
#else
			av_seek_frame(ic,-1,
			   (SoftDevice->GetSTC()/9+skip*10000)*100);
#endif
 			PLDBG("clear\n");
			skip=0;
			cPlayer::DeviceClear();
			//SoftDevice->ClearPacketQueue();
			PLDBG("clear finished\n");
		};

		ret = av_read_frame(ic, &pkt);
		//ret = av_read_packet(ic, &pkt);
		PacketCount++;
		if (ret < 0) {
		        printf("Error (%d) reading packet!\n",ret);
			reading=false;
			continue;
		}
		av_dup_packet(&pkt);

		lastPTS=pkt.pts;
		if ( pkt.pts != (int64_t) AV_NOPTS_VALUE )
			pkt.pts/=100;
		//pkt.pts*=1000/AV_TIME_BASE;

		if (pause) {
			DeviceFreeze();
			while (pause)
		   		usleep(10000);
			DevicePlay();
		};

                // set audio index if not yet set
                if ( AudioIdx== -1 &&
                     ic->streams[pkt.stream_index]->codec.codec_type == CODEC_TYPE_AUDIO )
                        AudioIdx=pkt.stream_index;
                   
                // set video index if not yet set
                if ( VideoIdx== -1 &&
                     ic->streams[pkt.stream_index]->codec.codec_type == CODEC_TYPE_VIDEO )
                        VideoIdx=pkt.stream_index;
               
                // skip packets which do not belont to the current streams
                if ( pkt.stream_index != VideoIdx &&
                     pkt.stream_index != AudioIdx )
                        continue;
                 
		// length = -2 : queue packet
		PKTDBG("Queue Packet PTS: %lld\n",pkt.pts);
		SoftDevice->PlayVideo((uchar *)&pkt,-2);
		//SoftDevice->QueuePacket(ic,pkt);
		
		if ( nStreams != ic->nb_streams ) {
			PacketCount=0;
			nStreams=ic->nb_streams;
			fprintf(stderr,"Streams: %d\n",nStreams);
			for (int i=0; i <nStreams; i++ ) {
				printf("Codec %d ID: %d\n",i,ic->streams[i]->codec.codec_id);
			};
		};

		if (PacketCount == 200)
			dump_format(ic, 0, "test", 0);
	}
	if (running) 
	   DeviceFlush(20000);
	
	DeviceClear();
        // force a softdevice reset
 	SoftDevice->SetPlayMode( pmNone );
	running=false;
	PLDBG("Thread beendet : SoftPlayer \n");
};

ePlayMode cSoftPlayer::GetPlayMode(AVFormatContext *IC) {
	int i;
	bool hasVideo=false;
	bool hasAudio=false;
        
	PLDBG("GetPlayMode nb_streams %d\n",IC->nb_streams);
	for (i = 0; i<IC->nb_streams; i++) {
		PLDBG("GetPlayMode stream %d codec_type %d\n",
			i, IC->streams[i]->codec.codec_type);
		if (IC->streams[i]->codec.codec_type == CODEC_TYPE_AUDIO)
			hasAudio=true;
		else if (IC->streams[i]->codec.codec_type == CODEC_TYPE_VIDEO)
			hasVideo=true;
	};

	if ( hasVideo && hasAudio)
	  return pmAudioVideo;
	else if ( hasVideo )
	  return pmVideoOnly;
	else if ( hasAudio )
	  return pmAudioOnly;
        
	return pmNone;
};

void cSoftPlayer::OpenFile(const char *filename) {
        int ret;
        printf("open file %s\n",filename);
        char str[60];
        if ( (ret=av_open_input_file( &ic, filename, NULL, 0, NULL)) ) {
                snprintf(str,60,"%s %s!","Could not open file",filename);
                Skins.Message(mtError, str);
                printf("could not open file. Return value %d\n",ret);
                ic=0;
                return;
        };

        if ( av_find_stream_info( ic ) ) {
                printf("could not find stream info\n");
        };
        softPlayMode=GetPlayMode( ic );
};

char *cSoftPlayer::GetTitle()  { 
        if (!ic) 
                return NULL;

        if (ic->title[0]!=0) {
                snprintf(title,120,"%s - %s - %s",
                                ic->author,ic->album,ic->title);
                return title;
        } else return ic->filename; 
};

bool cSoftPlayer::GetIndex(int &Current, int &Total, bool SnapToIFrame ) {
        Current=(int) SoftDevice->GetSTC()/(9*10000);
        Total=ic->duration/AV_TIME_BASE;
        return true;
};
        
int cSoftPlayer::GetDuration() { 
        if (ic) 
                return ic->duration/AV_TIME_BASE; 
        else return 0; 
};

int cSoftPlayer::GetCurrPos() { 
        if (SoftDevice) 
                return SoftDevice->GetSTC()/(9*10000); 
        else return 0;
};

// -------------------cSoftControl----------------------------------------

cSoftControl::cSoftControl(const char * filename ) : 
        cControl(new cSoftPlayer) {
  displayReplay=NULL;
  OsdActive=OsdNone;
  privateMenu=NULL;
  displayReplay=NULL;
  shouldStop=false;
  playList=NULL;
  SoftPlayer = dynamic_cast<cSoftPlayer*> (player);
  SoftPlayer->OpenFile(filename);
};

cSoftControl::cSoftControl(cPlayList * PlayList ) : 
        cControl(new cSoftPlayer) {
  // FIXME delete displayReplay and privateMenu
  displayReplay=NULL;
  privateMenu=NULL;
  
  OsdActive=OsdNone;
  shouldStop=false;
  playList=PlayList;
  playList->PrepareForPlayback();
  SoftPlayer = dynamic_cast<cSoftPlayer*> (player);
  char *nextfile=PlayList->NextFile();
  if (nextfile)
	  SoftPlayer->OpenFile(nextfile);
};

cSoftControl::~cSoftControl() {
	PLDBG("~cSoftControl()\n");
	Hide();
	delete player;
	SoftPlayer = NULL;
	player = NULL;
};

void cSoftControl::Hide() {
	if (OsdActive==OsdProgress) {
		delete displayReplay;
		displayReplay=NULL;
		OsdActive=OsdNone;
	} else if (OsdActive==OsdPrivMenu) {
                delete privateMenu;
                privateMenu=NULL;
                OsdActive=OsdNone;
        };
                
};

void cSoftControl::ShowProgress() {
	if ( OsdActive!=OsdProgress ) {
	 	int TotalDuration=SoftPlayer->GetDuration();
		displayReplay=Skins.Current()->DisplayReplay(false);
		OsdActive=OsdProgress;
		displayReplay->SetTitle(SoftPlayer->GetTitle());
		displayReplay->SetProgress(SoftPlayer->GetCurrPos()
		  ,TotalDuration);
		PLDBG("CurrPos %d Duration %d\n",SoftPlayer->GetCurrPos()
		  ,SoftPlayer->GetDuration());
		char str[60];
		sprintf(str,"%02d:%02d:%02d",TotalDuration/3600,
			TotalDuration/60%60,TotalDuration%60);
		displayReplay->SetTotal(str);
	};
};		

eOSState cSoftControl::ProcessKey(eKeys Key) {
	eOSState state = cOsdObject::ProcessKey(Key);

        if ( state != osUnknown && state != osContinue ) {
		PLDBG("cOsdObject::ProcessKey processed a key :-)\n");
                 return state;
        };

	if ( OsdActive == OsdProgress ) {
                int CurrentPos=SoftPlayer->GetCurrPos();
	  	displayReplay->SetProgress(CurrentPos,
		    SoftPlayer->GetDuration());
               	char str[60];
		sprintf(str,"%02d:%02d:%02d",CurrentPos/3600,
			CurrentPos/60%60,CurrentPos%60);
		displayReplay->SetCurrent(str);
		PLDBG("CurrPos %d Duration %d\n",SoftPlayer->GetCurrPos()
		  ,SoftPlayer->GetDuration());
	};


	if ( !SoftPlayer->IsRunning()  ) {
		PLDBG("SoftPlayer not runnig. Looking for next file\n");
                char * nextFile;
                if (!playList || !(nextFile=playList->NextFile())) {
                        PLDBG("No playlist or no next file. Ending.\n");
                        return osEnd;
                };
  		
                SoftPlayer->OpenFile(nextFile);
		SoftPlayer->Activate(true);
	};

        if ( OsdActive == OsdPrivMenu  && privateMenu) {
                state = privateMenu->ProcessKey(Key);
                if (state == osUser3 ) {
                        char * nextFile;
                        SoftPlayer->Stop(); 
                        if (!playList || !(nextFile=playList->NextFile())) {
                                PLDBG("No playlist or no next file. Ending.\n");
                                return osEnd;
                        };

                        SoftPlayer->OpenFile(nextFile);
                        SoftPlayer->Activate(true);
                };
                if (state == osEnd || state == osBack) {
                        PLDBG("private menu osEnd\n");
                        delete privateMenu;
                        privateMenu=NULL;
                        OsdActive=OsdNone;
                        return osContinue;
                };
                        
                return state;
        };


	if(state==osUnknown) {
		state = osContinue;

		switch (Key) {
			// Positioning:
			case kRed:   
                                if (Softplay->currList) {
                                        Hide();
                                        OsdActive=OsdPrivMenu;
                                        privateMenu=Softplay->currList->ReplayList();
                                        privateMenu->Display();
                                        return osContinue;
                                };
                                break;
			case kGreen|k_Repeat:
			case kGreen:   SoftPlayer->SkipSeconds(-60); break;
			case kYellow|k_Repeat:
			case kYellow:  SoftPlayer->SkipSeconds( 60); break;
			case kBlue:    
                                SoftPlayer->Stop(); 
			        shouldStop=true;  
                                return osEnd;
                                break;
			case kPause: SoftPlayer->TogglePause(); break;
			case kUp:
			case kPlay:
			              SoftPlayer->Play();  break;
			case kDown:
			              SoftPlayer->Pause();  break;
			case kOk: if (OsdActive==OsdProgress)
					Hide();
				  else ShowProgress(); 
				  break;
			default:
			   break;
		};
	};
	return osContinue;
};


