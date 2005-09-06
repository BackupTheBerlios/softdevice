/*
 * mpeg2decoder.c: A plugin for the Video Disk Recorder
 *
 * See the README file for copyright information and how to reach the author.
 *
 * $Id: mpeg2decoder.c,v 1.53 2005/09/06 21:55:27 lucke Exp $
 */

#include <math.h>
#include <sched.h>

#include <vdr/plugin.h>

#include "mpeg2decoder.h"
#include "audio.h"
#include "utils.h"
#include "setup-softdevice.h"


//#define MPGDEB(out...) {printf("mpegdec[%04d]:",(int)(getTimeMilis() % 10000));printf(out);}

#ifndef MPGDEB
#define MPGDEB(out...)
#endif

#define CMDDEB(out...) {printf("CMD[%04d]:",(int)(getTimeMilis() % 10000));printf(out);}

#ifndef CMDDEB
#define CMDDEB(out...)
#endif

//#define BUFDEB(out...) {printf("BUF[%04d]:",(int)(getTimeMilis() % 10000));printf(out);}

#ifndef BUFDEB
#define BUFDEB(out...)
#endif

// 0: save buffers, 1: good seeking, 2: HDTV buffers
int dvb_buf_size[] = {64*1024,32*1024,64*1024};
int packet_buf_size[] = {300,150,2000};

//#define AV_STATS
//------------------------------------cPacketBuffer------------------------

cPacketQueue::cPacketQueue(int maxPackets) {
  MaxPackets=maxPackets;
  FirstPacket=LastPacket=0;
  queue=new AVPacket[MaxPackets];
  memset(queue,0,sizeof(queue));
};

cPacketQueue::~cPacketQueue() {
  Clear();
  delete queue;
};

int cPacketQueue::PutPacket(const AVPacket &Packet) {
  if (Available()>MaxPackets*2/3) {
      BUFDEB("PacketQueue.EnableGet.Signal\n");
      EnableGet.Signal();
  };
  
  if (FirstPacket == Next(LastPacket) ) {
        BUFDEB("PacketQueue.EnablePut.Sleep start\n");
        EnablePut.Sleep(50000);
        BUFDEB("PacketQueue.EnablePut.Sleep stop\n");
  };
  
  if (FirstPacket != Next(LastPacket) ) {
    queue[LastPacket]=Packet;
    LastPacket=Next(LastPacket);
//    printf("PutPacket %x FirstPacket %d LastPacket %d\n",
//      queue,FirstPacket,LastPacket);
    return 0;
  } else return -1;
};

AVPacket * cPacketQueue::GetReadPacket() {
//  printf("GetReadPacket %x FirstPacket %d LastPacket %d\n",
//    queue,FirstPacket,LastPacket);
  if (Available()==0)  {
      BUFDEB("PacketQueue.EnablePut.Signal pid: %d\n",getpid());
      EnablePut.Signal();
  };
  
  if (FirstPacket==LastPacket) {
       BUFDEB("PacketQueue.EnableGet.Sleep start pid: %d\n",getpid());
       EnableGet.Sleep(10000);  
       BUFDEB("PacketQueue.EnableGet.Sleep stop pid: %d \n",getpid());
  };
  
  if ( FirstPacket != LastPacket )
    return &queue[FirstPacket];

  return NULL;
};

void cPacketQueue::FreeReadPacket(AVPacket *Packet) {
  if (&queue[FirstPacket] == Packet ) 
     FirstPacket=Next(FirstPacket);
  else {
    fprintf(stderr,"Serious problem in FreeReadPacket! Wrong Packet, exiting!\n");
    exit(-1);
  };
};

void cPacketQueue::Clear() {
  while (FirstPacket != LastPacket ) {
     av_free_packet( &queue[FirstPacket] );
     FirstPacket=Next(FirstPacket);
  };
};

//-------------------cClock ------------------------------------------

int64_t cClock::audioOffset=0;
int64_t cClock::audioPTS=0;
int64_t cClock::videoOffset=0;
int64_t cClock::videoPTS=0;
bool cClock::freezeMode=true;

int64_t  cClock::GetPTS() {
  //MPGDEB("audioOffset %lld time %lld\n",audioOffset,GetTime());
  if ( audioOffset )
     return freezeMode ? audioPTS : GetTime()+audioOffset;
  else if ( videoOffset )
     return freezeMode ? videoPTS : GetTime()+videoOffset;
  else return 0;
};

// --- cStreamDecoder ---------------------------------------------------------

cStreamDecoder::cStreamDecoder(AVCodecContext *Context)
        : PacketQueue(packet_buf_size[setupStore.bufferMode])
{
  context=Context;
  if (context)
        context->error_resilience=1;
  CMDDEB("Neuer StreamDecoder Pid: %d context %p type %d\n",
        getpid(),context,context->codec_type );
  pts=0;
  frame=0;
  initCodec();
  syncTimer=NULL;

//  Context->debug |=FF_DEBUG_STARTCODE;
  active=true;
  Start(); // starte thread
}

cStreamDecoder::~cStreamDecoder()
{
  CMDDEB("~cStreamDecoder: context %p\n",context );
  active=false;
  Cancel(3);
  if (codec && context)  
    avcodec_close(context);
  else fprintf(stderr,"Error not closing context %p, codec %p\n",context,codec);
  
  PacketQueue.Clear();
}

void cStreamDecoder::Action()
{
  CMDDEB("Neuer Thread gestartet: pid:%d type %d\n",getpid(),context->codec_type );
  running=true;
  freezeMode=false;
  AVPacket *pkt;

  while ( PacketQueue.Available() < 7 && active) { 
    BUFDEB("wait while loop packets %d StreamDecoder  pid:%d type %d\n",
      PacketQueue.Available(),getpid(),context->codec_type );
    usleep(10000);
  };
 
  while(active)
  {
    BUFDEB("while loop start StreamDecoder  pid:%d type %d\n",getpid(),context->codec_type );

    while (freezeMode && active)
        usleep(50000);

    mutex.Lock();
    if ( (pkt=PacketQueue.GetReadPacket())!= 0 ) {
       BUFDEB("got packet PTS: %lld pid: %d type %d \n",
         pkt->pts,getpid(),context->codec_type);
       if (codec && context) 
         DecodePacket(pkt);
       av_free_packet(pkt);
       PacketQueue.FreeReadPacket(pkt);
       BUFDEB("finished packet pid: %d type %d \n",getpid(),context->codec_type);
       mutex.Unlock();
    } else {
      mutex.Unlock();
      usleep(10000);
    };
#if 0 
    {
      static int count=0;
      count++;
      if (!(count % 10)) {
        printf("Type: %d Buffer fill: %d\n",
          context->codec_type,PacketQueue.Available());
        count=0;
        usleep(10000);
      };
    };
#endif

  }
  running=false;
  CMDDEB("thread finished pid: %d type %d \n",getpid(),context->codec_type );
}

void cStreamDecoder::Play(void)
{
  freezeMode=false;
  if (syncTimer)
    syncTimer->Signal();
};

void cStreamDecoder::Freeze(void)
{
  //printf("freeze mode\n");
  freezeMode=true;
};

void cStreamDecoder::Stop(void)
{
  active=false;
  if (syncTimer)
    syncTimer->Signal();
  mutex.Lock();
  PacketQueue.Clear();
  mutex.Unlock();
  PacketQueue.EnablePutSignal();
  Cancel(3);
}

void cStreamDecoder::Clear(void)
{
  CMDDEB("cStreamDecoder clear type: %d\n",context->codec_type);

  // stop playback bevore clear
  bool oldfreezeMode=freezeMode;
  Freeze();
  
  mutex.Lock();
  if (codec && context)
    avcodec_flush_buffers(context);
  PacketQueue.Clear();
  mutex.Unlock();

  // restore old freezeMode
  freezeMode=oldfreezeMode;
  
  CMDDEB("cStreamDecoder clear finished type: %d\n",context->codec_type);
}

bool cStreamDecoder::initCodec(void)
{
  int ret;
  if (!context)
    return false;

  codec = avcodec_find_decoder(context->codec_id);
  if (!codec)
  {
    printf("[mpegdecoder] Error! Codec %d not supported by libavcodec\n",
      context->codec_id);
    return false;
  }
  
    /* we don't send complete frames */
  if(codec->capabilities&CODEC_CAP_TRUNCATED)
    context->flags|= CODEC_FLAG_TRUNCATED; 
  
  if ( (ret=avcodec_open(context, codec)) < 0)
  {
    printf("[mpegdecoder] Error! Could not open codec %d Error: %d\n",
      context->codec_id,ret);
    codec=NULL;
    return false;
  }
  MPGDEB("Codec %d initialized.\n",context->codec_id);
  return true;
};

void cStreamDecoder::resetCodec(void)
{
  if (!context || !codec) {
    fprintf(stderr,"Error: not reseting codec context %p codec %p\n",context,codec);
    return;
  };
  printf("[mpegdecoder] resetting codec\n");
  avcodec_close(context);
  initCodec();
}

int cStreamDecoder::BufferFill()
{
  return ( PacketQueue.Available() ) *100/PacketQueue.GetMaxPackets();
}

// --- AUDIO ------------------------------------------------------------------
cAudioStreamDecoder::cAudioStreamDecoder(AVCodecContext *Context,
                                         cAudioOut *AudioOut, int AudioMode)
                                         : cStreamDecoder(Context)
{
  audioOut=AudioOut;
  audioMode=AudioMode;
  audiosamples=(uint8_t *)malloc(AVCODEC_MAX_AUDIO_FRAME_SIZE);
  
  audioOutContext.period_size=2*1024;
  audioOutContext.samplerate=44100;
  audioOutContext.channels=2;
}

/* ---------------------------------------------------------------------------
 */
uint64_t cAudioStreamDecoder::GetPTS()
{
  MPGDEB("pts %lld aDelay: %d ",pts,audioOut->GetDelay());
  uint64_t PTS= pts - audioOut->GetDelay() + setupStore.avOffset*10;
 return PTS;
};

void cAudioStreamDecoder::OnlyLeft(uint8_t *samples,int Length) {
  for (int i=0; i < Length/2; i+=2)
     ((uint16_t*)samples)[i+1]=((uint16_t*)samples)[i];
};

void cAudioStreamDecoder::OnlyRight(uint8_t *samples,int Length) {
  for (int i=0; i < Length/2; i+=2)
    ((uint16_t*)samples)[i]=((uint16_t*)samples)[i+1];
};

/* ---------------------------------------------------------------------------
 */

int cAudioStreamDecoder::DecodePacket(AVPacket *pkt) {
    int     len=0;
    int     audio_size=0;
    uint8_t *data=pkt->data;
    int     size=pkt->size;

  if (context->codec_id == CODEC_ID_AC3)
  {
    switch(setupStore.ac3Mode)
    {
      case 0:
        // get the AC3 -> 2CH stereo data
        context->channels = 2;
        break;
      case 1:
        // feed data for AC3 pass through to device
        audioOut->WriteAC3(data,size);
        return size;
      case 2:
        // get the AC3 -> 4CH stereo data
        context->channels = 4;
        break;
      case 3:
        // set channels to auto mode to get decoded stream for analog out
        context->channels = 0;
        break;
    }
  }
  while ( size > 0 && active ) {
    BUFDEB("start decode audio. pkt size: %d \n",size);
    len=avcodec_decode_audio(context, (short *)audiosamples,
                 &audio_size, data, size);
    BUFDEB("end decode audio\n");
    if (len < 0) {
      printf("[mpegdecoder] Error while decoding audio frame %d\n", frame);
      resetCodec();
      return len;
    }
    size-=len;
    data+=len;

    MPGDEB("audio: count: %d  Length: %d len: %d a_size: %d a_delay: %d\n",
       frame,pkt->size,len, audio_size, audioOut->GetDelay());

    // no new frame decoded, continue
    if (audio_size <= 0)
      continue;

    if ( audio_size > AVCODEC_MAX_AUDIO_FRAME_SIZE ) {
        fprintf(stderr,"Error audio_size greater than MAX_AUDIO_FRAME_SIZE!\n");
        continue;
    };
    
    audioOutContext.channels=context->channels;
    audioOutContext.samplerate=context->sample_rate;
    
    if (audioMode && audioOutContext.channels==2) {
      // respect mono left/right only channels
      if (audioMode==1)
        OnlyLeft(audiosamples,audio_size);
      else OnlyRight(audiosamples,audio_size);
    };
    
    audioOut->SetParams(audioOutContext);
    audioOut->Write(audiosamples,audio_size);
    // adjust PTS according to audio_size, sampel_rate and no. of channels  
    pts += (audio_size*10000/(context->sample_rate*2*context->channels)); 

    if (pkt->pts != (int64_t) AV_NOPTS_VALUE) {
      MPGDEB("audio pts %lld pkt->PTS : %lld pts - valid PTS: %lld \n",
         pts,pkt->pts,(int) pts - pkt->pts );
      pts = pkt->pts;
      pkt->pts=AV_NOPTS_VALUE;
    }
    uint64_t PTS= pts - audioOut->GetDelay() + setupStore.avOffset*10;

    MPGDEB("audio pts offset %lld\n",cClock::GetTime()-PTS);
    cClock::AdjustAudioPTS(PTS);
    frame++;
  }

  MPGDEB("a_delay %d at end of decode\n",audioOut->GetDelay());

  return len;
}

cAudioStreamDecoder::~cAudioStreamDecoder()
{
  cClock::AdjustAudioPTS(0);
  Cancel(3);
  free(audiosamples);
}

// --- VIDEO ------------------------------------------------------------------

cVideoStreamDecoder::cVideoStreamDecoder(AVCodecContext *Context,
                                         cVideoOut *VideoOut, cClock *Clock,
                                         int Trickspeed)
                                         : cStreamDecoder(Context)
{
  width = height = -1;
  pix_fmt = PIX_FMT_NB;
  pic_buf_lavc = pic_buf_mirror = pic_buf_pp = pic_buf_convert = NULL;
  currentMirrorMode  = setupStore.mirror;
  currentDeintMethod = setupStore.deintMethod;
  currentppMethod = setupStore.ppMethod;
  currentppQuality = setupStore.ppQuality;

  memset(pts_values,-1,sizeof(pts_values));
  lastPTSidx=lastPTS=0;
  lastCodedPictNo=-1;
#ifdef PP_LIBAVCODEC
  ppmode = ppcontext = NULL;
#endif //PP_LIBAVCODEC
  videoOut=VideoOut;
  clock = Clock;

  // init A-V syncing variables
  offset=0;
  delay=0;
  hurry_up=0;
  syncTimer = new cSyncTimer (emRtcTimer);
  syncTimer->Reset();

  default_frametime = DEFAULT_FRAMETIME;
  trickspeed = Trickspeed;

  picture=avcodec_alloc_frame();
}

void cVideoStreamDecoder::Play(void)
{
  cStreamDecoder::Play();
  videoOut->FreezeMode(freezeMode);
};

void cVideoStreamDecoder::Freeze(void)
{
  cStreamDecoder::Freeze();
  videoOut->FreezeMode(freezeMode);
};

void cVideoStreamDecoder::Stop(void)
{
  active=false;
  syncTimer->Signal(); // abort waiting for frame display
  Cancel(3);
}

uint64_t cVideoStreamDecoder::GetPTS() {
  return pts - (delay + syncTimer->TimePassed())/100;
}

int cVideoStreamDecoder::DecodePacket(AVPacket *pkt)
{
  int len=0;
  int got_picture=0;

  uint8_t *data=pkt->data;
  int size=pkt->size;
  //MPGDEB("got video packet\n");
  while ( size > 0 ) {
    if ( context->width  > 2048 || context->height  > 2048 ) {
      fprintf(stderr,"Invalid width (%d) or height (%d), most likely a broken stream\n",
          context->width,context->height);
      return len;
    };

    BUFDEB("start decode video stream %d data: %p size: %d \n",
        pkt->stream_index,data,size);
    len = avcodec_decode_video(context, picture, &got_picture,data, size);
    BUFDEB("end decode video\n");
    if (len < 0) {
      printf("[mpegdecoder] Error while decoding video frame %d\n", frame);
      resetCodec();
      return len;
    }
    size-=len;
    data+=len;
 
    // save coded picture number together with corresponding pts
    if (context->coded_frame  &&
        context->coded_frame->coded_picture_number!=lastCodedPictNo
        && lastPTS != (int64_t) AV_NOPTS_VALUE ) {
      pts_values[lastPTSidx].pts = lastPTS;
      pts_values[lastPTSidx].duration = lastDuration;
      pts_values[lastPTSidx].coded_frame_no = 
        context->coded_frame->coded_picture_number;
      lastPTSidx = (lastPTSidx+1)%NO_PTS_VALUES;
      lastCodedPictNo = context->coded_frame->coded_picture_number;
      MPGDEB("Got pts: pictno %d  pts: %lld lastIdx %d \n",
        context->coded_frame->coded_picture_number,
        lastPTS,
        lastPTSidx);
      lastPTS = AV_NOPTS_VALUE;
    };
    
    if (pkt->pts != (int64_t) AV_NOPTS_VALUE) {
         lastPTS=pkt->pts;
         lastDuration=pkt->duration;
         
         if (lastDuration) {
#if LIBAVCODEC_BUILD > 4753
                 default_frametime=context->time_base.num*
                         10000/context->time_base.den;
#else
                 default_frametime=lastDuration/100;
#endif
                 if (!default_frametime) {
                  /* ----------------------------------------------------------
                   * we should have another/better guess.
                   */
                  //fprintf (stderr, " --- setting default frametime !! ---\n");
                  default_frametime = DEFAULT_FRAMETIME;
                 }
                 MPGDEB("Set default_frametime to %d\n",default_frametime);
         };
    };

    if (!got_picture)
      continue;

    // postproc stuff....
    if (setupStore.mirror == 1)
      Mirror();

    if (setupStore.deintMethod == 1)
      deintLibavcodec();
#ifdef PP_LIBAVCODEC
#ifdef FB_SUPPORT
    if (setupStore.deintMethod > 2 || setupStore.ppMethod!=0 )
#else
    if (setupStore.deintMethod > 1 || setupStore.ppMethod!=0 )
#endif //FB_SUPPORT
      ppLibavcodec();
#endif //PP_LIBAVCODEC

    // do format conversions if necessary
    if (context->pix_fmt!=PIX_FMT_YUV420P) 
      libavcodec_img_convert();

    width  = context->width;
    height = context->height;
    pix_fmt = context->pix_fmt;

   
    // find decoded pictures pts value
    int findPTS=(lastPTSidx+1)%NO_PTS_VALUES;
    while ( picture->coded_picture_number !=
                pts_values[findPTS].coded_frame_no && 
                findPTS != lastPTSidx) 
         findPTS=(findPTS+1)%NO_PTS_VALUES;
   
     // found corresponding pts value
     if (picture->coded_picture_number==pts_values[findPTS].coded_frame_no) {
          MPGDEB("video pts: %lld, values.pkt : %lld pts - valid PTS: %lld pictno: %d idx: %d\n",
          pts,pts_values[findPTS].pts,(int) pts - pts_values[findPTS].pts,
          picture->coded_picture_number,findPTS);
          pts = pts_values[findPTS].pts;
     };
  
  if (!hurry_up || frame % 2 ) {
    // prepare picture for display
    videoOut->CheckAspectDimensions(picture,context);
    videoOut->SetOldPicture(picture,context->width,context->height);

    // sleep ....
    delay-=syncTimer->GetRelTime();
    MPGDEB("Frame# %-5d  aPTS: %lld offset: %d delay %d \n",frame,clock->GetPTS(),offset,delay );

    videoOut->Sync(syncTimer, &delay);
    // display picture
    videoOut->YUV(picture->data[0], picture->data[1],picture->data[2],
        context->width,context->height,
        picture->linesize[0],picture->linesize[1]);
  } else fprintf(stderr,"+");
  // we just displayed a frame, now it's the right time to
  // measure the A-V offset
  // the A-V syncing code is partly based on MPlayer...
  uint64_t aPTS = clock->GetPTS();
  // update video pts
  cClock::AdjustVideoPTS(pts);

  if ( aPTS )
    offset = aPTS - pts ;
  else offset = 0;

  // this few lines does the whole syncing
  int pts_corr;

  // calculate pts correction. Correct 1/10 of offset at a time
  pts_corr = offset/10;

  //Max. correction is 2/10 frametime.
  if (pts_corr > 2*frametime() / 10 )
    pts_corr = 2*frametime() / 10;
  else if (pts_corr < -2*frametime() / 10 )
    pts_corr = -2*frametime() / 10;

  // calculate delay
  delay += ( frametime() - pts_corr  ) * 100;
  // update video pts 
  pts += frametime();
 
  if (delay > 2*frametime()*100)
    delay = 2*frametime()*100;
  else if (delay < -frametime()*100)
    delay = -frametime()*100;

  if (offset >  8*frametime())
     hurry_up=1;
  else if ( (offset < 2*frametime()) && (hurry_up > 0) )
     hurry_up=0;

#if 1
  int dispTime=syncTimer->GetRelTime();
  delay-=dispTime;
  if (!(frame % 1) || context->hurry_up) {
    MPGDEB("Frame# %-5d A-V(ms) %-5d delay %d FrameT: %s, dispTime(ms): %1.2f\n",
      frame,(int)(clock->GetPTS()-pts),delay,
      (picture->pict_type == FF_I_TYPE ? "I_t":
      (picture->pict_type == FF_P_TYPE ? "P_t":
      (picture->pict_type == FF_B_TYPE ? "B_t":"other"
      )))
      ,(float)dispTime/1000 );
  }
#endif

#ifdef AV_STATS
  {
    const int Freq=10;
    static float offsetSum=0;
    static float offsetSqSum=0;
    static float DispTSum=0;
    static float DispTSqSum=0;
    static int StatCount=0;

    offsetSum+=(float)offset/10.0;
    offsetSqSum+=(float) offset*offset/100.0;
    DispTSum+=(float) dispTime;
    DispTSqSum+=(float) dispTime*dispTime;
    StatCount+=1;

    if ( (StatCount % Freq) == 0 )
    {
      printf("A-V: %02.2f+-%02.2f ms DispTime: %02.2f ms hurry_up: %d\n",
       offsetSum/(float)Freq,
       sqrt(offsetSqSum/((float)Freq)-(offsetSum*offsetSum)/((float)Freq*Freq)),
       DispTSum/(float)Freq/1000,hurry_up);
      offsetSum=offsetSqSum=DispTSum=DispTSqSum=0;
      StatCount=0;
    };
  };
#endif

  frame++;
  };
  return len;
}

void cVideoStreamDecoder::TrickSpeed(int Speed)
{
  trickspeed = Speed;
  syncTimer->Signal();
}

//------------------------------------ postproc stuff -------------------------
uchar *cVideoStreamDecoder::allocatePicBuf(uchar *pic_buf, PixelFormat pix_fmt)
{
  // (re)allocate picture buffer for deinterlaced/mirrored picture
  if (pic_buf == NULL)
    fprintf(stderr,
            "[softdevice] allocating picture buffer for resolution %dx%d "
	    "format %d\n",
            context->width, context->height, pix_fmt);
  else
    fprintf(stderr,
            "[softdevice] resolution changed to %dx%d, format %d - "
            "reallocating picture buffer\n",
            context->width, context->height, pix_fmt);

  pic_buf = (uchar *) realloc(pic_buf,
      avpicture_get_size(pix_fmt, context->width, context->height));

  return pic_buf;
}

uchar *cVideoStreamDecoder::freePicBuf(uchar *pic_buf)
{
  if (pic_buf)
  {
    free (pic_buf);
    fprintf(stderr,"[softdevice] picture buffer released\n");
  }

  return NULL;
}

void cVideoStreamDecoder::deintLibavcodec(void)
{
  if (pic_buf_lavc == NULL ||
      context->width != width ||
      context->height != height ||
      context->pix_fmt != pix_fmt)
    pic_buf_lavc = allocatePicBuf(pic_buf_lavc,context->pix_fmt);

  if ( !pic_buf_lavc ) {
    fprintf(stderr,
            "[softdevice] no picture buffer is allocated for deinterlacing !\n"
            "[softdevice] switching deinterlacing off !\n");
    setupStore.deintMethod = 0;
    return;
  }
  
  avpicture_fill(&avpic_dest,pic_buf_lavc,
                    context->pix_fmt,context->width ,context->height);

  memcpy(avpic_src.data,picture->data,sizeof(avpic_src.data));
  memcpy(avpic_src.linesize,picture->linesize,sizeof(avpic_src.linesize));

  if (avpicture_deinterlace(&avpic_dest, &avpic_src, context->pix_fmt,
                              context->width, context->height) < 0) 
  {
    fprintf(stderr,
            "[softdevice] error, libavcodec deinterlacer failure\n"
            "[softdevice] switching deinterlacing off !\n");
    setupStore.deintMethod = 0;
    return;
  } 
    
  memcpy(picture->data,avpic_dest.data,sizeof(picture->data));
  memcpy(picture->linesize,avpic_dest.linesize,sizeof(picture->data));
}

void cVideoStreamDecoder::libavcodec_img_convert(void)
{
  if (pic_buf_convert == NULL ||
      context->width != width ||
      context->height != height) {
    pic_buf_convert = allocatePicBuf(pic_buf_convert,PIX_FMT_YUV420P);
    fprintf(stderr,"allocated convert buf\n");
  }

  if ( !pic_buf_convert ) {
     fprintf(stderr,
            "[softdevice] no picture buffer is allocated for img_convert !\n"
            "[softdevice] switching img_convert off !\n");
     return;
  }
 
  avpicture_fill(&avpic_dest,pic_buf_convert,
                    PIX_FMT_YUV420P,context->width ,context->height);

  memcpy(avpic_src.data,picture->data,sizeof(avpic_src.data));
  memcpy(avpic_src.linesize,picture->linesize,sizeof(avpic_src.linesize));
  
  if (img_convert(&avpic_dest,PIX_FMT_YUV420P,
		  &avpic_src, context->pix_fmt,
                              context->width, context->height) < 0) {
     fprintf(stderr,
              "[softdevice] error, libavcodec img_convert failure\n");
     return;
  } 

  memcpy(picture->data,avpic_dest.data,sizeof(picture->data));
  memcpy(picture->linesize,avpic_dest.linesize,sizeof(picture->data));
}

void cVideoStreamDecoder::Mirror(void)
{
    uchar *ptr_src1, *ptr_src2;
    uchar *ptr_dest1, *ptr_dest2;

  if (pic_buf_mirror == NULL ||
      context->width != width ||
      context->height != height ||
      context->pix_fmt != pix_fmt)
    pic_buf_mirror = allocatePicBuf(pic_buf_mirror,context->pix_fmt);

  if ( !pic_buf_mirror ) {
    fprintf(stderr,
            "[softdevice] no picture buffer is allocated for mirroring !\n"
            "[softdevice] switching mirroring off !\n");
    setupStore.mirror = 0;
    return;
  }

  avpicture_fill(&avpic_dest,pic_buf_mirror,
      context->pix_fmt,context->width ,context->height);

  // mirror luminance
  ptr_src1  = picture->data[0];
  ptr_dest1 = avpic_dest.data[0];

  for (int h = 0; h < context->height; h++)
  {
    for (int w = context->width; w > 0; w--)
    {
      *ptr_dest1 = ptr_src1[h * picture->linesize[0] + w - 1];
      ptr_dest1++;
    }
  }

  // mirror chrominance
  ptr_src1  = picture->data[1];
  ptr_src2  = picture->data[2];
  ptr_dest1 = avpic_dest.data[1];
  ptr_dest2 = avpic_dest.data[2];

  int h_shift;
  int v_shift;
  avcodec_get_chroma_sub_sample(context->pix_fmt,&h_shift,&v_shift);

  for (int h = 0; h < context->height >> v_shift ; h++)
  {
    for (int w = context->width >> h_shift; w > 0; w--)
    {
      *ptr_dest1 = ptr_src1[h * picture->linesize[1] + w - 1];
      *ptr_dest2 = ptr_src2[h * picture->linesize[2] + w - 1];
      ptr_dest1++;
      ptr_dest2++;
    }
  }

  picture->data[0]     = avpic_dest.data[0];
  picture->data[1]     = avpic_dest.data[1];
  picture->data[2]     = avpic_dest.data[2];

  picture->linesize[0] = context->width;
  picture->linesize[1] = context->width >> h_shift ;
  picture->linesize[2] = context->width >> h_shift ;
}

#ifdef PP_LIBAVCODEC
void cVideoStreamDecoder::ppLibavcodec(void)
{
    int deintWork;

  if (pic_buf_pp == NULL ||
      context->width != width ||
      context->height != height ||
      context->pix_fmt != pix_fmt)
    pic_buf_pp = allocatePicBuf(pic_buf_pp,context->pix_fmt);

  if (ppcontext == NULL ||
      context->width != width ||
      context->height != height||
      context->pix_fmt != pix_fmt) {
          // reallocate ppcontext if format or size of picture changed
    if (ppcontext)
    {
      pp_free_context(ppcontext);
      ppcontext = NULL;
    }
    /* set one of this values instead of 0 in pp_get_context for
     * processor-independent optimations:
       PP_CPU_CAPS_MMX, PP_CPU_CAPS_MMX2, PP_CPU_CAPS_3DNOW
     */
    int flags=0;
#ifdef USE_MMX
    flags|=PP_CPU_CAPS_MMX;
#endif
#ifdef USE_MMX2
    flags|=PP_CPU_CAPS_MMX2;
#endif
    if (context->pix_fmt == PIX_FMT_YUV420P)
      flags|=PP_FORMAT_420;
    else if (context->pix_fmt == PIX_FMT_YUV422P)
      flags|=PP_FORMAT_422;
    else if (context->pix_fmt == PIX_FMT_YUV444P)
      flags|=PP_FORMAT_444;

    ppcontext = pp_get_context(context->width, context->height,flags);
  }

  deintWork = setupStore.deintMethod;
  if (currentDeintMethod != deintWork || ppmode == NULL 
      || currentppMethod != setupStore.ppMethod 
      || currentppQuality != setupStore.ppQuality ) {
    // reallocate ppmode if method or quality changed

    if (ppmode)  {
      pp_free_mode (ppmode);
      ppmode = NULL;
    }
    char mode[60]="";
    if (setupStore.getPPdeintValue() && setupStore.getPPValue())
      sprintf(mode,"%s,%s",setupStore.getPPdeintValue(),
          setupStore.getPPValue());
    else if (setupStore.getPPdeintValue() )
      sprintf(mode,"%s",setupStore.getPPdeintValue());
    else if (setupStore.getPPValue() )
      sprintf(mode,"%s",setupStore.getPPValue());

    ppmode = pp_get_mode_by_name_and_quality(mode, setupStore.ppQuality);
    
    currentDeintMethod = deintWork;
    currentppMethod = setupStore.ppMethod;
    currentppQuality = setupStore.ppQuality;
  }

  if (ppmode == NULL || ppcontext == NULL) {
    fprintf(stderr,
            "[softdevice] pp-filter %s couldn't be initialized,\n"
            "[softdevice] switching postprocessing off !\n",
            setupStore.getPPValue());
    setupStore.deintMethod = 0;
    return;
  }

  
  if ( !pic_buf_pp ) {
    fprintf(stderr,
            "[softdevice] no picture buffer is allocated for postprocessing !\n"
            "[softdevice] switching postprocessing off !\n");
    setupStore.deintMethod = 0;
    return;
  }
 
  avpicture_fill(&avpic_dest,pic_buf_pp,
                    context->pix_fmt,context->width ,context->height);

  pp_postprocess(picture->data,
      picture->linesize,
      (uchar **)&avpic_dest.data,
      (int *)&avpic_dest.linesize,
      context->width,
      context->height,
      picture->qscale_table,
      picture->qstride,
      ppmode,
      ppcontext,
      picture->pict_type);

  memcpy(picture->data,avpic_dest.data,sizeof(picture->data));
  memcpy(picture->linesize,avpic_dest.linesize,sizeof(picture->data));
}
#endif //PP_LIBAVCODEC

//----------------------------- end of postproc stuff ----------------------

cVideoStreamDecoder::~cVideoStreamDecoder()
{
  cClock::AdjustVideoPTS(0);
  videoOut->InvalidateOldPicture();
  delete(syncTimer);
  free(picture);
  if (pic_buf_lavc)
     pic_buf_lavc = freePicBuf(pic_buf_lavc);
  if (pic_buf_pp)
     pic_buf_pp = freePicBuf(pic_buf_pp);
  if (pic_buf_convert)
     pic_buf_convert = freePicBuf(pic_buf_convert);
  if (pic_buf_mirror)
     pic_buf_mirror = freePicBuf(pic_buf_mirror);

#ifdef PP_LIBAVCODEC
  if (ppcontext) {
     pp_free_context(ppcontext);
     ppcontext = NULL;
  }

  if (ppmode)  {
      pp_free_mode (ppmode);
      ppmode = NULL;
  }
#endif  
}

// --------------libavformat interface stuff ---------------------------------
static int read_packet_RingBuffer(void *opaque, uint8_t *buf, int buf_size) {
     cMpeg2Decoder *Dec=(cMpeg2Decoder *)(opaque);
     if (Dec) 
       return Dec->read_packet(buf,buf_size);
     return -1;
};
    
#if LIBAVFORMAT_BUILD >4625
static offset_t seek_RingBuffer(void *opaque, offset_t offset, int whence) 
#else
static int seek_RingBuffer(void *opaque, offset_t offset, int whence) 
#endif
{
     cMpeg2Decoder *Dec=(cMpeg2Decoder *)(opaque);
     if (Dec) 
       return Dec->seek(offset, whence);
     return -1;
};

//----------------------------   MPEG Decoder
cMpeg2Decoder::cMpeg2Decoder(cAudioOut *AudioOut, cVideoOut *VideoOut)
{
  if ( avcodec_build() != LIBAVCODEC_BUILD ) {
     fprintf(stderr,"Fatal Error! Libavcodec library build(%d) doesn't match avcodec.h build(%d)!!!\n",avcodec_build(),LIBAVCODEC_BUILD);
     fprintf(stderr,"Check your ffmpeg installation / the pathes in the Makefile!!!\n");
     exit(-1);
  };

  avcodec_init();
  avcodec_register_all();
 
  audioOut=AudioOut;
  videoOut=VideoOut;
  aout=NULL;
  vout=NULL;
  
  StreamBuffer=NULL;
  
  running=false;
  decoding=false;
  IsSuspended=false;
  Speed=1;
}

cMpeg2Decoder::~cMpeg2Decoder()
{
  if (aout)
    delete(aout);
  aout=0;
  if (vout)
    delete(vout);
  vout=0;
}

int cMpeg2Decoder::read_packet(uint8_t *buf, int buf_size) {
   BUFDEB("read_packet Del %d\n",LastSize);
    StreamBuffer->Del(LastSize);
    int size=StreamBuffer->Available(); 
    if ( size < buf_size ) {
      BUFDEB("read_packet Available()%d < buf_size EnablePut.Signal\n",size);
      EnablePutSignal.Signal();
    };
start:
    int count=0;
    size=StreamBuffer->Available(); 
    while ( size < buf_size && ThreadActive 
           && count <  1  ) {
      BUFDEB("read_packet EnableGet.Sleep start\n");
      EnableGetSignal.Sleep(50000);
      BUFDEB("read_packet EnableGet.Sleep end\n");
      size = StreamBuffer->Available();
      if (size>188) {
        //try to get more data...
        count++;
      };
   };
    
    // signal eof if thread should end...
    if (!ThreadActive && size == 0) 
      return -1;
      
    size = buf_size;
    uchar *u =StreamBuffer->Get(size);
    if (u) {
       //size-=8;
       // libavformat wants to be able to read beyond the boundaries
       if (size>buf_size)
         size=buf_size;
 
       ic->pb.buffer=u;
       LastSize=size;
       BUFDEB("read_packet: got %d bytes\n",size);
       return size;
    } else  {
       BUFDEB("read_packet u is NULL!!!\n");
       if (ThreadActive) {
         //try again...
         usleep(10000);
         goto start;
       };
    };
    return 0;
};

int cMpeg2Decoder::seek(offset_t offset, int whence) {
   printf("unimplemented: seek offset %lld whence %d\n", (long long int)offset, whence);
   return -EINVAL;
};

void cMpeg2Decoder::initStream() {
   AVInputFormat *fmt;
   
   LastSize=0;
   av_register_all();

   fmt=av_find_input_format("mpeg");
   fmt->flags |= AVFMT_NOFILE;

   if ( int err=av_open_input_file(&ic, "null",fmt,0,NULL) ) {
       printf("Failed to open input stream.Error %d\n",err);
   };

   init_put_byte(&ic->pb, NULL,dvb_buf_size[setupStore.bufferMode]/2, 0, this,
       read_packet_RingBuffer,NULL,seek_RingBuffer);
   CMDDEB("init put byte finished\n");
};

//------------------------------------------------------
void cMpeg2Decoder::Action()
{
  CMDDEB("Neuer Thread gestartet: Mpeg2Decoder pid %d\n",getpid());
  ThreadRunning=true;
  //ThreadActive=true;
  AVPacket  pkt;
  int ret;
  int PacketCount=0;

  int nStreams=0;
  
  while(ThreadActive) {
        while (freezeMode && ThreadActive)
          usleep(50000);
      
        //ret = av_read_frame(ic, &pkt);
        BUFDEB("av_read_frame start\n");
        ret = av_read_packet(ic, &pkt);
        if (ret < 0) {
            BUFDEB("cMpeg2Decoder Stream Error!\n");
            if (ThreadActive)
		    usleep(10000);
            continue;
        }
        PacketCount++;
        BUFDEB("got packet from av_read_frame!\n");

        if ( pkt.pts != (int64_t) AV_NOPTS_VALUE )
          pkt.pts/=9;

        QueuePacket(ic,pkt);

        if (nStreams!=ic->nb_streams ){
          PacketCount=0;
          nStreams=ic->nb_streams;
/*        fprintf(stderr,"Streams: %d\n",nStreams);
          for (int i=0; i <nStreams; i++ ) {
            printf("Codec %d ID: %d\n",i,ic->streams[i]->codec.codec_id);
          };*/
        };
/*
        if (!(PacketCount % 100)) {
          sched_yield();
        };
*/
//        if (PacketCount == 200)
//          dump_format(ic, 0, "test", 0);
  }
  running=false;
  CMDDEB("Thread beendet : mpegDecoder pid %d\n",getpid());
}

void cMpeg2Decoder::ClearPacketQueue()
{
  if (aout)
    aout->Clear();
  if (vout)
    vout->Clear();
};

void cMpeg2Decoder::QueuePacket(const AVFormatContext *ic, AVPacket &pkt)
{
  BUFDEB("QueuePacket AudioIdx: %d VideoIdx %d pkt.stream_index: %d\n",
    AudioIdx,VideoIdx,pkt.stream_index);

  if (!ic) {
        fprintf(stderr,"Error: ic is null!\n");
        av_free_packet(&pkt);
        return;
  };
  if ( pkt.stream_index >= ic->nb_streams ) {
         fprintf(stderr,"Error: stream index larger than nb_streams\n");
        av_free_packet(&pkt);
        return;
  }; 
  // check if there are new streams
  if (AudioIdx != DONT_PLAY && ic->streams[pkt.stream_index] &&
#if LIBAVFORMAT_BUILD > 4628
      ic->streams[pkt.stream_index]->codec->codec_type == CODEC_TYPE_AUDIO &&
#else
      ic->streams[pkt.stream_index]->codec.codec_type == CODEC_TYPE_AUDIO &&
#endif
      AudioIdx != pkt.stream_index) {

    CMDDEB("new Audio stream index.. old %d new %d\n",
      AudioIdx,pkt.stream_index);
    AudioIdx = pkt.stream_index;
    if (aout) {
      aout->Stop();
      delete aout;
      aout=NULL;
    };
#if LIBAVFORMAT_BUILD > 4628
    aout = new cAudioStreamDecoder(ic->streams[pkt.stream_index]->codec,
                 audioOut, audioMode );
#else
    aout = new cAudioStreamDecoder(&ic->streams[pkt.stream_index]->codec,
                 audioOut, audioMode );
#endif
  } else
#if LIBAVFORMAT_BUILD > 4628
  if (VideoIdx != DONT_PLAY && ic->streams[pkt.stream_index] &&
      ic->streams[pkt.stream_index]->codec->codec_type == CODEC_TYPE_VIDEO &&
      VideoIdx!=pkt.stream_index) 
#else
  if (VideoIdx != DONT_PLAY && ic->streams[pkt.stream_index] &&
      ic->streams[pkt.stream_index]->codec.codec_type == CODEC_TYPE_VIDEO &&
      VideoIdx!=pkt.stream_index) 
#endif
  {
    CMDDEB("new Video stream index.. old %d new %d\n",
      VideoIdx,pkt.stream_index);
    VideoIdx=pkt.stream_index;
    if (vout) {
      vout->Stop();
      delete vout;
      vout = NULL;
    };
#if LIBAVFORMAT_BUILD > 4628
    vout = new cVideoStreamDecoder(ic->streams[pkt.stream_index]->codec,
                   videoOut, &clock, Speed );
#else
    vout = new cVideoStreamDecoder(&ic->streams[pkt.stream_index]->codec,
                   videoOut, &clock, Speed );
#endif
  };
  
  // write streams 
  if ( pkt.stream_index == VideoIdx && vout ) {
    BUFDEB("QueuePacket video stream\n");
    while ( vout->PutPacket(pkt) == -1 && ThreadActive ) {
      // PutPacket sleeps is necessary
      //printf("Video Buffer full\n");
    };
  } else if ( pkt.stream_index == AudioIdx && aout ) {
    BUFDEB("QueuePacket audio stream\n");
    while ( aout->PutPacket(pkt) == -1 && ThreadActive ) {
      // PutPacket sleeps is necessary
      //printf("Audio Buffer full\n");
    };
  } else {
    //printf("Unknown packet or vout or aout not init!!\n");
    av_free_packet(&pkt);
  }
 BUFDEB("QueuePacket finished...\n");
};

void cMpeg2Decoder::Start(bool GetMutex)
{
  CMDDEB("Start IsSuspended %d \n",IsSuspended);
  if (IsSuspended)
    // don't start if we are suspended
    return;
 
  if (GetMutex)
    mutex.Lock();
   
  if (StreamBuffer) {
  	delete StreamBuffer;
	StreamBuffer=NULL;
  };
  StreamBuffer=new cSoftRingBufferLinear(dvb_buf_size[setupStore.bufferMode],0);
  StreamBuffer->Clear();
  initStream();

  ThreadActive=true;
  freezeMode=false;
  AudioIdx=NO_STREAM;
  VideoIdx=NO_STREAM;
  cThread::Start();
  running=true;
  if (GetMutex)
    mutex.Unlock();
}

void cMpeg2Decoder::Suspend()
{
  CMDDEB("Suspend\n");
  Stop();
  audioOut->Suspend();
  videoOut->Suspend();
  IsSuspended=true;
}

void cMpeg2Decoder::Resume()
{
  CMDDEB("Resume\n");
  IsSuspended=false;
  if (!videoOut->Resume()) {
        fprintf(stderr,"Could not open video out! Sleeping again...\n");
        videoOut->Suspend();
        setupStore.shouldSuspend=true;
        IsSuspended=true;
        return;
  };

  if (!audioOut->Resume()) {
        fprintf(stderr,"Could not open audio out! Sleeping again...\n");
        setupStore.shouldSuspend=true;
        IsSuspended=true;
        videoOut->Suspend();
        return;
  };

  Start();
  IsSuspended=false;
}

void cMpeg2Decoder::Play(void)
{
  CMDDEB("Play\n");
  freezeMode=false;
  if (running) 
  {
    if (aout)
      aout->Play();
    if (vout)
      vout->Play();

    cClock::SetFreezeMode(false);
  };
};

void cMpeg2Decoder::SetPlayMode(softPlayMode playMode)
{
  curPlayMode=playMode;
  switch (curPlayMode) {
    case PmAudioVideo: 
      CMDDEB("SetPlayMode PmAudioVideo\n");
      PlayAudioVideo(true,true);
      break;
    case PmVideoOnly:
      CMDDEB("SetPlayMode PmVideoOnly\n");
      PlayAudioVideo(false,true);
      break;
    case PmAudioOnly:
      CMDDEB("SetPlayMode PmAudioOnly\n");
      PlayAudioVideo(true,false);
      break;
    default:
      break;
  };
};
      
void cMpeg2Decoder::Freeze(void)
{
  CMDDEB("Freeze\n");
  freezeMode=true;
  // sleep a short while before putting the
  // audio and video stream decoders to sleep
  usleep(20000);
  if (running) 
  { 
    if (aout)
      aout->Freeze();
    if (vout)
      vout->Freeze();

    cClock::SetFreezeMode(true);
  };
};

void cMpeg2Decoder::Stop(bool GetMutex)
{
  if (GetMutex)
     mutex.Lock();
  CMDDEB("Stop\n");
  // can't stop properly in freeze mode
  //  Freeze();
  freezeMode=false;
  if (running)
  {
    running=false;
    ThreadActive=false;

    StreamBuffer->Clear();
    EnableGetSignal.Signal();
    Cancel(4);
    CMDDEB("stopping video\n"); 
    if (vout) {
      vout->Stop();
      delete(vout);
    }
    CMDDEB("stopping audio\n");
    if (aout) {
      aout->Stop();
      delete(aout);
    }
    aout=NULL;
    vout=NULL;
    av_close_input_file(ic);
  }
  if (StreamBuffer) {
     delete StreamBuffer;
     StreamBuffer=NULL;
  };

  Speed = 1;
  CMDDEB("Stop finished\n");
  if (GetMutex)
    mutex.Unlock();
}

/* ----------------------------------------------------------------------------
 */
int cMpeg2Decoder::StillPicture(uchar *Data, int Length)
{
  //Clear();
  mutex.Lock();
  CMDDEB("StillPicture \n");
  // XXX hack to ingore audio junk sent by vdr in the still picture
  AudioIdx=DONT_PLAY;
  for (int i=0; 4>i;i++) {
    int P;
    uchar *data=Data;
    int Size=Length;
    while ( (P = StreamBuffer->Put(data, Size)) != Size ) {
      data+=P;
      Size-=P;
      usleep( 10000 );
    }
  }
  CMDDEB("StillPicture end \n");
  mutex.Unlock();
  return Length;
}

/* ----------------------------------------------------------------------------
 */
void cMpeg2Decoder::Clear(void)
{
  mutex.Lock();
  CMDDEB("Clear\n");
  Stop(false);
  Start(false);
  CMDDEB("Clear finished\n");
  mutex.Unlock();
}

/* ----------------------------------------------------------------------------
 */
void cMpeg2Decoder::TrickSpeed(int trickSpeed)
{
  CMDDEB("TrickSpeed %d\n",Speed);
  Speed=trickSpeed;
  // XXX hack to ingore audio junk sent by vdr in the
  if (trickSpeed!=1) {
    if (aout)
      aout->Clear();
    AudioIdx=DONT_PLAY;
  } else if (AudioIdx==DONT_PLAY)
    AudioIdx=NO_STREAM;
  Play();
  if (running)
  {
    if (aout)
      aout->TrickSpeed(Speed);
    if (vout)
      vout->TrickSpeed(Speed);
  };
}
void cMpeg2Decoder::SetAudioMode(int AudioMode)  { 
  CMDDEB("SetAudioMode %d\n",AudioMode);
  audioMode=AudioMode; 
  if (aout) 
    aout->SetAudioMode(audioMode); 
};

/* ----------------------------------------------------------------------------
 */
int64_t cMpeg2Decoder::GetSTC(void) {
  if (running) {
    return clock.GetPTS()*9;

    if (vout)
      return vout->GetPTS()*9;
    if (aout)
      return aout->GetPTS()*9;
  }
  return -1;
};

int cMpeg2Decoder::BufferFill() 
{
  if (freezeMode)
    return 100;
  int fill=0;
  int fill2=0;
  BUFDEB("buffer fill\n");
  if (running && vout )
    fill= vout->BufferFill();
    BUFDEB("vout buffer fill: %d\n",fill);
  if (running && aout )
    if ( (fill2=aout->BufferFill()) >fill)
       fill=fill2;
    
    BUFDEB("aout buffer fill: %d\n",fill2);
  return fill;
}

/* ----------------------------------------------------------------------------
 */
int cMpeg2Decoder::Decode(const uchar *Data, int Length)
{
  BUFDEB("Decode %x, Length %d\n",Data,Length);
  if (running && !IsSuspended && setupStore.shouldSuspend)
     // still running and should suspend
     Suspend();

  if (!running && IsSuspended && !setupStore.shouldSuspend)
     // not running and should resume
     Resume();
     
  if (!running) {
    BUFDEB("not running..\n");
    return Length;
  };

  if (freezeMode)
    return 0;

  //MPGDEB("Decode: StreamBuffer: %d plus %d\n",StreamBuffer->Available(),Length);
  mutex.Lock();
  int P;
  int Size=Length;
  while ( (P = StreamBuffer->Put(Data, Size)) != Size ) {
    Data+=P;
    Size-=P;
    BUFDEB("Decode EnableGet.Signal(), EnablePut.Sleep start\n");
    EnableGetSignal.Signal();
    EnablePutSignal.Sleep(50000);
    BUFDEB("Decode EnablePut.Sleep end\n");
  }
  mutex.Unlock();
  if (StreamBuffer->Available()>MIN_BUF_SIZE) {
      BUFDEB("Decode Available >MIN_BUF_SIZE EnableGetSignal.Signal \n");
      EnableGetSignal.Signal();
  };
  BUFDEB("Decode finished\n");
  
  return Length;
}
