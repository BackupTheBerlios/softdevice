/*
 * mpeg2decoder.c: A plugin for the Video Disk Recorder
 *
 * See the README file for copyright information and how to reach the author.
 *
 * $Id: mpeg2decoder.c,v 1.6 2004/10/29 16:41:39 iampivot Exp $
 */

#include <vdr/plugin.h>

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




#define GET_MPEG2_PTS(x)   ( ((uint64_t)x[4]&0xFE) >>1     | \
                             ((uint64_t)x[3])      <<7     | \
                             ((uint64_t)x[2]&0xFE) <<14    | \
                             ((uint64_t)x[1])      <<22    | \
                             ((uint64_t)x[0]&0x0E) <<29 )


// --- cStreamDecoder ---------------------------------------------------------

cStreamDecoder::cStreamDecoder(unsigned int StreamID)
{
  streamID = StreamID;
  frame=0;
}



cStreamDecoder::~cStreamDecoder()
{
}

void cStreamDecoder::SyncPTS(uint64_t spts)
{
    int diff= (int)(pts-spts);

  if (abs(diff) > 100)
    pts=spts;
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
        payload--;
        break;

      case HEADER+3:      // ??
        state++;
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
        }
        payload--;
        break;

      case OPTHEADER:     // save PTS
        payload--;
        *hdr_ptr=*inbuf_ptr;
        hdr_ptr++;
        if (!--headerLength)
          state=STREAM;
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

// --- AUDIO ------------------------------------------------------------------
cAudioStreamDecoder::cAudioStreamDecoder(unsigned int StreamID,
                                         cAudioOut *AudioOut,
                                         uint64_t *commonPTS)
                                          : cStreamDecoder(StreamID)
{
  audioOut=AudioOut;
  cPTS=commonPTS;
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

void cAudioStreamDecoder::Write(uchar *Data, int Length)
{
    int size = Length;

  while (size > 0)
  {
      int len = ParseStream(Data,size);

    Data += len;
    size -= len;
  }
}

int cAudioStreamDecoder::DecodeData(uchar *Data, int Length)
{
    int len;
    int audio_size;

  len=avcodec_decode_audio(context, (short *)audiosamples, &audio_size, Data, Length);

  if (audio_size > 0)
  {
    //fwrite(audiosamples,1,audio_size,fo);
    audioOut->SetParams(context->channels,context->sample_rate);
    audioOut->Write(audiosamples,audio_size);
    int delay = audioOut->GetDelay();
    if (delay < 20)// if we have less than 20 ms in buffer we double frames
      audioOut->Write(audiosamples,audio_size);

    pts += (audio_size/(48*4)); // PTS weiterzählen, egal ob Samples gespielt oder nicht

    //  printf("Audiodelay: %d \n",delay);
    *cPTS = pts - delay + setupStore.avOffset; // Das ist die Master-PTS die wird an den video Teil übergeben,
    // damit Video syncronisieren kann
    if (validPTS)
      SyncPTS(GET_MPEG2_PTS(header)/90); // milisekunden
  }
  return len;
}


cAudioStreamDecoder::~cAudioStreamDecoder()
{
  avcodec_close(context);
  free(context);
  free(audiosamples);
}

// --- VIDEO ------------------------------------------------------------------

cVideoStreamDecoder::cVideoStreamDecoder(unsigned int StreamID,
                                         cVideoOut *VideoOut,
                                         uint64_t *commonPTS)
                                          : cStreamDecoder(StreamID)
{
  width = height = -1;
  pic_buf_lavc = pic_buf_mirror = pic_buf_pp = NULL;
  currentMirrorMode  = setupStore.mirror;
  currentDeintMethod = setupStore.deintMethod;

#ifdef PP_LIBAVCODEC
  ppmode = ppcontext = NULL;
#endif //PP_LIBAVCODEC
  codec = avcodec_find_decoder(CODEC_ID_MPEG2VIDEO);
  avgOffset=0;
  if (!codec)
  {
    printf("[mpegdecoder] Fatal error! Video codec not found\n");
    exit(1);
  }
  videoOut = VideoOut;
  cPTS=commonPTS;
  //syncdevice = SyncDevice;
  context=avcodec_alloc_context();
  picture=avcodec_alloc_frame();
  if(codec->capabilities&CODEC_CAP_TRUNCATED)
    context->flags|= CODEC_FLAG_TRUNCATED; /* we dont send complete frames */
  if (avcodec_open(context, codec) < 0)
  {
    printf("[mpegdecoder] Fatal error! Could not open video codec\n");
    exit(1);
  }
  ringBuffer=new cRingBufferLinear(1024*1024,1024,true);
  //ringBuffer->SetTimeouts(100,10);
  Start(); // starte thread
}


void cVideoStreamDecoder::Write(uchar *Data, int Length)
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
    usleep(1);
  }
#endif
}


void cVideoStreamDecoder::Action()
{
//    printf("Neuer Thread geatartet: pid:%d\n",getpid());
  running=true;
  active=true;
  while(active)
  {
      int size=10240;

    mutex.Lock();
    uchar *u =ringBuffer->Get(size);
    if (u)
    {
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
//    printf("Thread beendet\n");
}


void cVideoStreamDecoder::resetCodec(void)
{
  printf("[mpegdecoder] resetting codec\n");
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
  if (got_picture)
  {
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

    if (validPTS &&
        (setupStore.syncOnFrames ||
         context->coded_frame->pict_type == FF_I_TYPE))
    {
      pts=(GET_MPEG2_PTS(header)/90);
    }

    // this few lines does the whole syncing
    int offset = *cPTS - pts;
    if (offset > 1000) offset = 1000;
    if (offset < -1000) offset = -1000;

    avgOffset = (int)( ( (24*avgOffset) + offset ) / 25);
    if (avgOffset > 1000) avgOffset = offset;
    if (avgOffset < -1000) avgOffset = offset;

    int frametime = 40; //ms
    if (avgOffset > 20 && offset > 0) {
      frametime -= (avgOffset / 10);
    }
    if (avgOffset < -20 && offset < 0) {
      frametime -= (avgOffset / 10);
    }
#if 0
    if (!(frame % 10) || context->hurry_up) {
      printf ("[mpegdecoder] Frame# %-5d A-V(ms) %-5d(Avg:%-4d) Frame Time:%-3d ms Hurry: %d\n",frame,(int)(*cPTS-pts),avgOffset, frametime,context->hurry_up);
    }
#endif
    //frametime=40;
    pts += 40;
    vpts += frametime; // 40 ms for PAL   // FIXME FOR NTSC

    int delay;
    delay = vpts - getTimeMilis();
    if (delay < -1000)
    {
      vpts = getTimeMilis();
      context->hurry_up++;
    }
    else
    {
      if (delay > 1000)
      {
        vpts = getTimeMilis();
      }
      else
      {
        while (delay > 0)
        {
          usleep(1000);
          delay = vpts - getTimeMilis();
        }
      }
      if (context->hurry_up)
        context->hurry_up--;
      if (context->hurry_up < 3)
      {
          videoOut->CheckAspectDimensions(picture,context);
          videoOut->YUV(picture->data[0], picture->data[1],picture->data[2],
                        context->width,context->height,
                        picture->linesize[0],picture->linesize[1]);
      }
    }
    frame++;
  }
  return len;
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
  Cancel(3);
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
  running=false;
  decoding=false;
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
  // ich weiß nicht, ob man's so kompliziert machen soll, aber jetzt hab ich's
  // schon so gemacht, das 2 Threads gestartet werden.
  // Audio is the master, Video syncs on Audio
  aout = new cAudioStreamDecoder( 0x000001C0, audioOut, &commonPTS);
  vout = new cVideoStreamDecoder( 0x000001E0, videoOut, &commonPTS);
  running=true;
}

void cMpeg2Decoder::Stop(void)
{
  if (running)
  {
    running=false;
    while (decoding)
      usleep(1);

    if (vout)
    {
    //	    vout->Stop();
      delete(vout);
    }

    if (aout)
    {
    //	    aout->Stop();
      delete(aout);
    }
    aout=vout=0;
  }
}

int cMpeg2Decoder::StillPicture(uchar *Data, int Length)
{
  return vout->StillPicture(Data,Length);
}

int cMpeg2Decoder::Decode(const uchar *Data, int Length)
{
    int len, streamlen;
    uint8_t *inbuf_ptr;
    inbuf_ptr = (uint8_t *)Data;
    int size=Length;

  decoding=true;
  if (!running)
  {
    decoding=false;
    return Length;
  }

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
          streamtype=syncword & 0x000000FF;
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
        if (streamtype >= 0xC0 && streamtype <= 0xCF) {
          aout->Write(header,6);
        }
        state=PAYLOADDATA;
        break;
      case PAYLOADDATA:
        streamlen =min(payload,size);
        if (streamtype == 0xE0)
          vout->Write(inbuf_ptr,streamlen);
        if (streamtype == 0xC0)
          aout->Write(inbuf_ptr,streamlen);
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
