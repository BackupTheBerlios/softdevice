/*
 * mpeg2decoder.h: A plugin for the Video Disk Recorder
 *
 * See the README file for copyright information and how to reach the author.
 *
 * $Id: mpeg2decoder.h,v 1.15 2005/03/17 20:15:35 wachm Exp $
 */
#ifndef MPEG2DECODER_H
#define MPEG2DECODER_H
#include <avcodec.h>
#ifdef PP_LIBAVCODEC
  #include <postproc/postprocess.h>
#endif //PP_LIBAVCODEC
#include "video.h"
#include "audio.h"
#include <avformat.h>
#include <sys/time.h>
#include <vdr/plugin.h>
#include <vdr/ringbuffer.h>

#define DEFAULT_FRAMETIME 40   // for PAL
#define DVB_BUF_SIZE   (64*1024)  // same value as in dvbplayer.c

#define NO_STREAM    -1
#define DONT_PLAY  -100

class cAudioStreamDecoder; 

// -----------------cClock --------------------------------------------
class cClock {
    static cAudioStreamDecoder *audioClock;

public:
    cClock() {audioClock=NULL;};
    virtual ~cClock() {};
    static void SetAudioClock(cAudioStreamDecoder *AudioClock)
    {audioClock=AudioClock;};
    virtual uint64_t GetPTS();
};	

// -----------------cPacketQueue -----------------------------------------
class cPacketQueue {
    // Only one thread may read, and only one thread may write!!!
public:
   const static int MaxPackets=500;
private:
    AVPacket queue[MaxPackets];
    int FirstPacket,LastPacket;

    inline int Next(int Packet) 
    { return (Packet+1)%MaxPackets; };
    
public:
    cPacketQueue();
    ~cPacketQueue() {};

    int PutPacket(const AVPacket &Packet);

    AVPacket * GetReadPacket();
    void FreeReadPacket(AVPacket *Packet);

    void Clear();
    inline int Available()
    { return (LastPacket+MaxPackets - FirstPacket)%MaxPackets; }; 
};
 
//--------------------------cSoftRingBufferLinear --------------------------
// wrapper class to access protected methods
class cSoftRingBufferLinear : public cRingBufferLinear {
public:
  cSoftRingBufferLinear(int Size, int Margin = 0, bool Statistics = false) 
     : cRingBufferLinear(Size,Margin,Statistics) {};
  ~cSoftRingBufferLinear() {};

  virtual int Available(void) {return cRingBufferLinear::Available();} ;
  virtual int Free(void) {return cRingBufferLinear::Free();} ;
  int Size(void) { return cRingBufferLinear::Size(); }
};

//-------------------------cRelTimer-----------------------------------
class cRelTimer {
   private:
      int64_t lastTime;
      inline int64_t GetTime()
      {  
        struct timeval tv;
        struct timezone tz;
        gettimeofday(&tv,&tz);
        return tv.tv_sec*1000000+tv.tv_usec;
      };

   public:
      cRelTimer() {lastTime=GetTime();};
      ~cRelTimer() {};
      
      int32_t TimePassed();
      int32_t GetRelTime();
      inline void Reset() { lastTime=GetTime(); };
};
      
//-------------------------cStreamDecoder ----------------------------------
// Output device handler
class cStreamDecoder : public cThread {
private:

    bool freezeMode;
    cPacketQueue PacketQueue;
protected:
    int64_t               pts;
    int                   frame;
    
    AVCodec               *codec;
    AVCodecContext        *context;
    
    cMutex                mutex;
    bool                  active, running;
    
    virtual void Action(void);
    virtual int DecodePacket(AVPacket *pkt) = 0;
public:
    inline int PutPacket(const AVPacket &pkt)
    { return PacketQueue.PutPacket(pkt); };

    virtual void Clear(void);
    virtual void Freeze(void);
    virtual void Play(void);
    virtual void Stop();
    virtual void TrickSpeed(int Speed) {return;};
    virtual int StillPicture(uchar *Data, int Length) {return 0;};
    virtual int BufferFill(void);
    bool    initCodec(void);
    void    resetCodec(void);
    virtual uint64_t GetPTS()  {return pts;};
    
    cStreamDecoder(AVCodecContext * Context);
    virtual ~cStreamDecoder();
};

//----------------------------cAudioStreamDecoder ----------------------------
class cAudioStreamDecoder : public cStreamDecoder {
private:
    uint8_t * audiosamples;
    cSoftRingBufferLinear *audioBuffer;
    cAudioOut *audioOut;
    SampleContext audioOutContext;
    int audioMode;

    void OnlyLeft(uint8_t *samples,int Length);
    // copy left data to right channel
    void OnlyRight(uint8_t *samples,int Length);
    // copy right data to left channel
protected:
public:
    virtual int DecodePacket(AVPacket *pkt);
    cAudioStreamDecoder(AVCodecContext *Context, cAudioOut *AudioOut,
       int AudioChannel=0);
    ~cAudioStreamDecoder();
    inline void SetAudioMode(int AudioMode)
      { audioMode=AudioMode; };
    virtual uint64_t GetPTS();
};

//---------------------------cVideoStreamDecoder -----------------------------
class cVideoStreamDecoder : public cStreamDecoder {
  private:
    cClock *clock;
#define NO_PTS_VALUES 10
    struct {
       int coded_frame_no;
       int64_t pts;
    } pts_values[NO_PTS_VALUES];
    int lastPTSidx;
    int lastCodedPictNo;
    int64_t lastPTS;
    AVFrame             *picture;
    AVPicture           avpic_src, avpic_dest;

    int                 width, height;
    int                 currentDeintMethod, currentMirrorMode;
    uchar               *pic_buf_lavc, *pic_buf_pp, *pic_buf_mirror;
#ifdef PP_LIBAVCODEC
    pp_mode_t           *ppmode;
    pp_context_t        *ppcontext;
#endif //PP_LIBAVCODEC

    cVideoOut           *videoOut;

    // A-V syncing stuff
    bool               syncOnAudio;
    cRelTimer          Timer;
    int                offset;
    int                delay;
    int                timePassed;
    int                rtc_fd; 
    int                frametime;
    int32_t GetRelTime();
   
    uchar   *allocatePicBuf(uchar *pic_buf);
    void    deintLibavcodec(void);
    uchar   *freePicBuf(uchar *pic_buf);
#ifdef PP_LIBAVCODEC
    void    ppLibavcodec(void);
#endif //PP_LIBAVCODEC
    void    Mirror(void);

  public:
    cVideoStreamDecoder(AVCodecContext *Context, cVideoOut *VideoOut,
       cClock *clock);
    ~cVideoStreamDecoder();

    virtual int DecodePacket(AVPacket *pkt);
    virtual int StillPicture(uchar *Data, int Length);
    virtual void TrickSpeed(int Speed);
    virtual uint64_t GetPTS();
};


//--------------------------------cMpeg2Decoder -------------------------
class cMpeg2Decoder: public cThread  {
private:
    cVideoStreamDecoder  *vout;
    cAudioStreamDecoder  *aout;
    cAudioOut       *audioOut;
    cVideoOut       *videoOut;
    bool running;
    bool IsSuspended;
    bool decoding;
    bool freezeMode;
    
    AVFormatContext *ic;
    int LastSize;
    cMutex  mutex;
    cSoftRingBufferLinear *StreamBuffer;
    void initStream();
    virtual void Action(void);
    volatile bool   ThreadActive, ThreadRunning;
    cClock  clock;
    //demuxing
    int AudioIdx;
    int VideoIdx;

    int audioMode;
public:
    int read_packet(uint8_t *buf, int buf_size);
    int seek(offset_t offset, int whence);

public:
    cMpeg2Decoder(cAudioOut *AudioOut, cVideoOut *VideoOut);
    ~cMpeg2Decoder();

    void DecodePacket(const AVFormatContext *ic,AVPacket &pkt);
    int Decode(const uchar *Data, int Length);
    int StillPicture(uchar *Data, int Length);
    void Start(bool GetMutex=true);
    void Play(void);
    void Freeze(void);
    void Stop(bool GetMutex=true);
    void Clear(void);
    void Suspend(void);
    void Resume(void);
    void TrickSpeed(int Speed);
    
    void PlayAudioVideo(bool playVideo,bool playAudio)
    { AudioIdx=playAudio?NO_STREAM:DONT_PLAY;
      VideoIdx=playVideo?NO_STREAM:DONT_PLAY;}

    inline void SetAudioMode(int AudioMode) 
    { audioMode=AudioMode; if (aout) aout->SetAudioMode(audioMode); };
    inline int GetAudioMode()
    { return audioMode; };
    
    int BufferFill(void);
    // in percent 
    
    int64_t GetSTC(void);
};

#endif // MPEG2DECODER_H
