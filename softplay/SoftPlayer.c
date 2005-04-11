/*
 * Media Player plugin for VDR
 *
 * Copyright (C) 2005 Martin Wache
 *
 * This code is distributed under the terms and conditions of the
 * GNU GENERAL PUBLIC LICENSE. See the file COPYING for details.
 *
 * $ Id: $
 */

#include "SoftPlayer.h"

#define PLDBG(out...) {printf(out);}

#ifndef PLDBG
#define PLDBG(out...)
#endif

// -------------------cSoftPlayer---------------------------------------
cSoftPlayer::cSoftPlayer() : cPlayer() {
	running=false;
	skip=0;
};

cSoftPlayer::~cSoftPlayer() {
	PLDBG("~cSoftPlayer \n");fflush(stdout);	
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
	cSoftDevice *SoftDevice;
  	AVPacket  pkt;
  	int ret;
  	int nStreams=0;
	int PacketCount=0;
	static int64_t lastPTS=0;

	PLDBG("Thread started: SoftPlayer\n");
	running=true;
	reading=true;
	pollTimeouts=0;

	if (!ic) {
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

		// length = -2 : queue packet
		SoftDevice->PlayVideo((uchar *)&pkt,-2);
		//SoftDevice->QueuePacket(ic,pkt);
		
		if (nStreams!=ic->nb_streams ){
			PacketCount=0;
			nStreams=ic->nb_streams;
			fprintf(stderr,"Streams: %d\n",nStreams);
			for (int i=0; i <nStreams; i++ ) {
				printf("Codec %d ID: %d\n",i,ic->streams[i]->codec.codec_id);
			};
		};

		if (PacketCount % 100) {
			usleep(100);
		};

		if (PacketCount == 200)
			dump_format(ic, 0, "test", 0);
	}
	if (running) 
	   DeviceFlush(20000);
	
	DeviceClear();
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
	printf("open file %s\n",filename);
	if ( av_open_input_file( &ic, filename, NULL, 0, &ap) ) {
		printf("could not open file\n");
		ic=0;
		return;
	};

	if ( av_find_stream_info( ic ) ) {
		printf("could not find stream info\n");
		//exit(-1);
	};
	softPlayMode=GetPlayMode( ic );
}; 

// -------------------cSoftControl----------------------------------------

cSoftControl::cSoftControl(const char * filename ) : cControl(new cSoftPlayer) 
{
  SoftPlayer = dynamic_cast<cSoftPlayer*> (player);
  SoftPlayer->OpenFile(filename);
};

cSoftControl::~cSoftControl() {
	PLDBG("~cSoftControl()\n");fflush(stdout);
	delete player;
	SoftPlayer = NULL;
	player = NULL;
};

eOSState cSoftControl::ProcessKey(eKeys Key) {
	eOSState state = cOsdObject::ProcessKey(Key);

	if ( state != osUnknown) {
		PLDBG("cOsdObject::ProcessKey processed a key :-)\n");
		return state;
	};

	if ( !SoftPlayer->IsRunning() ) {
		PLDBG("SoftPlayer not runnig. Ending\n");
		return osEnd;
	};
	
	if(state==osUnknown) {
		state = osContinue;

		switch (Key) {
			// Positioning:
			//case kRed:     TimeSearch(); break;
			case kGreen|k_Repeat:
			case kGreen:   SoftPlayer->SkipSeconds(-60); break;
			case kYellow|k_Repeat:
			case kYellow:  SoftPlayer->SkipSeconds( 60); break;
			case kBlue:    SoftPlayer->Stop();  break;
			default:
			   break;
		};
	};
	return osContinue;
};


