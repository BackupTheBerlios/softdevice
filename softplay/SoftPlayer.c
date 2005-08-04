/*
 * Media Player plugin for VDR
 *
 * Copyright (C) 2005 Martin Wache
 *
 * This code is distributed under the terms and conditions of the
 * GNU GENERAL PUBLIC LICENSE. See the file COPYING for details.
 *
 * $Id: SoftPlayer.c,v 1.10 2005/08/04 08:50:49 wachm Exp $
 */

#include "SoftPlayer.h"
#include "softplay.h"

//#define PLDBG(out...) { printf("PLDBG: ");printf(out);}
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
	int64_t PTS=0;
	int64_t lastPTS=0;

	PLDBG("Thread started: SoftPlayer\n");
	running=true;
	reading=true;
	pollTimeouts=0;
	pause=false;
	forward=true;
	new_forward=true;
	speed=-1;
	new_speed=-1;
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
#if LIBAVFORMAT_BUILD > 4618
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

                if ( speed != new_speed ) {
                        PLDBG("speed change\n");
                        cPlayer::DeviceClear();
                      //  if (new_speed!=-1) 
                        //        av_seek_frame(ic,-1,SoftDevice->GetSTC()/9*100);
                        
//                        if (new_speed != -1)
//                                cPlayer::DeviceTrickSpeed(2);
//                        else cPlayer::DeviceTrickSpeed(1);

                        fast_STC=SoftDevice->GetSTC()/9*100;
                        forward = new_forward;
                        speed = new_speed;
                };

                if ( speed!=-1 ) {
                        int step=5*100000; // 5 zehntel sek.
                        if (!forward)
                                step=-step;
                        fast_STC+=step;
#if LIBAVFORMAT_BUILD > 4618
			av_seek_frame(ic,-1,fast_STC,
			   AVSEEK_FLAG_BACKWARD);
#else
			av_seek_frame(ic,-1,fast_STC);
#endif
                      
                        PKTDBG("fast_STC %lld diff %lld forward %d step %d\n",
                               fast_STC,PTS-lastPTS,forward,step);
                };

		ret = av_read_frame(ic, &pkt);
		//ret = av_read_packet(ic, &pkt);
		PacketCount++;
		if (ret < 0) {
		        printf("Error (%d) reading packet!\n",ret);
			reading=false;
			continue;
		}
        
                // set audio index if not yet set
                if ( AudioIdx== -1 &&
#if LIBAVFORMAT_BUILD > 4628
                     ic->streams[pkt.stream_index]->codec->codec_type == CODEC_TYPE_AUDIO 
#else 
                     ic->streams[pkt.stream_index]->codec.codec_type == CODEC_TYPE_AUDIO 
#endif
                        )
                     AudioIdx=pkt.stream_index;
                   
                // set video index if not yet set
                if ( VideoIdx== -1 &&
#if LIBAVFORMAT_BUILD > 4628
                     ic->streams[pkt.stream_index]->codec->codec_type == CODEC_TYPE_VIDEO 
#else 
                     ic->streams[pkt.stream_index]->codec.codec_type == CODEC_TYPE_VIDEO 
#endif
                        )
                    VideoIdx=pkt.stream_index;
                    
                // skip packets which do not belong to the current streams
                if ( pkt.stream_index != VideoIdx &&
                     pkt.stream_index != AudioIdx ) {
			printf("Drop Packet PTS: %lld\n",pkt.pts);
                        continue;
		};
			
		lastPTS=PTS;
                PTS=pkt.pts;
#if LIBAVFORMAT_BUILD > 4623
		AVRational time_base;
                time_base=ic->streams[pkt.stream_index]->time_base;
		if ( pkt.pts != (int64_t) AV_NOPTS_VALUE ) {
			pkt.pts=av_rescale(pkt.pts, AV_TIME_BASE* (int64_t)time_base.num, time_base.den)/100 ;
                };

                //printf("PTS: %lld new %lld num %d den %d\n",PTS,pkt.pts,
                //                time_base.num,time_base.den);
#else
		if ( pkt.pts != (int64_t) AV_NOPTS_VALUE )
			pkt.pts/=100;
#endif
		//pkt.pts*=1000/AV_TIME_BASE;

		if (pause) {
			DeviceFreeze();
			while (pause && running)
		   		usleep(10000);
			DevicePlay();
		};
               
                av_dup_packet(&pkt);
		// length = -2 : queue packet
		PKTDBG("Queue Packet PTS: %lld\n",pkt.pts);
		SoftDevice->PlayVideo((uchar *)&pkt,-2);
		//SoftDevice->QueuePacket(ic,pkt);
		
		if ( nStreams != ic->nb_streams ) {
			PacketCount=0;
			nStreams=ic->nb_streams;
			fprintf(stderr,"Streams: %d\n",nStreams);
			for (int i=0; i <nStreams; i++ ) {
#if LIBAVFORMAT_BUILD > 4628
				printf("Codec %d ID: %d\n",i,ic->streams[i]->codec->codec_id);
#else 
				printf("Codec %d ID: %d\n",i,ic->streams[i]->codec.codec_id);
#endif
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
#if LIBAVFORMAT_BUILD > 4628
		if (IC->streams[i]->codec->codec_type == CODEC_TYPE_AUDIO)
#else
		if (IC->streams[i]->codec.codec_type == CODEC_TYPE_AUDIO)
#endif
			hasAudio=true;
#if LIBAVFORMAT_BUILD > 4628                        
		else if (IC->streams[i]->codec->codec_type == CODEC_TYPE_VIDEO)
#else
                else if (IC->streams[i]->codec.codec_type == CODEC_TYPE_VIDEO)
#endif
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
	if (!filename) {
                printf("Filname is NULL!!\n");
                ic=0;
                return;
        };
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
        } else return &ic->filename[Softplay->MediaPathLen()]; 
};

bool cSoftPlayer::GetIndex(int &Current, int &Total, bool SnapToIFrame ) {
	if (ic) {
		Current=(int) SoftDevice->GetSTC()/(9*10000)-ic->start_time/AV_TIME_BASE;
		Total=(ic->duration)/AV_TIME_BASE;
		//printf("duration %lld start_time %lld\n",ic->duration,ic->start_time);
                return true;
        } else {
                Current=0;
                Total=0;
                return false;
        };
        return false;
};
        
int cSoftPlayer::GetDuration() { 
        if (ic) {
		//printf("duration %lld start_time %lld\n",ic->duration,ic->start_time);		
                return (ic->duration)/AV_TIME_BASE; 
	} else return 0; 
};

int cSoftPlayer::GetCurrPos() { 
        if (SoftDevice) 
                return SoftDevice->GetSTC()/(9*10000)-ic->start_time/AV_TIME_BASE; 
        else return 0;
};

void cSoftPlayer::PlayFile(const char *file) {
        if (running)
                Stop(); 

        OpenFile(file);
        Activate(true);
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
		OsdActive=OsdProgress;
                displayReplay=Skins.Current()->DisplayReplay(false);
                currTitleHash=0;
        };

        char *Title=SoftPlayer->GetTitle();
        if ( currTitleHash!=SimpleHash(Title) ) {
                int TotalDuration=SoftPlayer->GetDuration();
                currTitleHash=SimpleHash(Title);
		displayReplay->SetTitle(Title);
		displayReplay->SetProgress(SoftPlayer->GetCurrPos()
                                ,TotalDuration);
		//PLDBG("CurrPos %d Duration %d\n",SoftPlayer->GetCurrPos()
		//  ,SoftPlayer->GetDuration());
		char str[60];
		sprintf(str,"%02d:%02d:%02d",TotalDuration/3600,
			TotalDuration/60%60,TotalDuration%60);
		displayReplay->SetTotal(str);
        };
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

eOSState cSoftControl::ProcessKey(eKeys Key) {
	eOSState state = cOsdObject::ProcessKey(Key);

        if ( state != osUnknown && state != osContinue ) {
		PLDBG("cOsdObject::ProcessKey processed a key :-)\n");
                 return state;
        };

	if ( OsdActive == OsdProgress ) 
                ShowProgress();

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
                if (state == PLAY_CURR_FILE ) {
                        char * nextFile;
                        if (!playList || !(nextFile=playList->CurrFile())) {
                                PLDBG("No playlist or no curr file. Ending.\n");
                                return osEnd;
                        };

                        SoftPlayer->PlayFile(nextFile);
                };
                if (state == osEnd || state == osBack) {
                        PLDBG("private menu osEnd\n");
                        delete privateMenu;
                        privateMenu=NULL;
                        OsdActive=OsdNone;
                        if (state == osEnd) {
                                SoftPlayer->Stop();
                                return osEnd;
                        };
                        return osContinue;
                };
                        
                return state;
        };


	if(state!=osUnknown) 
		return state;

	state = osContinue;
	switch (Key) {
		// Positioning:
		case k8:   
			if (Softplay->currList) {
				Hide();
				OsdActive=OsdPrivMenu;
				privateMenu=new cReplayList(Softplay->currList);
				privateMenu->Display();
				return osContinue;
			};
			break;
		case k5:   
			if (Softplay->currList) {
				Hide();
				OsdActive=OsdPrivMenu;
				privateMenu=new cEditList(Softplay->currList);
				privateMenu->Display();
				return osContinue;
			};
			break;
		case kGreen|k_Repeat:
		case kGreen:   
			if ( SoftPlayer->GetDuration() > 300 )
				SoftPlayer->SkipSeconds(-60); 
			else SoftPlayer->SkipSeconds(-15);
			break;
		case kYellow|k_Repeat:
		case kYellow: 
			if ( SoftPlayer->GetDuration() > 300 )
				SoftPlayer->SkipSeconds( 60);
			else SoftPlayer->SkipSeconds( 15);
			break;
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
		case kRight:
			     //SoftPlayer->FastForward();  
			     break;
		case kLeft:
			     //SoftPlayer->FastBackward();  
			     break;
		case kOk: if (OsdActive==OsdProgress)
				  Hide();
			else ShowProgress(); 
			break;
		case k9: if (playList) {
				 char * nextFile=playList->NextFile();
				 if (nextFile)
					 SoftPlayer->PlayFile(nextFile);
			 };
			 break;
		case k7: if (playList) {
				 char * prevFile=playList->PrevFile();
				 printf("play PrevFile %p\n",prevFile);
				 if (prevFile)
					 SoftPlayer->PlayFile(prevFile);
			 };
			 break;
		case k6: if (playList) {
				 char * nextFile=playList->NextAlbumFile();
				 if (nextFile)
					 SoftPlayer->PlayFile(nextFile);
			 };
			 break;
		case k4: if (playList) {
				 char * prevFile=playList->PrevAlbumFile();
				 if (prevFile)
					 SoftPlayer->PlayFile(prevFile);
			 };
			 break;

		default:
			 break;
	};
	return state;
};


