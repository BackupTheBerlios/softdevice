/*
 * mpeg2decoder.c: A plugin for the Video Disk Recorder
 *
 * See the README file for copyright information and how to reach the author.
 *
 * $Id: mpeg2decoder.c,v 1.65 2006/06/15 21:34:20 wachm Exp $
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

//#define CMDDEB(out...) {printf("CMD[%04d]:",(int)(getTimeMilis() % 10000));printf(out);}

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
  delete[] queue;
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

cStreamDecoder::cStreamDecoder(AVCodecContext *Context, bool packetMode)
        : PacketQueue(packet_buf_size[setupStore.bufferMode])
{
  context=Context;
  if (context)
        context->error_resilience=1;
  CMDDEB("Neuer StreamDecoder Pid: %d context %p type %d\n",
        getpid(),context,context->codec_type );
  pts=0;
  frame=0;
  this->packetMode = packetMode;
  initCodec();
  syncTimer=NULL;

//  Context->debug |=0xF|FF_DEBUG_STARTCODE|FF_DEBUG_PTS;
//  av_log_set_level(AV_LOG_DEBUG);
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

void cStreamDecoder::Freeze(bool freeze)
{
  //printf("freeze mode\n");
  freezeMode=freeze;
};

void cStreamDecoder::Stop(void)
{
  CMDDEB("cStreamDecoder::Stop\n");
  active=false;
  if (syncTimer)
    syncTimer->Signal();
  mutex.Lock();
  PacketQueue.Clear();
  mutex.Unlock();
  PacketQueue.EnablePutSignal();
  Cancel(3);
  CMDDEB("cStreamDecoder::Stop finished\n");
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

  if (!packetMode)
  {
      /* we don't send complete frames */
    if(codec->capabilities&CODEC_CAP_TRUNCATED)
      context->flags|= CODEC_FLAG_TRUNCATED;
  }

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
                                         cAudioOut *AudioOut, int AudioMode,
                                         bool packetMode)
                                         : cStreamDecoder(Context, packetMode)
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

/* ---------------------------------------------------------------------------
 */
void cAudioStreamDecoder::OnlyLeft(uint8_t *samples,int Length) {
  for (int i=0; i < Length/2; i+=2)
     ((uint16_t*)samples)[i+1]=((uint16_t*)samples)[i];
};

/* ---------------------------------------------------------------------------
 */
void cAudioStreamDecoder::OnlyRight(uint8_t *samples,int Length) {
  for (int i=0; i < Length/2; i+=2)
    ((uint16_t*)samples)[i]=((uint16_t*)samples)[i+1];
};

/* ---------------------------------------------------------------------------
 */
bool cAudioStreamDecoder::UpmixMono(uint8_t *samples,int Length) {
  if (Length > AVCODEC_MAX_AUDIO_FRAME_SIZE / 2)
    return false;

  for (int i = 2*Length -1; i >= 0; i--) {
    ((uint16_t*)samples)[i]=((uint16_t*)samples)[i/2];
    ((uint16_t*)samples)[i+1]=((uint16_t*)samples)[i/2];
  }
  return true;
}

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
      {
          int       delta_pts;
          uint64_t  PTS;

        // feed data for AC3 pass through to device
        audioOut->WriteAC3(data,size);
//
// TODO it's a hack for subsequent pts counting
//
#define AC3_PTS_TRACE 0

        delta_pts = (size*10000*2/(48000*3));
        pts += delta_pts;
        if (pkt->pts != (int64_t) AV_NOPTS_VALUE)
        {
#if AC3_PTS_TRACE
          fprintf (stderr,
                   "audio pts %lld pkt->PTS : %lld pts - valid PTS: %lld \n",
                   pts,pkt->pts,(int) pts - pkt->pts );
#endif
          pts = pkt->pts;
          pkt->pts=AV_NOPTS_VALUE;
        }
        PTS = pts - audioOut->GetDelay() + setupStore.avOffset*10;
#if AC3_PTS_TRACE
        fprintf (stderr, "audio pts offset %lld %d\n",
                 cClock::GetTime()-PTS, delta_pts);
#endif
        cClock::AdjustAudioPTS(PTS);

        return size;
      }
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
    }
    else if (!audioMode && audioOutContext.channels==1) {
      // respect true "mono only" channels
      if (UpmixMono(audiosamples, audio_size)) {
        audioOutContext.channels = 2;
        audio_size *= 2;
      }
    }

    audioOut->SetParams(audioOutContext);
    audioOut->Write(audiosamples,audio_size);
    // adjust PTS according to audio_size, sampel_rate and no. of channels
    if (!context->channels)
      pts += (audio_size*10000/(context->sample_rate*2*2));
    else
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
  CMDDEB("~cAudioStreamDecoder\n");
  cClock::AdjustAudioPTS(0);
  Cancel(3);
  free(audiosamples);
}

// --- VIDEO ------------------------------------------------------------------

int GetBuffer(struct AVCodecContext *c, AVFrame *pic) {
  int w=c->width;
  int h=c->height;

#if LIBAVCODEC_BUILD >  4737
  if(avcodec_check_dimensions(c,w,h))
    return -1;
#endif

#if LIBAVCODEC_BUILD >  4713
  avcodec_align_dimensions(c, &w, &h);
#endif
  
#define EDGE_WIDTH 16
  bool EmuEdge=c->flags&CODEC_FLAG_EMU_EDGE;
  if(!EmuEdge){
    w+= EDGE_WIDTH*2;
    h+= EDGE_WIDTH*2;
  }
  sPicBuffer *buf;

  cVideoOut *VideoOutPtr=(cVideoOut *)c->opaque;
  if (!VideoOutPtr) {
    fprintf(stderr,"VideoOutPtr null!\n");
    return 0;
  };

  buf=VideoOutPtr->GetBuffer(c->pix_fmt,w,h);
  if (!buf || !buf->pixel[0]) {
    fprintf(stderr,"buf or buf->pixel[0] null!\n");
    return 0;
  };
  
  pic->type= FF_BUFFER_TYPE_USER;
  pic->opaque = (void*) buf;

  int h_chroma_shift, v_chroma_shift;
  VideoOutPtr->GetChromaSubSample(c->pix_fmt, h_chroma_shift, v_chroma_shift);

  for(int i=0; i<4; i++){
    const int h_shift= i==0 ? 0 : h_chroma_shift;
    const int v_shift= i==0 ? 0 : v_chroma_shift;

    pic->base[i]= buf->pixel[i];
    if(EmuEdge) {
      pic->data[i] = buf->pixel[i];
      buf->edge_width=buf->edge_height=0;
    } else {
      buf->edge_width=buf->edge_height=EDGE_WIDTH;
      pic->data[i] = buf->pixel[i] + (buf->stride[i]*
          EDGE_WIDTH>>v_shift) + (EDGE_WIDTH>>h_shift);
      //pic->data[i] = buf->pixel[i] + ALIGN((buf->stride[i]*
      //              EDGE_WIDTH>>v_shift) + (EDGE_WIDTH>>h_shift),
      //              STRIDE_ALIGN);
    };
    pic->linesize[i]= buf->stride[i];
  }
  pic->age=buf->age;

  return 1;
};

void ReleaseBuffer(struct AVCodecContext *c, AVFrame *pic) {
  cVideoOut *VideoOutPtr=(cVideoOut *)c->opaque;
  if (!VideoOutPtr)
    return;
  
  VideoOutPtr->ReleaseBuffer((sPicBuffer *)pic->opaque);
  for(int i=0; i<3; i++){
    pic->data[i]=NULL;
  }
  pic->opaque = (void*) NULL;
};

cVideoStreamDecoder::cVideoStreamDecoder(AVCodecContext *Context,
                                         cVideoOut *VideoOut, cClock *Clock,
                                         int Trickspeed,
                                         bool packetMode)
     : cStreamDecoder(Context, packetMode), Mirror(VideoOut),
       DeintLibav(VideoOut)
#ifdef PP_LIBAVCODEC
       ,LibAvPostProc(VideoOut)
#endif
{
  memset(pts_values,-1,sizeof(pts_values));
  lastPTSidx=lastPTS=0;
  lastCodedPictNo=-1;
  videoOut=VideoOut;
  clock = Clock;

  // init A-V syncing variables
  offset=0;
  delay=0;
  hurry_up=0;
  syncTimer = new cSyncTimer ((eSyncMode) setupStore.syncTimerMode);
  syncTimer->Reset();

  default_frametime = DEFAULT_FRAMETIME;
  trickspeed = Trickspeed;

  picture=avcodec_alloc_frame();

  if (codec->capabilities&CODEC_CAP_DR1) {
          context->get_buffer=GetBuffer;
          context->release_buffer=ReleaseBuffer;
          context->opaque=(void*)videoOut;
  } else {
          context->opaque=NULL;
          printf("not using direct rendering\n");
  };
}

void cVideoStreamDecoder::Play(void)
{
  cStreamDecoder::Play();
  videoOut->FreezeMode(freezeMode);
};

void cVideoStreamDecoder::Freeze(bool freeze)
{
  cStreamDecoder::Freeze(freeze);
  videoOut->FreezeMode(freezeMode);
};

uint64_t cVideoStreamDecoder::GetPTS() {
  return pts - (delay + syncTimer->TimePassed())/100;
}

int cVideoStreamDecoder::DecodePicture_avcodec(sPicBuffer *&pic, int &got_picture,
                              uint8_t *data, int length, int64_t pkt_pts) {
  int len=0;
  
  if ( context->width  > 2048 || context->height  > 2048 ) {
    fprintf(stderr,"Invalid width (%d) or height (%d), most likely a broken stream\n",
        context->width,context->height);
    context->width=0;
    context->height=0;
    return -1;
  };

  len = avcodec_decode_video(context, picture, &got_picture,data, length);
  if (len < 0)
    return len;

  // save coded picture number together with corresponding pts
  if (context->coded_frame  &&
      context->coded_frame->coded_picture_number!=lastCodedPictNo
      && lastPTS != (int64_t) AV_NOPTS_VALUE ) {
  //  (*(sPicBuffer *)context->coded_frame->opaque).pts=lastPTS;
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
  }

  if (pkt_pts != (int64_t)AV_NOPTS_VALUE) {
          lastPTS = pkt_pts;
          // FIXME duration?
  };

  if (!got_picture)
    return len;

  // we got a picture
  if ( picture->opaque && context->opaque ) {
          pic=(sPicBuffer *) picture->opaque;
  } else {
          // no direct rendering
          pic=&privBuffer;
          pic->edge_width=pic->edge_height=0;
          for (int i=0; i<4; i++) {
                  pic->pixel[i]=picture->data[i];
                  pic->stride[i]=picture->linesize[i];
          };
          pic->owner=NULL;
          pic->format=context->pix_fmt;
  };      
          
  pic->pts=AV_NOPTS_VALUE;
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
     pic->pts = pts_values[findPTS].pts;
  }

  /* ------------------------------------------------------------------------
   * DV read via dv1394 seems to have
   * context->coded_frame->coded_picture_number
   * always set to zero. Therefor lets do the following guess upon current
   * PTS value. Don't know if there are other codecs which deliver
   * codec_picture_number as zero.
   * Hopefully noone else will see '#' chars printed.
   */
  if (!pic->pts && lastPTS != (int64_t) AV_NOPTS_VALUE)
  {
    fprintf(stderr,"#");
    pic->pts = lastPTS;
  }

  // save the picture's properties
#if LIBAVCODEC_BUILD > 4684
  pic->interlaced_frame=picture->interlaced_frame;
#endif
  pic->width=context->width;
  pic->height=context->height;
#if LIBAVCODEC_BUILD > 4686
  /* --------------------------------------------------------------------------
   * removed aspect ratio calculation based on picture->pan_scan->width
   * as this value seems to be wrong on some dvds.
   * reverted this removal as effect from above it is not reproducable.
   */
  if (!context->sample_aspect_ratio.num)
  {
    pic->aspect_ratio = (float) (context->width) / (float) (context->height);
  }
  else if (picture->pan_scan && picture->pan_scan->width)
  {
    pic->aspect_ratio = (float) (picture->pan_scan->width *
        context->sample_aspect_ratio.num) /
      (float) (picture->pan_scan->height *
                context->sample_aspect_ratio.den);
  }
  else
  {
    pic->aspect_ratio =(float) (context->width * 
        context->sample_aspect_ratio.num) /
      (float) (context->height * 
                context->sample_aspect_ratio.den);
  }
#else
  pic->aspect_ratio = context->aspect_ratio;
#endif
  pic->dtg_active_format= context->dtg_active_format;
  return len;
};

int cVideoStreamDecoder::DecodePacket(AVPacket *pkt)
{
  int len=0;
  int got_picture=0;
  sPicBuffer *pic;

  uint8_t *data=pkt->data;
  int size=pkt->size;
  //MPGDEB("got video packet\n");
  while ( size > 0 ) {
    BUFDEB("start decode video stream %d data: %p size: %d \n",
        pkt->stream_index,data,size);
    len = DecodePicture_avcodec(pic, got_picture,
                                data, size, pkt->pts)
      //avcodec_decode_video(context, picture, &got_picture,data, size);
    BUFDEB("end decode video got_picture %d, data %p, size %d\n",
        got_picture,data,size);
    if (len < 0) {
      printf("[mpegdecoder] Error while decoding video frame %d\n", frame);
      resetCodec();
      return len;
    }
    size-=len;
    data+=len;


    // FIXME check this again...
    if (pkt->pts != (int64_t) AV_NOPTS_VALUE) {
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
      Mirror.Filter(pic,pic);
   
    if (setupStore.deintMethod == 1)
      DeintLibav.Filter(pic,pic);

#ifdef PP_LIBAVCODEC
#ifdef FB_SUPPORT
    if (setupStore.deintMethod > 2 || setupStore.ppMethod!=0 )
#else
    if (setupStore.deintMethod > 1 || setupStore.ppMethod!=0 )
#endif //FB_SUPPORT
      LibAvPostProc.Filter(pic,pic);
#endif //PP_LIBAVCODEC
    
  if (pic->pts != AV_NOPTS_VALUE )
          pts=pic->pts;

  if (!hurry_up || frame % 2 ) {
    MPGDEB("DrawVideo...delay : %d\n",delay);
    videoOut->DrawVideo_420pl(syncTimer, &delay, pic);
    MPGDEB("end DrawVideo\n");
  } else
    fprintf(stderr,"+");
  // we just displayed a frame, now it's the right time to
  // measure the A-V offset
  // the A-V syncing code is partly based on MPlayer...
  uint64_t aPTS = clock->GetPTS();
  // update video pts
  cClock::AdjustVideoPTS(pts);

  if ( aPTS )
    offset = aPTS - pts ;
  else offset = 0;
  if ( abs(offset) > 100000)
          offset=0;

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
      ,(float)dispTime/1000.0 );
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
       sqrt(offsetSqSum/((float)Freq)-(offsetSum*offsetSum)/((float)(Freq*Freq))),
       DispTSum/(float)Freq/1000.0,hurry_up);
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

cVideoStreamDecoder::~cVideoStreamDecoder()
{
  cClock::AdjustVideoPTS(0);
  delete(syncTimer);
  syncTimer=NULL;
  free(picture);
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
  aoutMutex.Lock();
  if (aout)
    delete(aout);
  aout=0;
  aoutMutex.Unlock();

  voutMutex.Lock();
  if (vout)
    delete(vout);
  vout=0;
  voutMutex.Unlock();
}

int cMpeg2Decoder::read_packet(uint8_t *buf, int buf_size) {
   if (!StreamBuffer)
           return 0;
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
   ic->pb.buf_end=NULL;
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

        BUFDEB("av_read_frame start\n");
        //ret = av_read_frame(ic, &pkt);
        ret = av_read_packet(ic, &pkt);
        if (ret < 0) {
            BUFDEB("cMpeg2Decoder Stream Error!\n");
            if (ThreadActive)
		    usleep(10000);
            continue;
        }
        //av_dup_packet(&pkt);
        PacketCount++;
        BUFDEB("got packet from av_read_frame!\n");

        if ( pkt.pts != (int64_t) AV_NOPTS_VALUE )
          pkt.pts/=9;

        QueuePacket(ic,pkt,packetMode);

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

void cMpeg2Decoder::ResetDecoder(int Stream)
{
  CMDDEB("ResetDecoder %d\n",Stream);

  if (Stream & SOFTDEVICE_AUDIO_STREAM)  {
    aoutMutex.Lock();
    if (aout) {
      CMDDEB("ResetDecoder resetting audio decoder\n");
      aout->Stop();
      delete aout;
      aout=NULL;
      AudioIdx=NO_STREAM;
    };
    aoutMutex.Unlock();
  };

  if (Stream & SOFTDEVICE_VIDEO_STREAM)  {
    voutMutex.Lock();
    if (vout) {
      CMDDEB("ResetDecoder resetting video decoder\n");
      vout->Stop();
      delete vout;
      vout=NULL;
      VideoIdx=NO_STREAM;
    };
    voutMutex.Unlock();
  };
};

void cMpeg2Decoder::QueuePacket(const AVFormatContext *ic, AVPacket &pkt,
		bool PacketMode)
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
  int packet_type=CODEC_TYPE_UNKNOWN;

#if LIBAVFORMAT_BUILD > 4628
  if ( ic->streams[pkt.stream_index]
                  && ic->streams[pkt.stream_index]->codec )
          packet_type = ic->streams[pkt.stream_index]->codec->codec_type;
#else
  if ( ic->streams[pkt.stream_index] )
          packet_type = ic->streams[pkt.stream_index]->codec.codec_type;
#endif

  if ( packet_type == CODEC_TYPE_UNKNOWN ) {
          BUFDEB("Unknown packet type! Return;\n");
          return;
  };

  // check if there are new streams
  if ( AudioIdx != DONT_PLAY && packet_type == CODEC_TYPE_AUDIO
       && AudioIdx != pkt.stream_index) {
    CMDDEB("new Audio stream index.. old %d new %d\n",
      AudioIdx,pkt.stream_index);
    AudioIdx = pkt.stream_index;
    aoutMutex.Lock();
    if (aout) {
      aout->Stop();
      delete aout;
      aout=NULL;
    };
#if LIBAVFORMAT_BUILD > 4628
    aout = new cAudioStreamDecoder(ic->streams[pkt.stream_index]->codec,
                 audioOut, audioMode, PacketMode );
#else
    aout = new cAudioStreamDecoder(&ic->streams[pkt.stream_index]->codec,
                 audioOut, audioMode, PacketMode );
#endif
    aoutMutex.Unlock();
  } else
  if (VideoIdx != DONT_PLAY && packet_type == CODEC_TYPE_VIDEO
       && VideoIdx!=pkt.stream_index) {
    CMDDEB("new Video stream index.. old %d new %d\n",
      VideoIdx,pkt.stream_index);
    VideoIdx=pkt.stream_index;
    voutMutex.Lock();
    if (vout) {
      vout->Stop();
      delete vout;
      vout = NULL;
    };
    voutMutex.Unlock();
#if LIBAVFORMAT_BUILD > 4628
    vout = new cVideoStreamDecoder(ic->streams[pkt.stream_index]->codec,
                   videoOut, &clock, Speed, PacketMode );
#else
    vout = new cVideoStreamDecoder(&ic->streams[pkt.stream_index]->codec,
                   videoOut, &clock, Speed, PacketMode );
#endif
  };

  // write streams
  voutMutex.Lock();
  if ( packet_type == CODEC_TYPE_VIDEO && vout ) {
    BUFDEB("QueuePacket video stream\n");
    while ( vout->PutPacket(pkt) == -1 && ThreadActive ) {
      // PutPacket sleeps is necessary
      //printf("Video Buffer full\n");
    };
  } ;
  voutMutex.Unlock();

  aoutMutex.Lock();
  if ( packet_type == CODEC_TYPE_AUDIO && aout ) {
    BUFDEB("QueuePacket audio stream\n");
    while ( aout->PutPacket(pkt) == -1 && ThreadActive ) {
      // PutPacket sleeps is necessary
      //printf("Audio Buffer full\n");
    };
  };
  aoutMutex.Unlock();

  if ( packet_type != CODEC_TYPE_VIDEO && packet_type != CODEC_TYPE_AUDIO ) {
    //printf("Unknown packet or vout or aout not init!!\n");
    av_free_packet(&pkt);
  }
 BUFDEB("QueuePacket finished...\n");
};

void cMpeg2Decoder::Start(bool GetMutex)
{
  CMDDEB("Mpeg2Decoder Start IsSuspended %d  GetMutex %d\n",IsSuspended,GetMutex);
  if (running)
          return;

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
  CMDDEB("mpeg2Decoder Start finished");
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
    aoutMutex.Lock();
    if (aout)
      aout->Play();
    aoutMutex.Unlock();

    voutMutex.Lock();
    if (vout)
      vout->Play();
    voutMutex.Unlock();

    cClock::SetFreezeMode(false);
  };
};

void cMpeg2Decoder::SetPlayMode(softPlayMode playMode, bool packetMode)
{
  curPlayMode=playMode;
  this->packetMode=packetMode;
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

void cMpeg2Decoder::Freeze(int Stream, bool freeze)
{
  CMDDEB("Freeze Streams %d freeze %d\n",Stream,freeze);
  if (Stream & SOFTDEVICE_BOTH_STREAMS == SOFTDEVICE_BOTH_STREAMS )
    freezeMode=freeze;
  // sleep a short while before putting the
  // audio and video stream decoders to sleep
  usleep(20000);
  if (running)
  {
    if (Stream & SOFTDEVICE_AUDIO_STREAM) {
      aoutMutex.Lock();
      if (aout)
        aout->Freeze(freeze);
      aoutMutex.Unlock();
    };

    if (Stream & SOFTDEVICE_VIDEO_STREAM) {
      voutMutex.Lock();
      if (vout)
        vout->Freeze(freeze);
      voutMutex.Unlock();
    };

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
    voutMutex.Lock();
    if (vout) {
      vout->Stop();
      delete(vout);
    }
    vout=NULL;
    voutMutex.Unlock();

    CMDDEB("stopping audio\n");
    aoutMutex.Lock();
    if (aout) {
      aout->Stop();
      delete(aout);
    }
    aout=NULL;
    aoutMutex.Unlock();

    av_close_input_file(ic);
    ic=NULL;
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

static uint8_t pes_packet_header[] = {
     0x00,0x00,0x01, 0xe0,           0x00,0x00,   0x84, 0x00, 0x00,0x00};
     // startcode,  video-stream 0, packet length,      no pts
int cMpeg2Decoder::StillPicture(uchar *Data, int Length)
{
  bool has_pesheader=false;
  CMDDEB("StillPicture %p length %d \n",Data,Length);
  // XXX hack to ingore audio junk sent by vdr in the still picture
  AudioIdx=DONT_PLAY;

  // check if data contains a valid pes header
#define SEARCH_LENGTH 64
  if (Length>SEARCH_LENGTH) {
    uchar *start=Data+1;
    do {
      start++;
      start=(uchar *)memchr(start,0x01,Data+SEARCH_LENGTH-start);
      if ( start && start[-1]==0 && start[-2] == 0
          && ((start[1] &0xF0)==0xe0) ) { // video stream pes header
        has_pesheader=true;
        break;
      };
    } while (start<Data+SEARCH_LENGTH && start);
  };

  for (int i=0; 4>i;i++) {
    if (!has_pesheader) {
      // send a fake pes header
      pes_packet_header[4]=(Length+2)>>8 & 0xFF;
      pes_packet_header[5]=(Length+2) & 0xFF;
      Decode(pes_packet_header,9);
    };
    Decode(Data,Length);
  }
  CMDDEB("StillPicture end \n");
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
    aoutMutex.Lock();
    if (aout)
      aout->Clear();
    aoutMutex.Unlock();

    AudioIdx=DONT_PLAY;
  } else if (AudioIdx==DONT_PLAY)
    AudioIdx=NO_STREAM;
  Play();
  if (running)
  {
    aoutMutex.Lock();
    if (aout)
      aout->TrickSpeed(Speed);
    aoutMutex.Unlock();

    voutMutex.Lock();
    if (vout)
      vout->TrickSpeed(Speed);
    voutMutex.Unlock();
  };
}
void cMpeg2Decoder::SetAudioMode(int AudioMode)  {
  CMDDEB("SetAudioMode %d\n",AudioMode);
  audioMode=AudioMode;
  aoutMutex.Lock();
  if (aout)
    aout->SetAudioMode(audioMode);
  aoutMutex.Unlock();
};

/* ----------------------------------------------------------------------------
 */
int64_t cMpeg2Decoder::GetSTC(void) {
  if (running) {
    return clock.GetPTS()*9;
/*
    if (vout)
      return vout->GetPTS()*9;
    if (aout)
      return aout->GetPTS()*9;
      */
  }
  return -1;
};

int cMpeg2Decoder::BufferFill(int Stream)
{
  if (freezeMode)
    return 100;

  int video_Fill=0;
  int audio_Fill=0;

  BUFDEB("BufferFill Stream %d vout %p aout %p\n",Stream,vout,aout);
  if (Stream & SOFTDEVICE_VIDEO_STREAM) {
    voutMutex.Lock();
    if ( vout  )
      video_Fill = vout->BufferFill();
    voutMutex.Unlock();
  };

  if ( Stream & SOFTDEVICE_AUDIO_STREAM )  {
    aoutMutex.Lock();
    if (aout)
      audio_Fill = aout->BufferFill();
    aoutMutex.Unlock();
  };

  BUFDEB("audio_Fill: %d video_Fill: %d\n",audio_Fill,video_Fill);
  return video_Fill>audio_Fill ? video_Fill : audio_Fill;
}

/* ----------------------------------------------------------------------------
 */
int cMpeg2Decoder::Decode(const uchar *Data, int Length)
{
  BUFDEB("Decode %p, Length %d\n",Data,Length);

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

  if (!StreamBuffer)
          return 0;

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
