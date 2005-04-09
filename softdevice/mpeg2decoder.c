/*
 * mpeg2decoder.c: A plugin for the Video Disk Recorder
 *
 * See the README file for copyright information and how to reach the author.
 *
 * $Id: mpeg2decoder.c,v 1.28 2005/04/09 13:09:17 wachm Exp $
 */

#include <math.h>

#include <vdr/plugin.h>

//#define SIG_TIMING
#ifndef SIG_TIMING
// for RTC
#include <sys/ioctl.h>
#include <linux/rtc.h>
#endif

#include "mpeg2decoder.h"
#include "audio.h"
#include "utils.h"
#include "setup-softdevice.h"


//#define MPGDEB(out...) {printf("mpegdec[%04d]:",getTimeMilis() % 10000);printf(out);}

#ifndef MPGDEB
#define MPGDEB(out...)
#endif

//#define CMDDEB(out...) {printf("CMD[%04d]:",getTimeMilis() % 10000);printf(out);}

#ifndef CMDDEB
#define CMDDEB(out...)
#endif

//#define BUFDEB(out...) {printf("BUF[%04d]:",getTimeMilis() % 10000);printf(out);}

#ifndef BUFDEB
#define BUFDEB(out...)
#endif


//#define AV_STATS
//------------------------------------cPacketBuffer------------------------

cPacketQueue::cPacketQueue() {
  FirstPacket=LastPacket=0;
  memset(queue,0,sizeof(queue));
};

int cPacketQueue::PutPacket(const AVPacket &Packet) {
  if (FirstPacket != Next(LastPacket)) {
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
  if (FirstPacket!=LastPacket) 
    return &queue[FirstPacket];
  else return NULL;
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

cAudioStreamDecoder *cClock::audioClock=NULL;
cVideoStreamDecoder *cClock::videoClock=NULL;
bool                 cClock::waitForSync=false;

uint64_t  cClock::GetPTS() {
  if (audioClock)
     return audioClock->GetPTS();
  else return 0;
};

//-----------------------cRelTimer-----------------------
int32_t cRelTimer::TimePassed()
{
  int64_t now;
  int32_t ret;

  now=GetTime();
  if ( now < lastTime ) {
    ret = (uint32_t) (now - lastTime + 60 *1000000); // untested
    MPGDEB("now %lld kleiner als lastTime %lld\n",now,lastTime);
  }
  else ret = now - lastTime;
  return ret;
};

int32_t cRelTimer::GetRelTime() 
{
  int64_t now;
  int32_t ret;

  now=GetTime();
  if ( now < lastTime ) {
    ret = (uint32_t) (now - lastTime + 60 *1000000); // untested
    MPGDEB("now %lld kleiner als lastTime %lld\n",now,lastTime);
  }
  else ret = now - lastTime;
  lastTime=now;
  return ret;
};
//-----------------------cSleepTimer-----------------------
 
void cSleepTimer::Sleep( int timeoutUS )
{
  if ( timeoutUS < 0 )
    return;
 
  struct timeval tv;
  gettimeofday(&tv,NULL);
  struct timespec timeout;
  timeout.tv_nsec=(tv.tv_usec+timeoutUS);//*1000;
  timeout.tv_sec=tv.tv_sec + timeout.tv_nsec / 1000000;
  timeout.tv_nsec%=1000000;
  timeout.tv_nsec*=1000;
  pthread_mutex_lock(&mutex);
  int retcode=0;
  while ( retcode != ETIMEDOUT ) {
    retcode = pthread_cond_timedwait(&cond, &mutex, &timeout);
  }
 
  pthread_mutex_unlock(&mutex);
};

// --- cStreamDecoder ---------------------------------------------------------

cStreamDecoder::cStreamDecoder(AVCodecContext *Context)
{
  context=Context;
  context->error_resilience=1;
  CMDDEB("Neuer StreamDecoder Pid: %d type %d\n",getpid(),context->codec_type );
  pts=0;
  frame=0;
  initCodec();

  active=true;
  Start(); // starte thread
}

cStreamDecoder::~cStreamDecoder()
{
  active=false;
  Cancel(3);
  if (codec) 
    avcodec_close(context);
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
      //printf("got packet pid: %d type %d \n",getpid(),context->codec_type);
       if (codec) 
         DecodePacket(pkt);
       av_free_packet(pkt);
       PacketQueue.FreeReadPacket(pkt);
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
};

void cStreamDecoder::Freeze(void)
{
  //printf("freeze mode\n");
  freezeMode=true;
};

void cStreamDecoder::Stop(void) 
{
  active=false;
  Cancel(3);
}

void cStreamDecoder::Clear(void)
{
  CMDDEB("cStreamDecoder clear\n");
  mutex.Lock();
  avcodec_flush_buffers(context);
  PacketQueue.Clear();
  mutex.Unlock();
  CMDDEB("cStreamDecoder clear finished\n");
}

bool cStreamDecoder::initCodec(void)
{
  codec = avcodec_find_decoder(context->codec_id);
  if (!codec)
  {
    printf("[mpegdecoder] Error! Codec %d not supported by libavcodec found\n",
      context->codec_id);
    return false;
  }
  
    /* we don't send complete frames */
  if(codec->capabilities&CODEC_CAP_TRUNCATED)
    context->flags|= CODEC_FLAG_TRUNCATED; 
  
  if (avcodec_open(context, codec) < 0)
  {
    printf("[mpegdecoder] Fatal error! Could not open codec %d\n",
      context->codec_id);
    return false;
  }
  MPGDEB("Codec %d initialized.\n");
  return true;
};

void cStreamDecoder::resetCodec(void)
{
  printf("[mpegdecoder] resetting codec\n");
  avcodec_close(context);
  initCodec();
}

int cStreamDecoder::BufferFill()
{
  return ( PacketQueue.Available() ) *100/PacketQueue.MaxPackets ;
}

// --- AUDIO ------------------------------------------------------------------
cAudioStreamDecoder::cAudioStreamDecoder(AVCodecContext *Context,
                                         cAudioOut *AudioOut, int AudioMode)
                                         : cStreamDecoder(Context)
{
  audioOut=AudioOut;
  audioMode=AudioMode;
  audiosamples=(uint8_t *)malloc(AVCODEC_MAX_AUDIO_FRAME_SIZE);
  
  audioBuffer=new cSoftRingBufferLinear(48*2*2*200*20,2*1024);
  audioOutContext.period_size=2*1024;
  audioOutContext.samplerate=44100;
  audioOutContext.channels=2;
}

/* ---------------------------------------------------------------------------
 */
uint64_t cAudioStreamDecoder::GetPTS()
{
  uint64_t PTS= pts - audioOut->GetDelay() + setupStore.avOffset*10;
  if (audioBuffer && audioOutContext.channels)
     PTS-=(audioBuffer->Available()*10000 /
       (audioOutContext.samplerate*2*audioOutContext.channels));
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
  int len=0;
  int audio_size=0;
   
  uint8_t *data=pkt->data;
  int size=pkt->size;

  while ( size > 0 ) {
    len=avcodec_decode_audio(context, (short *)audiosamples, 
                 &audio_size, data, size);
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
    if (audio_size == 0)
      continue;

    if (!frame)
    {
      //first frame - wait for ready for play
      cClock::SetAudioClock(this);
      while ( !cClock::ReadyForPlay() ) {
         usleep(10000);
         MPGDEB("audioStreamDecoder waiting for ReadyForPlay...\n");
      };
    };
    audioOutContext.channels=context->channels;
    audioOutContext.samplerate=context->sample_rate;
    audioOut->SetParams(audioOutContext);
    //audioOut->SetParams(context->channels,context->sample_rate);
    //audioOut->Write(audiosamples,audio_size);
    audioBuffer->Put(audiosamples,audio_size);
    // adjust PTS according to audio_size, sampel_rate and no. of channels  
    pts += (audio_size*10000/(context->sample_rate*2*context->channels)); 

    if (pkt->pts != (int64_t) AV_NOPTS_VALUE) {
      MPGDEB("audio pts %lld pkt->PTS : %lld pts - valid PTS: %lld \n",
         pts,pkt->pts,(int) pts - pkt->pts );
      pts = pkt->pts;
      pkt->pts=AV_NOPTS_VALUE;
    }
    frame++;
  }

  while ( (unsigned) audioBuffer->Available() >= audioOutContext.period_size )
  {
    uint8_t *samples=audioBuffer->Get(audio_size);
    if (!samples) 
        break;
    if ( (unsigned) audio_size  > audioOutContext.period_size )
      audio_size=audioOutContext.period_size;

    if (audioMode && audioOutContext.channels==2) {
      // respect mono left/right only channels
      if (audioMode==1)
          OnlyLeft(samples,audio_size);
      else OnlyRight(samples,audio_size);
    };
    audioOut->Write(samples,audio_size);
    audioBuffer->Del(audio_size);
  };
  MPGDEB("a_delay %d at end of decode\n",audioOut->GetDelay());

  return len;
}


cAudioStreamDecoder::~cAudioStreamDecoder()
{
  cClock::SetAudioClock(NULL);
  Cancel(3);
  free(audiosamples);
  delete audioBuffer;
}

// --- VIDEO ------------------------------------------------------------------

cVideoStreamDecoder::cVideoStreamDecoder(AVCodecContext *Context,
                                         cVideoOut *VideoOut, cClock *Clock)
                                         : cStreamDecoder(Context)
{
  width = height = -1;
  pic_buf_lavc = pic_buf_mirror = pic_buf_pp = NULL;
  currentMirrorMode  = setupStore.mirror;
  currentDeintMethod = setupStore.deintMethod;

  memset(pts_values,-1,sizeof(pts_values));
  lastPTSidx=lastPTS=0;
  lastCodedPictNo=-1;
#ifdef PP_LIBAVCODEC
  ppmode = ppcontext = NULL;
#endif //PP_LIBAVCODEC
  videoOut=VideoOut;
  clock = Clock;

  // init A-V syncing variables
  frametime=DEFAULT_FRAMETIME;
  syncOnAudio=1;
  offset=0;
  delay=0;
  hurry_up=0;
  Timer.Reset();

#ifndef SIG_TIMING
  if ( (rtc_fd = open("/dev/rtc",O_RDONLY)) < 0 ) 
    fprintf(stderr,"Could not open /dev/rtc \n");
  else 
  {
    uint64_t irqp = 1024;

    if ( ioctl(rtc_fd, RTC_IRQP_SET, irqp) < 0) 
    {
      //fprintf(stderr,"Could not set irq period\n");
      close(rtc_fd);
      rtc_fd=-1;
    }
    else if ( ioctl( rtc_fd, RTC_PIE_ON, 0 ) < 0) 
    {
      //fprintf(stderr,"Error in rtc_pie on \n");
      close(rtc_fd);
      rtc_fd=-1;
    };// else fprintf(stderr,"Set up to use linux RTC\n");
 };
#else
  rtc_fd=-1;
#endif

  picture=avcodec_alloc_frame();
}

uint64_t cVideoStreamDecoder::GetPTS() {
  return pts - (delay + Timer.TimePassed())/100;
};

int cVideoStreamDecoder::DecodePacket(AVPacket *pkt)
{
  int len=0;
  int got_picture=0;

  uint8_t *data=pkt->data;
  int size=pkt->size;
  //MPGDEB("got video packet\n");
  while ( size > 0 ) {
    len = avcodec_decode_video(context, picture, &got_picture,data, size);
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
    
    if (pkt->pts != (int64_t) AV_NOPTS_VALUE)
         lastPTS=pkt->pts;

    if (!got_picture)
      continue;

    if (!frame)
    {
      //first frame - wait for ready for play 
      cClock::SetVideoClock(this);
      while ( !cClock::ReadyForPlay() ) {
        MPGDEB("audioStreamDecoder waiting for ReadyForPlay...\n");
        usleep(10000);
      };
    };
      
    // postproc stuff....
    if (setupStore.mirror == 1)
      Mirror();
    else if (pic_buf_mirror)
      pic_buf_mirror = freePicBuf(pic_buf_mirror);

    if (setupStore.deintMethod == 0
#ifdef FB_SUPPORT
        || setupStore.deintMethod == 2
#endif
       )
    {
      pic_buf_lavc = freePicBuf(pic_buf_lavc);
      pic_buf_pp = freePicBuf(pic_buf_pp);
    }
    else if (setupStore.deintMethod == 1)
      deintLibavcodec();
#ifdef PP_LIBAVCODEC
#ifdef FB_SUPPORT
    else if (setupStore.deintMethod > 2)
#else
    else if (setupStore.deintMethod > 1)
#endif //FB_SUPPORT
      ppLibavcodec();
#endif //PP_LIBAVCODEC

  width  = context->width;
  height = context->height;
  /*
     if ( abs(AudioStream->GetPTS() - pts)  > 10000 ) 
     {
  //hmm, pts is completly wrong... just sync to audio
  pts=AudioStream->GetPTS();
  offset=0;
  MPGDEB("pts = AudioPTS\n");
  };
   */
/*   if (picture->pts)
   {
     MPGDEB("got pts from picture: old pts: %lld pict pts: %lld\n",pts,picture->pts/100);
     pts = picture->pts/100;
   } else {*/
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
   //}
  
  // prepare picture for display
  videoOut->CheckAspectDimensions(picture,context);
  
  if (!hurry_up || frame % 2 ) {
  // sleep ....
  delay-=Timer.GetRelTime();
  MPGDEB("Frame# %-5d  aPTS: %lld offset: %d delay %d \n",frame,clock->GetPTS(),offset,delay );
#ifdef SIG_TIMING
  Timer.Sleep(delay-1000);
  delay-=Timer.GetRelTime();
#else
  if ( rtc_fd >= 0 ) {
    // RTC timinig
    while (delay > 15000) {
      //sleep one timer tick
      usleep(10000);
      //MPGDEB("RTC sleep loop %d \n",delay);
      delay-=Timer.GetRelTime();
    }
    while (delay  > 1200) {
      uint32_t ts;
      //MPGDEB("RTC Loop %d \n",delay);
      if ( read(rtc_fd, &ts, sizeof(ts) )  <= 0) 
      {
        fprintf(stderr,"Linux RTC read error, disableing RTC\n");
        close(rtc_fd);
        rtc_fd=-1;
      }
      delay-=Timer.GetRelTime();
    }
  } else {
    // usleep timing
    const int rest=2200;
    while (delay > rest) {     
      //usleep(1000);
      usleep(rest);
      delay  -= Timer.GetRelTime();
      //MPGDEB("Loop %d \n",delay);  return len;
    }
    // cpu burn timing
    //while (delay > 100) { 
    //  delay  -= Timer.GetRelTime();
    //   printf("Loop2 %d \n",delay);
    //};
  }
#endif
  
  // display picture
  videoOut->YUV(picture->data[0], picture->data[1],picture->data[2],
      context->width,context->height,
      picture->linesize[0],picture->linesize[1]);
  };
  // we just displayed a frame, now it's the right time to
  // measure the A-V offset
  // the A-V syncing code is partly based on MPlayer...
  uint64_t aPTS = clock->GetPTS();

  if ( syncOnAudio && aPTS )
    offset = aPTS - pts ;
  else offset = 0;

  // this few lines does the whole syncing
  int pts_corr;

  // calculate pts correction. Max. correction is 1/10 frametime.
  pts_corr = offset * 10;
  if (pts_corr > frametime*100*2 )
    pts_corr = frametime*100*2;
  else if (pts_corr < -frametime*100*2)
    pts_corr =-frametime*100*2;

  // calculate delay
  delay += ( frametime *1000 - pts_corr ) ;
  // update video pts 
  pts += frametime*10;
  // so that pts - delay/1000 is always the current PTS
 
  if (delay > 2*frametime*1000)
    delay = 2*frametime*1000;
  else if (delay < -2*frametime*1000)
    delay = -2*frametime*1000;    


  if (offset >  8*frametime*10)
     hurry_up=1;
  else if ( (offset < 2*frametime*10) && (hurry_up > 0) )
     hurry_up=0;

#if 1
  int dispTime=Timer.GetRelTime();
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
  frametime = DEFAULT_FRAMETIME * Speed;
  syncOnAudio = ( Speed == 1);
}


//------------------------------------ postproc stuff -------------------------
uchar *cVideoStreamDecoder::allocatePicBuf(uchar *pic_buf)
{
  // (re)allocate picture buffer for deinterlaced/mirrored picture
  if (pic_buf == NULL)
    fprintf(stderr,
            "[softdevice] allocating picture buffer for resolution %dx%d\n",
            context->width, context->height);
  else
    fprintf(stderr,
            "[softdevice] resolution changed to %dx%d - "
            "reallocating picture buffer\n",
            context->width, context->height);

  pic_buf = (uchar *)realloc(pic_buf,
                             context->width *
                              context->height *
                                sizeof(uchar) *
                                  3 / 2);

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
      context->height != height)
    pic_buf_lavc = allocatePicBuf(pic_buf_lavc);

  if (pic_buf_lavc)
  {
    avpic_dest.data[0] = pic_buf_lavc;
    avpic_dest.data[1] = avpic_dest.data[0] + context->width * context->height;
    avpic_dest.data[2] = avpic_dest.data[1] + context->width * context->height / 4;

    avpic_dest.linesize[0] = context->width;
    avpic_dest.linesize[1] = context->width / 2;
    avpic_dest.linesize[2] = context->width / 2;

    avpic_src.data[0]     = picture->data[0];
    avpic_src.data[1]     = picture->data[1];
    avpic_src.data[2]     = picture->data[2];

    avpic_src.linesize[0] = picture->linesize[0];
    avpic_src.linesize[1] = picture->linesize[1];
    avpic_src.linesize[2] = picture->linesize[2];

    if (avpicture_deinterlace(&avpic_dest, &avpic_src, context->pix_fmt,
                              context->width, context->height) < 0)
    {
      fprintf(stderr,
              "[softdevice] error, libavcodec deinterlacer failure\n"
              "[softdevice] switching deinterlacing off !\n");
      setupStore.deintMethod = 0;
    }
    else
    {
      picture->data[0]     = avpic_dest.data[0];
      picture->data[1]     = avpic_dest.data[1];
      picture->data[2]     = avpic_dest.data[2];

      picture->linesize[0] = avpic_dest.linesize[0];
      picture->linesize[1] = avpic_dest.linesize[1];
      picture->linesize[2] = avpic_dest.linesize[2];
    }

  }
  else
  {
    fprintf(stderr,
            "[softdevice] no picture buffer is allocated for deinterlacing !\n"
            "[softdevice] switching deinterlacing off !\n");
    setupStore.deintMethod = 0;
  }
}

void cVideoStreamDecoder::Mirror(void)
{
    uchar *ptr_src1, *ptr_src2;
    uchar *ptr_dest1, *ptr_dest2;

  if (pic_buf_mirror == NULL ||
      context->width != width ||
      context->height != height)
    pic_buf_mirror = allocatePicBuf(pic_buf_mirror);

  if (pic_buf_mirror)
  {
    avpic_dest.data[0] = pic_buf_mirror;
    avpic_dest.data[1] = avpic_dest.data[0] + context->width * context->height;
    avpic_dest.data[2] = avpic_dest.data[1] + context->width * context->height / 4;

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

    for (int h = 0; h < context->height / 2; h++)
    {
      for (int w = context->width / 2; w > 0; w--)
      {
        *ptr_dest1 = ptr_src1[h * picture->linesize[0] / 2 + w - 1];
        *ptr_dest2 = ptr_src2[h * picture->linesize[0] / 2 + w - 1];
        ptr_dest1++;
        ptr_dest2++;
      }
    }

    picture->data[0]     = avpic_dest.data[0];
    picture->data[1]     = avpic_dest.data[1];
    picture->data[2]     = avpic_dest.data[2];

    picture->linesize[0] = context->width;
    picture->linesize[1] = context->width / 2;
    picture->linesize[2] = context->width / 2;

  }
  else
  {
    fprintf(stderr,
            "[softdevice] no picture buffer is allocated for mirroring !\n"
            "[softdevice] switching mirroring off !\n");
    setupStore.mirror = 0;
  }
}

#ifdef PP_LIBAVCODEC
void cVideoStreamDecoder::ppLibavcodec(void)
{
    int deintWork;

  if (pic_buf_pp == NULL ||
      context->width != width ||
      context->height != height)
    pic_buf_pp = allocatePicBuf(pic_buf_pp);

  if (ppcontext == NULL ||
      context->width != width ||
      context->height != height)
  {
    if (ppcontext)
    {
      pp_free_context(ppcontext);
      ppcontext = NULL;
    }
    /* set one of this values instead of 0 in pp_get_context for
     * processor-independent optimations:
       PP_CPU_CAPS_MMX, PP_CPU_CAPS_MMX2, PP_CPU_CAPS_3DNOW
     */
    ppcontext = pp_get_context(context->width, context->height, 0);
  }

  deintWork = setupStore.deintMethod;
#if FB_SUPPORT
  if (currentDeintMethod > 2)
#else
  if (currentDeintMethod > 1)
#endif
  {
    if (currentDeintMethod != deintWork || ppmode == NULL)
    {

      if (ppmode)
      {
        pp_free_mode (ppmode);
        ppmode = NULL;
      }
    
      ppmode = pp_get_mode_by_name_and_quality(setupStore.getPPValue(), 6);
      currentDeintMethod = deintWork;
    }
  }

  if (ppmode == NULL || ppcontext == NULL)
  {
    fprintf(stderr,
            "[softdevice] pp-filter %s couldn't be initialized,\n"
            "[softdevice] switching postprocessing off !\n",
            setupStore.getPPValue());
    setupStore.deintMethod = 0;
  }
  else
  {
    if (pic_buf_pp)
    {
      avpic_dest.data[0] = pic_buf_pp;
      avpic_dest.data[1] = avpic_dest.data[0] +
                            context->width * context->height;
      avpic_dest.data[2] = avpic_dest.data[1] +
                            context->width * context->height / 4;

      avpic_dest.linesize[0] = context->width;
      avpic_dest.linesize[1] = context->width / 2;
      avpic_dest.linesize[2] = context->width / 2;

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

      picture->data[0]     = avpic_dest.data[0];
      picture->data[1]     = avpic_dest.data[1];
      picture->data[2]     = avpic_dest.data[2];

      picture->linesize[0] = avpic_dest.linesize[0];
      picture->linesize[1] = avpic_dest.linesize[1];
      picture->linesize[2] = avpic_dest.linesize[2];
    }
    else
    {
      fprintf(stderr,
              "[softdevice] no picture buffer is allocated for postprocessing !\n"
              "[softdevice] switching postprocessing off !\n");
      setupStore.deintMethod = 0;
    }
  }
}
#endif //PP_LIBAVCODEC
//----------------------------- end of postproc stuff ----------------------

cVideoStreamDecoder::~cVideoStreamDecoder()
{
  cClock::SetVideoClock(NULL);
  //RTC
  if (rtc_fd)
    close(rtc_fd);

  free(picture);
}

// --------------libavformat interface stuff ---------------------------------
static int read_packet_RingBuffer(void *opaque, uint8_t *buf, int buf_size) {
     cMpeg2Decoder *Dec=(cMpeg2Decoder *)(opaque);
     if (Dec) 
       return Dec->read_packet(buf,buf_size);
     return -1;
};
     
static int seek_RingBuffer(void *opaque, offset_t offset, int whence) {
     cMpeg2Decoder *Dec=(cMpeg2Decoder *)(opaque);
     if (Dec) 
       return Dec->seek(offset, whence);
     return -1;
};

//----------------------------   MPEG Decoder
cMpeg2Decoder::cMpeg2Decoder(cAudioOut *AudioOut, cVideoOut *VideoOut)
{
  avcodec_init();
  avcodec_register_all();
 
  audioOut=AudioOut;
  videoOut=VideoOut;
  aout=NULL;
  vout=NULL;
  
  StreamBuffer=new cSoftRingBufferLinear(DVB_BUF_SIZE,16);
  
  running=false;
  decoding=false;
  IsSuspended=false;
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
//    printf("Del %d\n",LastSize);
    StreamBuffer->Del(LastSize);
start:
    BUFDEB("read_packet: StreamBuffer: %d\n",StreamBuffer->Available());
    int size=StreamBuffer->Available(); 
    int count=0;
    while ( size < buf_size && ThreadActive 
           && count <  1  ) {
      size = StreamBuffer->Available();
      usleep(10000);
      if (size>188) {
        count++;
        BUFDEB("read_packet: sleeping while data (%d) in buffer\n",size);
      };
    };
    
    // signal eof if thread should end...
    if (!ThreadActive && size == 0) 
      return -1;
      
    size = buf_size;
    uchar *u =StreamBuffer->Get(size);
    if (u) {
       size-=8;
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
   printf("unimplemented: seek offset %d whence %d\n",offset,whence);
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

   init_put_byte(&ic->pb, NULL, 32*1024, 0, this,
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
            BUFDEB("cMpeg2Decoder Stream Error!!!!!!!!!!!!!!!!!!\n");
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

        if (!(PacketCount % 100)) {
          usleep(10000);
        };

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
  // check if there are new streams
  if (AudioIdx != DONT_PLAY &&
      ic->streams[pkt.stream_index]->codec.codec_type == CODEC_TYPE_AUDIO && 
      AudioIdx != pkt.stream_index) {
   
    CMDDEB("new Audio stream index.. old %d new %d\n",
      AudioIdx,pkt.stream_index);
    AudioIdx = pkt.stream_index;
    if (aout) {
      aout->Stop();
      delete aout;
      aout=NULL;
    };
    aout = new cAudioStreamDecoder(&ic->streams[pkt.stream_index]->codec,
                 audioOut, audioMode );
  } else 
  if (VideoIdx != DONT_PLAY &&
      ic->streams[pkt.stream_index]->codec.codec_type == CODEC_TYPE_VIDEO && 
      VideoIdx!=pkt.stream_index) {
  
    CMDDEB("new Video stream index.. old %d new %d\n",
      VideoIdx,pkt.stream_index);
    VideoIdx=pkt.stream_index;
    if (vout) {
      vout->Stop();
      delete aout;
      vout = NULL;
    };
    vout = new cVideoStreamDecoder(&ic->streams[pkt.stream_index]->codec, 
                   videoOut, &clock );
  };
  
  // write streams 
  if ( pkt.stream_index == VideoIdx && vout ) {
    BUFDEB("QueuePacket video stream\n");
    while ( vout->PutPacket(pkt) == -1 ) {
      //printf("Video Buffer full\n");
      usleep(50000);
    };
  } else if ( pkt.stream_index == AudioIdx && aout ) {
    BUFDEB("QueuePacket audio stream\n");
    while ( aout->PutPacket(pkt) == -1 ) {
      //printf("Audio Buffer full\n");
      usleep(50000);
    };
  } else {
    //printf("Unknown packet or vout or aout not init!!\n");
    av_free_packet(&pkt);
  }

};

void cMpeg2Decoder::Start(bool GetMutex)
{
  CMDDEB("Start IsSuspended %d \n",IsSuspended);
  if (IsSuspended)
    // don't start if we are suspended
    return;
 
  if (GetMutex)
    mutex.Lock();
    
  StreamBuffer->Clear();
  initStream();
  clock.SetWaitForSync(curPlayMode==PmAudioVideo);
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
  };
};

void cMpeg2Decoder::SetPlayMode(softPlayMode playMode)
{
  curPlayMode=playMode;
  clock.SetWaitForSync(curPlayMode==PmAudioVideo);
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
  clock.SetWaitForSync(false);
  if (running)
  {
    running=false;
    
    ThreadActive=false;
    Cancel(4);
    StreamBuffer->Clear();
   
    if (vout) {
      vout->Stop();
      delete(vout);
    }

    if (aout) {
      aout->Stop();
      delete(aout);
    }
    aout=NULL;
    vout=NULL;
    av_close_input_file(ic);
  }
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
  // we have only video, so no syncing should be done
  clock.SetWaitForSync(false); 
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
void cMpeg2Decoder::TrickSpeed(int Speed)
{
  CMDDEB("TrickSpeed %d\n",Speed);
  if ( Speed!=1 )
    clock.SetWaitForSync(false);
  else clock.SetWaitForSync(curPlayMode==PmAudioVideo);

  if (running)
  {
    if (aout)
      aout->TrickSpeed(Speed);
    if (vout)
      vout->TrickSpeed(Speed);
  };
}

/* ----------------------------------------------------------------------------
 */
int64_t cMpeg2Decoder::GetSTC(void) {
  if (running) {
    if (vout)
      return vout->GetPTS()*9;
    if (aout)
      return aout->GetPTS()*9;
  }
  return -1;
};

int cMpeg2Decoder::BufferFill() 
{
  int fill=0;
  if (running && vout )
    fill= vout->BufferFill();
  if (running && aout )
    if (aout->BufferFill()>fill)
       fill=aout->BufferFill();
    
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
    usleep( 50000 );
  }
  mutex.Unlock();
  BUFDEB("Deocde finished\n");
  
  return Length;
}
