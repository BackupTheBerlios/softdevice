/*
 * Media Player plugin for VDR
 *
 * Copyright (C) 2005 Martin Wache
 *
 * This code is distributed under the terms and conditions of the
 * GNU GENERAL PUBLIC LICENSE. See the file COPYING for details.
 *
 * $Id: SoftPlayer.c,v 1.16 2007/03/12 21:32:03 wachm Exp $
 */

#include "SoftPlayer.h"
#include "softplay.h"
#include "PlayListMenu.h"
#include "FileIndex.h"

#include "vdr/status.h"

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
        reading=false;
        *filename=0;
        *curr_filename = *album = *title = *author = 0;
        start_time = duration = 0;

	skip=0;
	SoftDevice=NULL;
        ic=NULL;
        Receiver=NULL;
	av_register_all();
};

cSoftPlayer::~cSoftPlayer() {
	PLDBG("~cSoftPlayer \n");	
	running=false;
        reading=false;
	Cancel(3);
	Detach();
        if (Receiver)
                delete Receiver;
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


void cSoftPlayer::RemuxAndQueue(AVPacket &pkt) {
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
                return;
        };

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


        av_dup_packet(&pkt);
        // length = -2 : queue packet
        PKTDBG("Queue Packet index: %d PTS: %lld\n",pkt.stream_index,pkt.pts);
#if VDRVERSNUM >= 10330
        SoftHandles.QueuePacket(SoftDevice,ic,pkt);
#else
        SoftDevice->PlayVideo((uchar *)&pkt,-2);
#endif           
};

void cSoftPlayer::ResetDevice(int Streams) {
#if VDRVERSNUM >= 10330
        SoftHandles.ResetDecoder(SoftDevice,Streams,0);
#else
        cPlayer::DeviceClear();
#endif
};
 
bool cSoftPlayer::PollDevice(int Streams) {
#if VDRVERSNUM >= 10330
        int count=0;
        while ( SoftHandles.BufferFill(SoftDevice,Streams,0) > 95 
                        && count++ < 10 )
                usleep(5000);
        return (SoftHandles.BufferFill(SoftDevice,Streams,0) > 95); 
#else
        cPoller Poller;
        return DevicePoll(Poller, 100);
#endif        
};

void cSoftPlayer::FlushDevice(int Streams) {
#if VDRVERSNUM >= 10330
	PLDBG("FlushDevice BufferFill(%d) at start: %d\n",
			Streams,SoftHandles.BufferFill(SoftDevice,Streams,0));
        int count=0;        
        while ( SoftHandles.BufferFill(SoftDevice,Streams,0) > 0 
                        && count++ < 200 )
                usleep(50000);
	PLDBG("FlushDevice BufferFill(%d) at end: %d\n",
			Streams,SoftHandles.BufferFill(SoftDevice,Streams,0));
#else
        DeviceFlush(20000);
#endif        
};

void cSoftPlayer::FreezeDevice(int Streams, bool Freeze) {
#if VDRVERSNUM >= 10330
	PLDBG("FreezeDevice(%d,%d)\n",Streams,Freeze);
        SoftHandles.Freeze(SoftDevice,Streams,Freeze);
#else
	if (Freeze)
		DeviceFreeze();
	else DevicePlay();
#endif        
};

void cSoftPlayer::FileReplay() {
        AVPacket  pkt;
        int ret;
        int PacketCount=0;
        int64_t PTS=0;
        int64_t lastPTS=0;

        pollTimeouts=0;
	pause=false;
	forward=true;
	new_forward=true;
	speed=-1;
	new_speed=-1;
        AudioIdx=-1;
        VideoIdx=-1;
	reading=true;

        OpenFile(filename);
        newFile=false;


        if (!ic) {
	   printf("ic is null!!\n");
	   reading= false;
	   return;
	};

        if (Receiver && (Streams & SOFTDEVICE_VIDEO_STREAM) ) {
                Receiver->StopTransfer();
	        DeviceClear();
        };
        if (Receiver && !(Streams & SOFTDEVICE_VIDEO_STREAM) )
                Receiver->StartTransfer();
       
        ResetDevice(Streams);
        while ( running && reading && !newFile) {
                if ( PollDevice(Streams) ) {
                        //PLDBG("poll timeout!!!\n");
                        pollTimeouts++;
                        if (pollTimeouts > 100) {
                                PLDBG("Too many poll timeouts! Reseting device.\n");
                                ResetDevice(Streams);
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
                        ResetDevice(Streams);
                        //SoftDevice->ClearPacketQueue();
                        PLDBG("clear finished\n");
                };

                if ( speed != new_speed ) {
                        PLDBG("speed change\n");
                        ResetDevice(Streams);
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
                
                if (pause) {
                        FreezeDevice(Streams,1);
                        while (pause && running && reading)
                                usleep(10000);
                        FreezeDevice(Streams,0);
                };
 
                lastPTS=PTS;
                PTS=pkt.pts;

                RemuxAndQueue(pkt);

                if (PacketCount == 200)
                        dump_format(ic, 0, "test", 0);

                // update duration (FIXME does that help?)
                duration=ic->duration;
                int currPos= SoftDevice->GetSTC()*AV_TIME_BASE/(9*10000);
                if (duration<currPos)
                        duration=currPos;
        }
	//if (running) 
	//   DeviceFlush(20000);
	PLDBG("Before FlushDevice %d %d \n",running,newFile);
        if (running && !newFile) {
                FlushDevice(Streams);
        };

        ResetDevice(Streams);

};

void cSoftPlayer::Action() {
	PLDBG("Thread started: SoftPlayer\n");
	running=true;

	SoftDevice=cDevice::PrimaryDevice();
#if VDRVERSNUM >= 10330
        cPlugin *softdevicePlugin=cPluginManager::GetPlugin("softdevice");
              
        printf("softdevicePlugin %p\n",softdevicePlugin);
        if ( !softdevicePlugin ||
             !softdevicePlugin->Service(GET_PACKET_HANDEL_IDV100,
                       (void *) &SoftHandles) ) {
                printf("Didn't find the softdevice for output!!\n");
                exit(-1);
        };

 	//SoftDevice->SetPlayMode( (ePlayMode) pmAudioVideo );
        if (SoftplaySetup.UseReceiver()) {
                const cChannel *channel=Channels.GetByNumber(cDevice::CurrentChannel());
                Receiver=new cSoftplayReceiver(channel,
                                cDevice::PrimaryDevice(),SoftHandles);
#if VDRVERSNUM >= 10500 
                cDevice *receiveDev=cDevice::GetDevice(channel,1,false);
#else
                cDevice *receiveDev=cDevice::GetDevice(channel,1);
#endif
                if (receiveDev)
                        receiveDev->AttachReceiver(Receiver);
        };
#else
	
	if (!SoftDevice) {
	   printf("the Softdevice has to be primary device!!\n");
	   exit(-1);
	};
        // limitation in cPlayer: we have to know the play mode at
	// start time. Workaraound set play mode again and
	// put the softdevice in packet mode
 	SoftDevice->SetPlayMode( (ePlayMode) -pmAudioVideo );
        // Length = -1: set format context 
	SoftDevice->PlayVideo((uchar *) ic,-1);
#endif


	while ( running ) {
                if (newFile)
                        FileReplay();

                usleep(20000);
        };
        
        if (Receiver)
                delete Receiver;
        Receiver=NULL;
	
	DeviceClear();
        // force a softdevice reset
 	//SoftDevice->SetPlayMode( pmNone ); //FIXME find a different solution
	running=false;
	PLDBG("Thread beendet : SoftPlayer \n");
};

int cSoftPlayer::GetPlayMode(AVFormatContext *IC) {
	int i;
	bool hasVideo=false;
	bool hasAudio=false;
        
	PLDBG("GetPlayMode nb_streams %d\n",IC->nb_streams);
	for (i = 0; i<IC->nb_streams; i++) {
#if LIBAVFORMAT_BUILD > 4628
            PLDBG("GetPlayMode stream %d codec_type %d\n",
			i, IC->streams[i]->codec->codec_type);
#else
	    PLDBG("GetPlayMode stream %d codec_type %d\n",
			i, IC->streams[i]->codec.codec_type);
#endif

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

        i=0;
        if (hasVideo)
                i|=SOFTDEVICE_VIDEO_STREAM;
        if (hasAudio)
                i|=SOFTDEVICE_AUDIO_STREAM;
	return i;
};

bool IsStreamFile( const char * const Filename) {
        char * pos;
        pos = rindex(Filename,'.');
	printf("IsStream %s\n",pos);
        if ( !pos )
                return false;
        if ( !strcmp(pos,".stream") ) {
                return true;
        };

        return false;
};

void cSoftPlayer::OpenFile(const char *filename) {
        int ret;
	char StreamFilename[120];
	
	ic=0;
	if (!filename) {
                printf("Filname is NULL!!\n");
                ic=0;
                return;
        };
	
	if (IsStreamFile(filename)) {
		FILE *streamfile;
		printf("Found stream!\n");
		streamfile=fopen(filename,"r");
		if (!streamfile)
			return;
		fgets(StreamFilename,120,streamfile);
		chomp(StreamFilename);
		filename=StreamFilename;
		fclose(streamfile);
	};
		
        printf("open file '%s'\n",filename);
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
        Streams=GetPlayMode( ic );

        if (ic) {
                strlcpy(title, ic->title, sizeof(title));
                strlcpy(author, ic->author, sizeof(author));
                strlcpy(album, ic->album, sizeof(album));
                strlcpy(curr_filename, ic->filename, sizeof(curr_filename));
                                
                stripTrailingNonLetters(title);
                stripTrailingNonLetters(author);
                stripTrailingNonLetters(album);
                stripTrailingNonLetters(curr_filename);
                duration=ic->duration;
                start_time=ic->start_time;
        };
};

bool cSoftPlayer::GetIndex(int &Current, int &Total, bool SnapToIFrame ) {
	if (ic) {
		Current=(int) SoftDevice->GetSTC()/(9*10000)-start_time/AV_TIME_BASE;
		Total=duration/AV_TIME_BASE;
		//printf("duration %lld start_time %lld\n",duration,start_time);
                return true;
        } else {
                Current=0;
                Total=0;
                return false;
        };
        return false;
};
        
int cSoftPlayer::GetCurrPos() { 
        if (SoftDevice) 
                return SoftDevice->GetSTC()/(9*10000)-start_time/AV_TIME_BASE; 
        else return 0;
};

void cSoftPlayer::PlayFile(const char *file) {
        if (!file)
                return;

        newFile=true;
        strlcpy(filename,file,sizeof(filename));
};
// ---- Receiver controls ------------------------------

void cSoftPlayer::ChannelUpDown( int i) {
        if (!Receiver )
                return;
    

        i = i>0 ? +1 : -1 ;
       
        int currChannelNo=Receiver->currChannel.Number();
        const cChannel *channel = Channels.GetByNumber( currChannelNo + i, i );
        if (!channel) {
                printf("Didn't get channel!\n");
                return;
        };
        
        delete Receiver;
        Receiver=NULL;
               
#if VDRVERSNUM >= 10500 
        cDevice *receiveDev=cDevice::GetDevice(channel,1,false);
#else
        cDevice *receiveDev=cDevice::GetDevice(channel,1);
#endif

        if (!receiveDev) {
                printf("Didn't get device!\n");
                Skins.Message(mtError, tr("Channel not available!"));
                channel = Channels.GetByNumber( currChannelNo );
#if VDRVERSNUM >= 10500 
                receiveDev=cDevice::GetDevice(channel,1,false);
#else
                receiveDev=cDevice::GetDevice(channel,1);
#endif
                if (!receiveDev) {
                        printf("Jetzt bin ich angeschmiert\n");
                        return;
                };
        };
        receiveDev->SwitchChannel(channel,false);
        Receiver=new cSoftplayReceiver(channel,
                        cDevice::PrimaryDevice(),SoftHandles);
        if (!receiveDev->AttachReceiver(Receiver) ) {
                printf("Could not attach to device!\n");
        };
                
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
  Receiver=NULL;
/*
  const cChannel *channel=Channels.Get(cDevice::CurrentChannel());
  Receiver=new cSoftplayReceiver(channel,
                  cDevice::PrimaryDevice());
  cDevice *receiveDev=cDevice::GetDevice(channel,1);
  if (receiveDev)
          receiveDev->AttachReceiver(Receiver);
                  
  */                        
  SoftPlayer = dynamic_cast<cSoftPlayer*> (player);
  SoftPlayer->PlayFile(filename);
  newFile=true;
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
  const char *nextfile=PlayList->NextFile();
  if (nextfile)
	  SoftPlayer->PlayFile(nextfile);
  newFile=true;

  SendStatus();
};

cSoftControl::~cSoftControl() {
        PLDBG("~cSoftControl()\n");
#if VDRVERSNUM >= 10338
        cStatus::MsgReplaying(this,"SoftPlay","",false);
#else
        cStatus::MsgReplaying(this,NULL);
#endif
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

void cSoftControl::SendStatus() {
        char str[100];
        cIndex *Idx = FileIndex ? FileIndex->GetIndex(
                        SoftPlayer->GetFilename()) : NULL ;

        char loop = '.';
        char shuffle = '.'; 
        if (playList) {
                loop= playList->options.autoRepeat ? 'L' : '.' ;
                shuffle = playList->options.shuffle ? 'S' : '.';
        };

        if ( Idx && *Idx->GetTitle() ) {
                snprintf(str,sizeof(str),"[%c%c] (%d/%d) %s - %s",
                                loop,shuffle,
                                0,1,
                                Idx->GetTitle(),Idx->GetAuthor());
                
        } else snprintf(str,sizeof(str),"[%c%c] (%d/%d) %s",
                        loop,shuffle,
                        0,1,
                        SoftPlayer->GetFilename());
        
        //printf("send status: '%s'\n",str);
#if VDRVERSNUM >= 10338
        cStatus::MsgReplaying(this,
                        (Idx && Idx->GetTitle() ? Idx->GetTitle() : "SoftPlay")
                        ,SoftPlayer->GetFilename(),true);
#else
        cStatus::MsgReplaying(this,str);
#endif
}

void cSoftControl::ShowProgress() {
        if (!SoftPlayer)
                return;

	if ( OsdActive!=OsdProgress ) {
		OsdActive=OsdProgress;
                displayReplay=Skins.Current()->DisplayReplay(false);
                newFile=true;
        };
        
        int TotalDuration=SoftPlayer->GetDuration();
        int CurrentPos=SoftPlayer->GetCurrPos();

        if ( newFile || currFileDuration!=SoftPlayer->GetDuration()) {
                char Title[80];
                if ( *SoftPlayer->GetTitle() )
                        snprintf(Title,80,"%s - %s",SoftPlayer->GetTitle(),
                               SoftPlayer->GetAuthor());
                else {
                        const char *pos=SoftPlayer->GetFilename();
                        const char *tmp=rindex(pos,'/');
                        snprintf(Title,80,"%s",tmp?tmp+1:pos);
                };
                displayReplay->SetTitle(Title);
                currFileDuration=TotalDuration;
		
                displayReplay->SetProgress(CurrentPos,TotalDuration);
		char str[60];
		sprintf(str,"%02d:%02d:%02d",TotalDuration/3600,
			TotalDuration/60%60,TotalDuration%60);
		displayReplay->SetTotal(str);

		newFile=false;
        };
        displayReplay->SetProgress(CurrentPos,
                        SoftPlayer->GetDuration());
        char str[60];
        sprintf(str,"%02d:%02d:%02d",CurrentPos/3600,
                        CurrentPos/60%60,CurrentPos%60);
        displayReplay->SetCurrent(str);

        displayReplay->SetMode(SoftPlayer->GetPlay(),SoftPlayer->GetForward(),
                        SoftPlayer->GetSpeed());
};		

eOSState cSoftControl::ProcessKey(eKeys Key) {
	eOSState state = cOsdObject::ProcessKey(Key);

        if ( state != osUnknown && state != osContinue ) {
		PLDBG("cOsdObject::ProcessKey processed a key :-)\n");
                 return state;
        };

        // update file information in the playlist
        if ( newFile && FileIndex ) {
                cIndex *Idx=FileIndex->GetOrAddIndex(SoftPlayer->GetFilename());
                
                if ( SoftPlayer->GetDuration() &&
                     Idx && !Idx->GetDuration() )
                        Idx->SetDuration(SoftPlayer->GetDuration());
                
                if ( SoftPlayer->GetTitle() && *SoftPlayer->GetTitle()
                     && Idx && !*Idx->GetTitle() ) {
                        // set extended information
                        PLDBG("Set extended information: %s %s %s %d \n",
                             SoftPlayer->GetTitle(),SoftPlayer->GetAlbum(),
                             SoftPlayer->GetAuthor(),SoftPlayer->GetDuration());

                        Idx->SetTitle(SoftPlayer->GetTitle());
                        Idx->SetAuthor(SoftPlayer->GetAuthor());
                        Idx->SetAlbum(SoftPlayer->GetAlbum());
                };
                SendStatus();
               
        };

	if ( OsdActive == OsdProgress ) 
                ShowProgress();

	if ( !SoftPlayer->IsRunning()  ) {
		PLDBG("SoftPlayer not runnig. Looking for next file\n");
                const char * nextFile=0;
                if ( (!playList || !(nextFile=playList->NextFile()))
                      && SoftplaySetup.ReturnToLiveTV() ) {
                        PLDBG("No playlist or no next file. Ending.\n");
                        return osEnd;
                };
  		
                SoftPlayer->PlayFile(nextFile);
		newFile=true;
	};

        if ( OsdActive == OsdPrivMenu  && privateMenu) {
                state = privateMenu->ProcessKey(Key);
                if (state == PLAY_CURR_FILE ) {
                        const char * nextFile;
                        if (!playList || !(nextFile=playList->CurrFile())) {
                                PLDBG("No playlist or no curr file. Ending.\n");
                                return osEnd;
                        };

                        SoftPlayer->PlayFile(nextFile);
                        newFile=true;
                };
                if (state == osEnd || state == osBack) {
                        PLDBG("private menu osEnd\n");
                        delete privateMenu;
                        privateMenu=NULL;
                        OsdActive=OsdNone;
                        if ( state == osEnd && SoftPlayer ) {
                                SoftPlayer->Stop();
                                return osEnd;
                        };
                        return osContinue;
                };
                
		if ( state != osUnknown )
                        return state;
        };


	if(state!=osUnknown) 
		return state;

	state = osContinue;
	switch (Key) {
                // Positioning:
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
		case kStop:
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
                case kFastFwd:
                case kRight:
			     //SoftPlayer->FastForward();  
			     break;
                case kFastRew:
		case kLeft:
			     //SoftPlayer->FastBackward();  
			     break;
		case kOk: if (OsdActive==OsdProgress)
				  Hide();
			else ShowProgress(); 
			break;

                        // playlist menus
                case kRed:
		case k8:   
			if (playList) {
				Hide();
				OsdActive=OsdPrivMenu;
				privateMenu=new cReplayList(playList);
				privateMenu->Display();
				return osContinue;
			};
			break;
		case k5:   
			if (playList) {
				Hide();
				OsdActive=OsdPrivMenu;
				privateMenu=new cAlbumList(playList);
				privateMenu->Display();
				return osContinue;
			};
			break;
                        
                        // track skips
		case k9: if (playList) {
				 const char * nextFile=playList->NextFile();
                                 printf("nextfile %p ReturnToLiveTv() %d\n",
                                                 nextFile,SoftplaySetup.ReturnToLiveTV());
				 if ( nextFile 
                                      || !SoftplaySetup.ReturnToLiveTV() )
					 SoftPlayer->PlayFile(nextFile);
                                 else {  // last file
                                         SoftPlayer->Stop();
                                         shouldStop=true;  
                                         return osEnd;
                                 };
                                 newFile=true;
			 };
			 break;
		case k7: if (playList) {
				 const char * prevFile=playList->PrevFile();
				 printf("play PrevFile %p\n",prevFile);
				 if (prevFile)
					 SoftPlayer->PlayFile(prevFile);
                                 newFile=true;
			 };
			 break;
                         
                         // album skips
		case k6: if (playList) {
				 const char * nextFile=playList->NextAlbumFile();
				 if ( nextFile
                                      || !SoftplaySetup.ReturnToLiveTV() )
					 SoftPlayer->PlayFile(nextFile);
                                 else {  // last file
                                         SoftPlayer->Stop();
                                         shouldStop=true;  
                                         return osEnd;
                                 };
                                 newFile=true;
                         };
			 break;
		case k4: if (playList) {
				 const char * prevFile=playList->PrevAlbumFile();
				 if (prevFile)
					 SoftPlayer->PlayFile(prevFile);
                                 newFile=true;
			 };
			 break;

			 // receiver
                case k3:
                         if (SoftPlayer->Receiver)
                                 SoftPlayer->ChannelUpDown(+1);
                         break;
                case k1:
                         if (SoftPlayer->Receiver)
                                 SoftPlayer->ChannelUpDown(-1);
                         break;

		case kBack:	
                         Hide();
                         OsdActive=OsdPrivMenu;
                         privateMenu=
                                 cMenuDirectory::OpenLastBrowsedDir();
                         //privateMenu->Display();
                         return osContinue;
			break;

			 
		default:
			 break;
	};
	return state;
};


