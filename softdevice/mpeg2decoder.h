/*
 * mpeg2decoder.h: A plugin for the Video Disk Recorder
 *
 * See the README file for copyright information and how to reach the author.
 *
 * $Id: mpeg2decoder.h,v 1.48 2011/04/16 14:23:59 lucke Exp $
 */
#ifndef MPEG2DECODER_H
#define MPEG2DECODER_H

#ifdef HAVE_CONFIG
# include "config.h"
#endif

#include <sys/time.h>
extern "C" {
#if HAVE_FFMPEG_LOCAL
# include <libavcodec/avcodec.h>
# include <libavformat/avformat.h>
#else
# include <avcodec.h>
# include <avformat.h>
#endif
}

#include <vdr/plugin.h>
#include <vdr/ringbuffer.h>

#include "sync-timer.h"
#include "video.h"
#include "audio.h"
#include "VideoFilter.h"

#define DEFAULT_FRAMETIME 400   // for PAL in 0.1ms
#define MIN_BUF_SIZE  (16*1024)

// this combination gives good results when seeking
#define DVB_BUF_SIZE   (32*1024)
#define PACKET_BUF_SIZE 150

// this combination is save
//#define DVB_BUF_SIZE   (64*1024)
//#define PACKET_BUF_SIZE 300

// this combination works for HDTV
//#define DVB_BUF_SIZE   (64*1024)
//#define PACKET_BUF_SIZE 2000


#define NO_STREAM    -1
#define DONT_PLAY  -100

#define SOFTDEVICE_VIDEO_STREAM  1
#define SOFTDEVICE_AUDIO_STREAM  2
#define SOFTDEVICE_BOTH_STREAMS (SOFTDEVICE_VIDEO_STREAM | SOFTDEVICE_AUDIO_STREAM )


class cAudioStreamDecoder;
class cVideoStreamDecoder;

// -----------------cClock --------------------------------------------
class cClock {
private:

    static int64_t audioOffset;
    static int64_t audioPTS;
    static int64_t videoOffset;
    static int64_t videoPTS;
    static bool freezeMode;

public:
    cClock() {audioOffset=0;videoOffset=0;audioPTS=0;videoPTS=0;
        freezeMode=true;};
    virtual ~cClock() {};

    static int64_t GetTime()
    {
      struct timeval tv;
      gettimeofday(&tv,NULL);
      return (int64_t)tv.tv_sec*10000+(int64_t)tv.tv_usec/100;
    };

    static inline void AdjustAudioPTS(int64_t aPTS) {
      if (aPTS)
        audioOffset=aPTS-GetTime();
      else audioOffset=0;
      audioPTS=aPTS;
    };

    static inline void AdjustVideoPTS(int64_t vPTS) {
      if (vPTS)
        videoOffset=vPTS-GetTime();
      else videoOffset=0;
      videoPTS=vPTS;
    };

    static inline void SetFreezeMode(bool FreezeMode) {
      freezeMode=FreezeMode;
    };

    int64_t GetPTS();
};

// -----------------cPacketQueue -----------------------------------------
class cPacketQueue {
    // Only one thread may read, and only one thread may write!!!
private:
    int MaxPackets;
    AVPacket *queue;
    int FirstPacket,LastPacket;

    inline int Next(int Packet)
    { return (Packet+1)%MaxPackets; };

    cSigTimer EnablePut;
    cSigTimer EnableGet;

public:
    cPacketQueue(int maxPackets=PACKET_BUF_SIZE);
    ~cPacketQueue();

    int PutPacket(const AVPacket &Packet);

    inline void EnablePutSignal()
    { EnablePut.Signal();};

    AVPacket * GetReadPacket();
    void FreeReadPacket(AVPacket *Packet);

    void Clear();
    inline int Available()
    { return (LastPacket+MaxPackets - FirstPacket)%MaxPackets; };
    inline int GetMaxPackets()
    { return MaxPackets;};
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

//-------------------------cStreamDecoder ----------------------------------
// Output device handler
class cStreamDecoder : public cThread {
private:
    cPacketQueue  PacketQueue;
    bool          packetMode;

protected:
    cSyncTimer         *syncTimer;
    bool freezeMode;
    int64_t           pts;
    int               frame;

    AVCodec           *codec;
    AVCodecContext    *context;

    cMutex            mutex;
    volatile bool     active;
    bool              running;

    virtual void      Action(void);
    virtual int       DecodePacket(AVPacket *pkt) = 0;

public:
    int               repeatFrame;

public:
    inline int        PutPacket(const AVPacket &pkt)
                        { return PacketQueue.PutPacket(pkt); };

    virtual void      Clear(void);
    virtual void      Freeze(bool freeze=true);
    virtual void      Play(void);
    void      Stop();
    virtual void      TrickSpeed(int Speed) {return;};
    virtual int       BufferFill(void);
    bool              initCodec(void);
    void              resetCodec(void);
    virtual int64_t  GetPTS()  {return pts;};

    cStreamDecoder(AVCodecContext * Context, bool packetMode);
    virtual ~cStreamDecoder();
};

//----------------------------cAudioStreamDecoder ----------------------------
class cAudioStreamDecoder : public cStreamDecoder {
private:
    uint8_t               *audiosamples;
    cSoftRingBufferLinear *audioBuffer;
    cAudioOut             *audioOut;
    SampleContext         audioOutContext;
    int                   audioMode;

    void OnlyLeft(uint8_t *samples,int Length);
    // copy left data to right channel

    void OnlyRight(uint8_t *samples,int Length);
    // copy right data to left channel

    bool UpmixMono(uint8_t *samples, int Length);
    // upmix mono channel to stereo

protected:
public:
    virtual int DecodePacket(AVPacket *pkt);
    cAudioStreamDecoder(AVCodecContext *Context, cAudioOut *AudioOut,
       int AudioChannel=0, bool packetMode = false);
    ~cAudioStreamDecoder();
    inline void SetAudioMode(int AudioMode)
      { audioMode=AudioMode; };
    virtual int64_t GetPTS();
};

//---------------------------cVideoStreamDecoder -----------------------------
class cVideoStreamDecoder : public cStreamDecoder {
  private:
    cClock *clock;
#define NO_PTS_VALUES 10
    struct {
       int coded_frame_no;
       int64_t pts;
       int duration;
    } pts_values[NO_PTS_VALUES];
    int lastPTSidx;
    int lastCodedPictNo;
    int64_t lastPTS;
    int lastDuration;
    AVFrame             *picture;
    sPicBuffer          privBuffer;

    cVideoMirror        Mirror;
    cDeintLibav         DeintLibav;
    cBorderDetect       BorderDetect;
#ifdef PP_LIBAVCODEC
    cLibAvPostProc      LibAvPostProc;
#endif //PP_LIBAVCODEC

    cVideoOut           *videoOut;

    // A-V syncing stuff
    int                offset;
    int                trickspeed;
    int                default_frametime;
    inline int frametime()
    {return trickspeed*default_frametime;};

  public:
    cVideoStreamDecoder(AVCodecContext *Context, cVideoOut *VideoOut,
       cClock *clock, int Trickspeed, bool packetMode);
    ~cVideoStreamDecoder();

    int DecodePicture_avcodec(sPicBuffer *&pic, int &got_picture,
                              uint8_t *data, int length, int64_t pts);
#ifdef HAVE_CLE266_MPEG_DECODER
    int DecodePicture_cle266(sPicBuffer *&pic, int &got_picture,
                              uint8_t *data, int length, int64_t pts);
#endif //HAVE_CLE266_MPEG_DECODER
    virtual void      Freeze(bool freeze=true);
    virtual void      Play(void);
    virtual int DecodePacket(AVPacket *pkt);
    virtual void TrickSpeed(int Speed);
    virtual int64_t GetPTS();
};

//--------------------------------cMpeg2Decoder -------------------------
class cMpeg2Decoder: public cThread  {
private:
    cVideoStreamDecoder  *vout;
    cAudioStreamDecoder  *aout;
    cMutex          voutMutex;
    cMutex          aoutMutex;
    cAudioOut       *audioOut;
    cVideoOut       *videoOut;
    bool running;
    bool IsSuspended;
    bool decoding;
    bool freezeMode;

    AVFormatContext *ic;
    ByteIOContext   *pb;
    bool            useAVReadFrame;
    int LastSize;
    cMutex  mutex;
    cSigTimer EnablePutSignal;
    cSigTimer EnableGetSignal;
    cSoftRingBufferLinear *StreamBuffer;
    void initStream();
    virtual void Action(void);
    volatile bool   ThreadActive, ThreadRunning;
    cClock  clock;
    //demuxing
    int AudioIdx;
    int VideoIdx;

    int audioMode;
    int Speed;

public:
    enum softPlayMode {
      PmNoChange=-1,
      PmAudioVideo,
      PmVideoOnly,
      PmAudioOnly,
    };
private:
    softPlayMode  curPlayMode;
    bool          packetMode;
public:
    int read_packet(uint8_t *buf, int buf_size);
    int seek(int64_t offset, int whence);

public:
    cMpeg2Decoder(cAudioOut *AudioOut, cVideoOut *VideoOut);
    ~cMpeg2Decoder();

    void QueuePacket(const AVFormatContext *ic,AVPacket &pkt,
		    bool PacketMode);
    void ResetDecoder( int Stream = SOFTDEVICE_BOTH_STREAMS );
    int Decode(const uchar *Data, int Length);
    int StillPicture(uchar *Data, int Length);
    void Start(bool GetMutex=true);
    void Play(void);
    void Freeze( int Stream = SOFTDEVICE_BOTH_STREAMS, bool freeze=true );
    void Stop(bool GetMutex=true);
    void Clear(void);
    void Suspend(void);
    void Resume(void);
    void TrickSpeed(int Speed);

    void SetPlayMode(softPlayMode playMode, bool packetMode);
    void PlayAudioVideo(bool playAudio,bool playVideo)
    { AudioIdx=playAudio?NO_STREAM:DONT_PLAY;
      VideoIdx=playVideo?NO_STREAM:DONT_PLAY;}

    void SetAudioMode(int AudioMode);
    inline int GetAudioMode()
    { return audioMode; };

    int BufferFill( int Stream = SOFTDEVICE_BOTH_STREAMS );
    // in percent

    int64_t GetSTC(void);
};

#endif // MPEG2DECODER_H
