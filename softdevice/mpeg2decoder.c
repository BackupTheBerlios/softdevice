/*
 * mpeg2decoder.c: A plugin for the Video Disk Recorder
 *
 * See the README file for copyright information and how to reach the author.
 *
 * $Id: mpeg2decoder.c,v 1.18 2005/03/05 14:35:51 lucke Exp $
 */

#include <math.h>

#include <vdr/plugin.h>

// for RTC
#include <sys/ioctl.h>
#include <linux/rtc.h>

#include "mpeg2decoder.h"
#include "audio.h"
#include "utils.h"
#include "setup-softdevice.h"
#define UNSYNCED 0
#define PAYLOAD 100
#define PAYLOADDATA 200
#define HEADER 300
#define OPTHEADER 400
#define STREAM 500

//#define HDRDEB(out...) {printf("header[%04d]:",getTimeMilis() % 10000);printf(out);}

#ifndef HDRDEB
#define HDRDEB(out...)
#endif

//#define MPGDEB(out...) {printf("mpegdec[%04d]:",getTimeMilis() % 10000);printf(out);}

#ifndef MPGDEB
#define MPGDEB(out...)
#endif

//#define BUFDEB(out...) {printf("BUF[%04d]:",getTimeMilis() % 10000);printf(out);}

#ifndef BUFDEB
#define BUFDEB(out...)
#endif

//#define CMDDEB(out...) {printf("CMD[%04d]:",getTimeMilis() % 10000);printf(out);}

#ifndef CMDDEB
#define CMDDEB(out...)
#endif


//#define AV_STATS


#define GET_MPEG2_PTS(x)   ( ((uint64_t)x[4]&0xFE) >>1     | \
                             ((uint64_t)x[3])      <<7     | \
                             ((uint64_t)x[2]&0xFE) <<14    | \
                             ((uint64_t)x[1])      <<22    | \
                             ((uint64_t)x[0]&0x0E) <<29 )

#define AC3_TEST  1

// --- cStreamDecoder ---------------------------------------------------------

cStreamDecoder::cStreamDecoder(unsigned int StreamID)
{
  streamID = StreamID;
  frame=0;
  freezeMode=false;
  state=UNSYNCED;
  prevPTS = newPTS = pts=0;
  for (int i = 0; i < PTS_COUNT; i++)
    historyPTS [i] = 0;
  historyPTSIndex = 0;
  validPTS=false;

  ringBuffer=new cSoftRingBufferLinear(DVB_BUF_SIZE,1024,true);
  //ringBuffer->SetTimeouts(100,10);
  Start(); // starte thread
  memset(header,0,MAX_HDR_LEN);
}



cStreamDecoder::~cStreamDecoder()
{
  active=false;
  Cancel(3);
}

int cStreamDecoder::ParseStream(uchar *Data, int Length)
{
  return ParseStreamIntern(Data,Length,streamID,streamID);
}

int cStreamDecoder::ParseStreamIntern(uchar *Data, int Length,
                                      unsigned int lowID,
                                      unsigned int highID)
{
    int len, streamlen;
    uint8_t *inbuf_ptr;
    int size=Length;

  inbuf_ptr = (uint8_t *)Data;
  while (size > 0)
  {
    len=1;
    switch (state)
    {
      // here we found a sync header
      case UNSYNCED:
        syncword = (syncword << 8) | *inbuf_ptr;
        if ( syncword >= lowID && syncword <= highID)
          state=HEADER;
        break;

      case HEADER:        //Payload length(hi)
        payload = *inbuf_ptr << 8;
        state++;
        break;

      case HEADER+1:      //Payload length(lo)
        payload += *inbuf_ptr;
        state++;
        break;

      case HEADER+2:      //weiß ich nicht (immer 0x80 oder 0x85)
        state++;
        *(uint8_t *)&Header1 = * inbuf_ptr;
        payload--;
        break;

      case HEADER+3:      // ??
        state++;
        *(uint8_t *)&Header2 = *inbuf_ptr;
        payload--;
        break;

      case HEADER+4:      // PTS-Header length
        headerLength=*inbuf_ptr;
        state=STREAM;
        hdr_ptr=0;
        validPTS=false;

        if (headerLength)
        { // we found optional headers (=PTS)
          validPTS=true;
          state=OPTHEADER;
          hdr_ptr=header;
          HDRDEB("ID 0x%x PtsDts 0x%x ESCR 0x%x ESRATE 0x%x optHLength 0x%x\n",
             streamID,Header2.ptsdts_flags,Header2.has_escr,Header2.has_es_rate,headerLength); 
        }
        payload--;
        break;

      case OPTHEADER:     // save PTS
            payload--;
            *hdr_ptr=*inbuf_ptr;
            hdr_ptr++;
            if (!--headerLength) {
              prevPTS = newPTS;
              if (validPTS)
                newPTS=GET_MPEG2_PTS(header)/90;
              else newPTS=0;

              state=STREAM;
              HDRDEB("ID: 0x%x PTS: %lld DTS: %lld ESCR: %lld\n",streamID,
                GET_MPEG2_PTS(header),GET_MPEG2_PTS((header+5)),
                GET_MPEG2_PTS((header+10)) );    
            }
            break;

      case STREAM:
        streamlen=min(payload,size); // Max data that is avaiable
        len=DecodeData(inbuf_ptr,streamlen);
        payload-=len;
        if (payload<=0)
          state=UNSYNCED;
        break;
      default:
        state=UNSYNCED;
    }
    size -= len;
    inbuf_ptr += len;
  }
  return Length;
}

void cStreamDecoder::Write(uchar *Data, int Length)
{
  int p;

  p = ringBuffer->Put(Data, Length);
#define BLOCK_BUFFER
#ifdef BLOCK_BUFFER
  while (p != Length)
  {
    //printf("ERROR: ring buffer overflow (%d bytes dropped)\n", Length - p);
    //printf("[mpegdecoder] Videobuffer is full. This should not happen! Audio is playing too slow\n");
    Length -= p;
    Data += p;
    p = ringBuffer->Put(Data, Length);
    usleep(40000);
  }
#endif
}

void cStreamDecoder::Action()
{
  //printf("Neuer Thread gestartet: pid:%d ID: 0x%x\n",getpid(),streamID);
  int size=0;
  running=true;
  active=true;

  while ( (size< 4*1024) && active ) {
    size = ringBuffer->Available();
    usleep(5000);
    //printf("buffer not filled ID 0x%x size: %d - wating\n",streamID,size);
  };

  while(active)
  {

    while (freezeMode)
        usleep(10000);

    mutex.Lock();
    uchar *u =ringBuffer->Get(size);
    BUFDEB("ringBuffer streamID 0x%x Get %d \n",streamID,size);
    if (u)
    {
      if (size>4*1024)
         size=4*1024;

      size=ParseStream(u,size);
      ringBuffer->Del(size);
      mutex.Unlock();
    }
    else
    {
      mutex.Unlock();
      usleep(1000);
    }
  }
  running=false;
  //printf("Thread beendet ID: 0x%x \n",streamID);
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
  printf("[mpegdecoder] Stream 0x%0x Clear %d \n",streamID,(ringBuffer->Free()*100)/ ringBuffer->Size() );
  mutex.Lock();
  avcodec_flush_buffers(context);
  ClearParseStream();
  ringBuffer->Clear();
  mutex.Unlock();
  printf("[mpegdecoder] Stream 0x%0x Clear %d \n",streamID,(ringBuffer->Free()*100)/ ringBuffer->Size() );
}

int cStreamDecoder::BufferFill()
{
  //fprintf(stderr,"ringBuffer free %d %%\n",(ringBuffer->Free()*100)/ ringBuffer->Size() );
  return ( ringBuffer->Available() ) *100/DVB_BUF_SIZE ;
}


// --- AUDIO ------------------------------------------------------------------
cAudioStreamDecoder::cAudioStreamDecoder(unsigned int StreamID,
                                         cAudioOut *AudioOut)
                                         : cStreamDecoder(StreamID)
{
  audioOut=AudioOut;
  codec = avcodec_find_decoder(CODEC_ID_MP2);
  if (!codec)
  {
    printf("[mpegdecoder] Fatal error! Audio codec not found\n");
    exit(1);
  }
  context=avcodec_alloc_context();
  audiosamples=(uint8_t *)malloc(AVCODEC_MAX_AUDIO_FRAME_SIZE);
  if (avcodec_open(context, codec) < 0)
  {
    printf("[mpegdecoder] Fatal error! Could not open audio codec\n");
    exit(1);
  }
}

/* ---------------------------------------------------------------------------
 */
uint64_t cAudioStreamDecoder::GetPTS()
{
  return pts - audioOut->GetDelay() + setupStore.avOffset;
}

/* ---------------------------------------------------------------------------
 */
void cAudioStreamDecoder::setStreamId(int id)
{
  /* -------------------------------------------------------------------------
   * don't hook on DD stream
   */
  if (id != 0x01bd)
  {
    streamID = id;
  }
}

/* ---------------------------------------------------------------------------
 */
int cAudioStreamDecoder::DecodeData(uchar *Data, int Length)
{
    int len;
    int audio_size;

  len=avcodec_decode_audio(context, (short *)audiosamples, &audio_size, Data, Length);
  frame++;
  MPGDEB("count: %d  Length: %d len: %d a_size: %d a_delay: %d\n",
      frame,Length,len, audio_size, audioOut->GetDelay());

  // no new frame decoded, return
  if (audio_size == 0)
    return len;

  if (validPTS)
  {
    validPTS=false;
    MPGDEB("valid PTS : %lld pts - valid PTS: %lld \n",
      GET_MPEG2_PTS(header)/90,(int) pts - GET_MPEG2_PTS(header)/90);
    pts = newPTS;
  }


  audioOut->SetParams(context->channels,context->sample_rate);
  audioOut->Write(audiosamples,audio_size);
  // adjust PTS according to audio_size, sampel_rate and no. of channels  
  pts += (audio_size/(context->sample_rate/1000*2*context->channels)); 
  return len;
}


cAudioStreamDecoder::~cAudioStreamDecoder()
{
  Cancel(3);
  avcodec_close(context);
  free(context);
  free(audiosamples);
}

// --- VIDEO ------------------------------------------------------------------

cVideoStreamDecoder::cVideoStreamDecoder(unsigned int StreamID,
                                         cVideoOut *VideoOut, cAudioStreamDecoder *AudioStreamDecoder)
                                         : cStreamDecoder(StreamID)
{
  width = height = -1;
  workBackIndex = currentBackIndex = 4;
  newBackIndex = 0;
  backIndexTab[0] = 0;
  backIndexTab[1] = 1;
  backIndexTab[2] = 2;
  backIndexTab[3] = 2;
  backIndexTab[4] = 4;
  pic_buf_lavc = pic_buf_mirror = pic_buf_pp = NULL;
  currentMirrorMode  = setupStore.mirror;
  currentDeintMethod = setupStore.deintMethod;

#ifdef PP_LIBAVCODEC
  ppmode = ppcontext = NULL;
#endif //PP_LIBAVCODEC
  codec = avcodec_find_decoder(CODEC_ID_MPEG2VIDEO);
  if (!codec)
  {
    printf("[mpegdecoder] Fatal error! Video codec not found\n");
    exit(1);
  }
  videoOut = VideoOut;

  // init A-V syncing variables
  frametime=DEFAULT_FRAMETIME;
  syncOnAudio=1;
  offset=0;
  delay=0;
  (void) GetRelTime();
  AudioStream=AudioStreamDecoder;

  if ( (rtc_fd = open("/dev/rtc",O_RDONLY)) < 0 ) 
    fprintf(stderr,"Could not open /dev/rtc \n");
  else 
  {
    uint64_t irqp = 1024;

    if ( ioctl(rtc_fd, RTC_IRQP_SET, irqp) < 0) 
    {
      fprintf(stderr,"Could not set irq period\n");
      close(rtc_fd);
      rtc_fd=-1;
    }
    else if ( ioctl( rtc_fd, RTC_PIE_ON, 0 ) < 0) 
    {
      fprintf(stderr,"Error in rtc_pie on \n");
      close(rtc_fd);
      rtc_fd=-1;
    }// else  fprintf(stderr,"Set up to use linux RTC\n");
 };

  context=avcodec_alloc_context();
  picture=avcodec_alloc_frame();
  if(codec->capabilities&CODEC_CAP_TRUNCATED)
    context->flags|= CODEC_FLAG_TRUNCATED; /* we dont send complete frames */
  if (avcodec_open(context, codec) < 0)
  {
    printf("[mpegdecoder] Fatal error! Could not open video codec\n");
    exit(1);
  }
}

int32_t cVideoStreamDecoder::GetRelTime() 
{
  struct timeval tv;
  struct timezone tz;
  int64_t now;
  int32_t ret;

  gettimeofday(&tv,&tz);
  now=tv.tv_sec*1000000+tv.tv_usec;
  if ( now < lastTime ) {
    ret = (uint32_t) (now - lastTime + 60 *1000000); // untested
    MPGDEB("now %lld kleiner als lastTime %lld\n",now,lastTime);
  }
  else ret = now - lastTime;
  lastTime=now;
  return ret;
};

void cVideoStreamDecoder::resetCodec(void)
{
  //printf("[mpegdecoder] resetting codec\n");
  avcodec_close(context);
  if(codec->capabilities&CODEC_CAP_TRUNCATED)
    context->flags|= CODEC_FLAG_TRUNCATED; /* we dont send complete frames */
  if (avcodec_open(context, codec) < 0)
  {
    printf("[mpegdecoder] Fatal error! Could not open video codec\n");
    exit(1);
  }
}

int cVideoStreamDecoder::StillPicture(uchar *Data, int Length)
{
  mutex.Lock();
  ringBuffer->Clear();
  avcodec_flush_buffers(context);
  mutex.Unlock();
  for (int i = 0;i < 4;++i)
    ParseStreamIntern(Data,Length,0x000001E0,0x000001EF);

  return Length;
}

int cVideoStreamDecoder::DecodeData(uchar *Data, int Length)
{
    int len;
    int got_picture;

  len = avcodec_decode_video(context, picture, &got_picture,Data, Length);
  if (len < 0)
  {
    printf("[mpegdecoder] Error while decoding frame %d\n", frame);
    resetCodec();
    return Length;
  }
  if (!got_picture)
    return len;

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

/*
 * A. recording sequence where only I-frames have pts values defined
 * B. recording sequence where each frame has a pts value defined
 * Lines marked with '*' show when we got a I-frame pts values
 * before it it passed to the decoder.
 * Lines marked with '+' are the ones when this I-frames is passed
 * to us for displaying.
 */

/* sequence A.
 V(0) NPTS(65785272) PPTS(0)        P(2) P2(1) CF(1)  CF2(0)
 V(0) NPTS(65785272) PPTS(0)        P(3) P2(3) CF(2)  CF2(2)
 V(0) NPTS(65785272) PPTS(0)        P(3) P2(3) CF(3)  CF2(3)
 V(0) NPTS(65785272) PPTS(0)        P(2) P2(2) CF(4)  CF2(1)
 V(0) NPTS(65785272) PPTS(0)        P(3) P2(3) CF(5)  CF2(5)
 V(0) NPTS(65785272) PPTS(0)        P(3) P2(3) CF(6)  CF2(6)
 V(0) NPTS(65785272) PPTS(0)        P(2) P2(2) CF(7)  CF2(4)
 V(0) NPTS(65785272) PPTS(0)        P(3) P2(3) CF(8)  CF2(8)
*V(1) NPTS(65785752) PPTS(65785272) P(3) P2(3) CF(9)  CF2(9)
 V(0) NPTS(65785752) PPTS(65785272) P(1) P2(2) CF(10) CF2(7)
 V(0) NPTS(65785752) PPTS(65785272) P(3) P2(3) CF(11) CF2(11)
 V(0) NPTS(65785752) PPTS(65785272) P(3) P2(3) CF(12) CF2(12)
+V(0) NPTS(65785752) PPTS(65785272) P(2) P2(1) CF(13) CF2(10)
*/

/* sequence B.
 V(1) NPTS(69692924) PPTS(69693004) P(2) P2(1) CF(1)  CF2(0)
 V(1) NPTS(69692964) PPTS(69692924) P(3) P2(3) CF(2)  CF2(2)
 V(1) NPTS(69693124) PPTS(69692964) P(3) P2(3) CF(3)  CF2(3)
 V(1) NPTS(69693044) PPTS(69693124) P(2) P2(2) CF(4)  CF2(1)
 V(1) NPTS(69693084) PPTS(69693044) P(3) P2(3) CF(5)  CF2(5)
 V(1) NPTS(69693244) PPTS(69693084) P(3) P2(3) CF(6)  CF2(6)
 V(1) NPTS(69693164) PPTS(69693244) P(2) P2(2) CF(7)  CF2(4)
 V(1) NPTS(69693204) PPTS(69693164) P(3) P2(3) CF(8)  CF2(8)
*V(1) NPTS(69693364) PPTS(69693204) P(3) P2(3) CF(9)  CF2(9)
 V(1) NPTS(69693284) PPTS(69693364) P(1) P2(2) CF(10) CF2(7)
 V(1) NPTS(69693324) PPTS(69693284) P(3) P2(3) CF(11) CF2(11)
 V(1) NPTS(69693484) PPTS(69693324) P(3) P2(3) CF(12) CF2(12)
+V(1) NPTS(69693404) PPTS(69693484) P(2) P2(1) CF(13) CF2(10)
*/

#if 0
  fprintf(stderr,
          " V(%d) NPTS(%lld) PPTS(%lld) P(%d) P2(%d) CF(%d) CF2(%d)\n",
          validPTS,
          newPTS,
          prevPTS,
          context->coded_frame->pict_type,
          picture->pict_type,
          context->coded_frame->coded_picture_number,
          picture->coded_picture_number);
#endif

  if (context->coded_frame->pict_type == FF_I_TYPE)
  {
    newBackIndex = 1;
  }
  else
  {
    newBackIndex++;
  }

  if (picture->coded_picture_number)
  {
    if (picture->pict_type == FF_I_TYPE)
    {
      if (newBackIndex > 0 && newBackIndex <=4)
      {
        if (newBackIndex != currentBackIndex)
        {
          dsyslog("[mpeg2decoder]: back index change (%d -> %d(%d))\n",
                  currentBackIndex,
                  newBackIndex,
                  backIndexTab[newBackIndex]);
        }
        currentBackIndex = newBackIndex;
        workBackIndex = backIndexTab [currentBackIndex];
      }
#if 0
      fprintf (stderr, " changing PTS from %lld to %lld. delta %lld\n",
               pts, historyPTS[(historyPTSIndex+PTS_COUNT-4)%PTS_COUNT],
               historyPTS[(historyPTSIndex+PTS_COUNT-workBackIndex)%PTS_COUNT]-pts);
#endif
      pts = historyPTS[(historyPTSIndex+PTS_COUNT-workBackIndex)%PTS_COUNT];
      //fprintf (stderr, "* using PTS value (%lld)\n", pts);
    }
  }
  else
  {
    /**
     * without subtracting 4 frame times I'll get deltas (see above)
     * upon 2nd PTS setting in the range 0 to -160 ms. So this should
     * change that 2nd deltas to -80 to +80 .
     */
    pts = newPTS - 2 * frametime;
    //fprintf (stderr, "+ using PTS value (%lld)\n", pts);
  }

  historyPTS[historyPTSIndex++%PTS_COUNT] = newPTS;

  // this few lines does the whole syncing
  int pts_corr;

  // calculate pts correction. Max. correction is 1/10 frametime.
  pts_corr = offset * 100;
  if (pts_corr > (frametime*1000)/10)
    pts_corr = frametime*100;
  else if (pts_corr < -frametime*100)
    pts_corr =-frametime*100;

  // calculate delay
  delay += ( frametime *1000 - pts_corr ) ;
  //delay = ( frametime *1000 - pts_corr ) ;
  if (delay > 2*frametime*1000)
    delay = 2*frametime*1000;
  else if (delay < -2*frametime*1000)
    delay = -2*frametime*1000;    
  delay-=GetRelTime();
   
  // prepare picture for display
  videoOut->CheckAspectDimensions(picture,context);

  MPGDEB("Frame# %-5d  aPTS: %lld offset: %d delay %d \n",frame,AudioStream->GetPTS(),offset,delay );

  if ( rtc_fd >= 0 ) {
    // RTC timinig
    while (delay > 15000) {
      //sleep one timer tick
      usleep(10000);
      //MPGDEB("RTC sleep loop %d \n",delay);
      delay-=GetRelTime();
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
      delay-=GetRelTime();
    }
  } else {
    // usleep timing
    const int rest=2200;
    while (delay > rest) {     
      //usleep(1000);
      usleep(rest);
      delay  -= GetRelTime();
      //MPGDEB("Loop %d \n",delay);  return len;
    }
    // cpu burn timing
    //while (delay > 100) { 
    //  delay  -= GetRelTime();
    //   printf("Loop2 %d \n",delay);
    //};
  }
  // display picture
  videoOut->YUV(picture->data[0], picture->data[1],picture->data[2],
      context->width,context->height,
      picture->linesize[0],picture->linesize[1]);

  // we just displayed a frame, now it's the right time to
  // measure the A-V offset
  // the A-V syncing code is partly based on MPlayer...
  uint64_t aPTS = AudioStream->GetPTS();

  if ( syncOnAudio && aPTS )
    offset = aPTS - pts ;
  else offset = 0;

#ifdef AV_STATS
  {
    const int Freq=10;
    static float offsetSum=0;
    static float offsetSqSum=0;
    static int StatCount=0;

    offsetSum+=(float)offset;
    offsetSqSum+=(float) offset*offset;
    StatCount+=1;

    if ( (StatCount % Freq) == 0 )
    {
      printf("A-V: %02.2f+-%02.2f \n",offsetSum/(float)Freq,
       sqrt(offsetSqSum/((float)Freq)-(offsetSum*offsetSum)/((float)Freq*Freq)));
      offsetSum=offsetSqSum=0;
      StatCount=0;
    };
  };
#endif

#if 1
  if (!(frame % 1) || context->hurry_up) {
    int dispTime=GetRelTime();
    MPGDEB("Frame# %-5d A-V(ms) %-5d delay %d FrameT: %s, dispTime(ms): %1.2f\n",
      frame,(int)(AudioStream->GetPTS()-pts),delay,
      (context->coded_frame->pict_type == FF_I_TYPE ? "I_t":
      (context->coded_frame->pict_type == FF_P_TYPE ? "P_t":
      (context->coded_frame->pict_type == FF_B_TYPE ? "B_t":"other"
      )))
      ,(float)dispTime/1000 );
    delay-=dispTime;
  }
#endif

  // update video pts
  pts += frametime;
  frame++;
  return len;
}

void cVideoStreamDecoder::TrickSpeed(int Speed)
{
  frametime = DEFAULT_FRAMETIME * Speed;
  syncOnAudio = ( Speed == 1);
}

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
  if (currentDeintMethod != deintWork)
  {
#if FB_SUPPORT
    if (currentDeintMethod > 2)
#else
    if (currentDeintMethod > 1)
#endif
    {
      if (ppmode)
      {
        pp_free_mode (ppmode);
        ppmode = NULL;
      }
    }
    ppmode = pp_get_mode_by_name_and_quality(setupStore.getPPValue(), 6);
    currentDeintMethod = deintWork;
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


cVideoStreamDecoder::~cVideoStreamDecoder()
{
  active = false;

  //RTC
  if (rtc_fd)
    close(rtc_fd);

  delete(ringBuffer);
  avcodec_close(context);
  free(context);
  free(picture);
}

//----------------------------   create a new MPEG Decoder


cMpeg2Decoder::cMpeg2Decoder(cAudioOut *AudioOut, cVideoOut *VideoOut)
{
  avcodec_init();
  avcodec_register_all();
  state=UNSYNCED;
  audioOut=AudioOut;
  videoOut=VideoOut;
  aout=vout=0;

  ac3Mode = ac3Parm = lpcmMode = 0;

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

void cMpeg2Decoder::Start(void)
{
  CMDDEB("Start IsSuspended %d \n",IsSuspended);
  if (IsSuspended)
    // don't start if we are suspended
    return;

  // ich weiß nicht, ob man's so kompliziert machen soll, aber jetzt hab ich's
  // schon so gemacht, das 2 Threads gestartet werden.
  // Audio is the master, Video syncs on Audio
  ac3Mode = ac3Parm = lpcmMode = 0;
  aout = new cAudioStreamDecoder( 0x000001C0, audioOut );
  vout = new cVideoStreamDecoder( 0x000001E0, videoOut,(cAudioStreamDecoder *) aout);
  running=true;
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
  if (running) 
  {
    aout->Play();
    vout->Play();
  };
};

void cMpeg2Decoder::Freeze(void)
{
  CMDDEB("Freeze\n");
  if (running) 
  {
    aout->Freeze();
    vout->Freeze();
  };
};
  
void cMpeg2Decoder::Stop(void)
{
  CMDDEB("Stop\n");
  if (running)
  {
    running=false;
    while (decoding)
      usleep(1);

    if (vout)
    {
      vout->Stop();
      delete(vout);
    }

    if (aout)
    {
      aout->Stop();
      delete(aout);
    }
    aout=vout=0;
  }
}

/* ----------------------------------------------------------------------------
 */
int cMpeg2Decoder::StillPicture(uchar *Data, int Length)
{
  return (running) ? vout->StillPicture(Data,Length): Length;
}

/* ----------------------------------------------------------------------------
 */
void cMpeg2Decoder::Clear(void)
{
  CMDDEB("Clear\n");
  if (running)
  {
    aout->Clear();
    vout->Clear();
  }
}

/* ----------------------------------------------------------------------------
 */
void cMpeg2Decoder::TrickSpeed(int Speed)
{
  CMDDEB("TrickSpeed %d\n",Speed);
  if (running)
  {
    aout->TrickSpeed(Speed);
    vout->TrickSpeed(Speed);
  };
}

/* ----------------------------------------------------------------------------
 */
int64_t cMpeg2Decoder::GetSTC(void) {
  if (running)
    return vout->GetPTS()*90;
  else return 0;
};

/* ----------------------------------------------------------------------------
 */
bool cMpeg2Decoder::BufferFilled()
{
  if (running)
    return vout->BufferFill()>95;
  else return false;
}

/* ----------------------------------------------------------------------------
 */
int cMpeg2Decoder::PlayAudio(const uchar *Data, int Length)
{
    const uchar *p;
    int         ac3ModeNew, ac3ParmNew, lpcmModeNew;

  ac3ModeNew = ac3Mode;
  ac3ParmNew = ac3Parm;
  lpcmModeNew = lpcmMode;

  p = Data;
  /* -------------------------------------------------------------------------
   * 1st check enshure that this is private stream
   */
  if (p[0] == 0x00 && p[1] == 0x00 && p[2] == 0x01 && p[3] == 0xbd)
  {
    p += 4 + 2 + 2;   // skip: stream id, length, ??
    p += *p;
    p++;

    /* ------------------------------------------------------------------------
     * check for ac3 sync word
     */
    if (p[0] == 0x0b && p[1] == 0x77)
    {
      p += 2 + 2 + 1;   // skip: sync word, crc, bitrate & frequency
      ac3ModeNew = (p[1] & 0xe0) >> 5;
      ac3ParmNew = ((p[1] & 0x01) << 1) | ((p[2] & 0x80) >> 7);
      lpcmModeNew = 0;
    }
    /* ------------------------------------------------------------------------
     * check for LPCM sync word
     */
    else if (p[0] == 0xa0 && p[1] == 0xff)
    {
      ac3ModeNew = ac3ParmNew = 0;
      lpcmModeNew = 1;
      p += 2 + 2 + 1;
    }
  }

  if (ac3ModeNew != ac3Mode || ac3ParmNew != ac3Parm)
  {
      char *info = NULL;

    fprintf (stderr,
             "[Softdevice] AC3 changed Mode(%d -> %d) Parm(%d -> %d)\n",
             ac3Mode, ac3ModeNew, ac3Parm, ac3ParmNew);
    ac3Mode = ac3ModeNew;
    ac3Parm = ac3ParmNew;

    info = "Off";
    if (ac3Mode == 0x02)
    {
      info = "2.0 Stereo";
      if (ac3Parm == 0x02)
        info = "2.0 Stereo Dolby Surround";
    } else if (ac3Mode == 0x07)
      info = "5.1 Dolby Digital";

    dsyslog ("[Mpeg2Decoder]: AC3 info: %s", info);
  }

  if (lpcmModeNew != lpcmMode)
  {
      char  *sampleRates [4] = {"48000", "96000", "44100", "32000"};

    dsyslog ("[Mpeg2Decoder]: LPCM info: %dch@%sHz",
             ((*p)&1)+1, sampleRates[((*p)&3)>>4]);
  }
#if VDRVERSNUM >= 10318
  if (running)
  {
    aout->Write((uchar *)Data,Length);
    aout->setStreamId(Data[2]<<8|Data[3]);
  }
#endif
  return Length;
}

/* ----------------------------------------------------------------------------
 */
int cMpeg2Decoder::Decode(const uchar *Data, int Length)
{
    int len, streamlen;
    uint8_t *inbuf_ptr;
    inbuf_ptr = (uint8_t *)Data;
    int size=Length;

  if (running && !IsSuspended && setupStore.shouldSuspend)
     // still running and should suspend
     Suspend();

  if (!running && IsSuspended && !setupStore.shouldSuspend)
     // not running and should resume
     Resume();
     
  if (!running)
    return Length;

  decoding=true;
  while (size > 0)
  {
    len=1;
    switch (state)
    {
      case UNSYNCED:
        syncword = (syncword << 8) | *inbuf_ptr;
        if ( (syncword >= 0x000001E0) && (syncword <= 0x000001EF) )
        {
          state=PAYLOAD;
          //streamtype=syncword & 0x000000FF;
          streamtype=0xE0;
        }
        else if ( (syncword >= 0x000001C0) && (syncword <= 0x000001CF) )
        {
          state=PAYLOAD;
          //streamtype=syncword & 0x000000FF;
          streamtype=0xc0;
        }
        break;
      case PAYLOAD:		//Payload length(hi)
        payload = *inbuf_ptr << 8;
        state++;
        break;
      case PAYLOAD+1:		//Payload length(lo)
        payload += *inbuf_ptr;
        header[0]=0;
        header[1]=0;
        header[2]=1;
        header[3]=streamtype;
        header[4]=payload >> 8;
        header[5]=payload & 0xFF;
        if (streamtype >= 0xE0 && streamtype <= 0xEF) {
          vout->Write(header,6);
        }
#if VDRVERSNUM < 10318
        if (streamtype >= 0xC0 && streamtype <= 0xCF) {
          aout->Write(header,6);
        }
#endif
        state=PAYLOADDATA;
        break;
      case PAYLOADDATA:
        streamlen =min(payload,size);
        if (streamtype == 0xE0)
          vout->Write(inbuf_ptr,streamlen);
#if VDRVERSNUM < 10318
        if (streamtype == 0xC0)
          aout->Write(inbuf_ptr,streamlen);
#endif
        payload-=streamlen;
        len = streamlen;
        if (payload <= 0)
          state=UNSYNCED;
        break;
    }
    size -= len;
    inbuf_ptr += len;
  }
  decoding=false;
  return Length;
}
