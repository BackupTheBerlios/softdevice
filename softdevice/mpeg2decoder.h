/*
 * mpeg2decoder.h: A plugin for the Video Disk Recorder
 *
 * See the README file for copyright information and how to reach the author.
 *
 * $Id: mpeg2decoder.h,v 1.10 2005/01/13 20:31:08 lucke Exp $
 */
#ifndef MPEG2DECODER_H
#define MPEG2DECODER_H
#include <avcodec.h>
#ifdef PP_LIBAVCODEC
  #include <postproc/postprocess.h>
#endif //PP_LIBAVCODEC
#include "video.h"
#include "audio.h"
#include <vdr/plugin.h>
#include <vdr/ringbuffer.h>
#define MAX_HDR_LEN 0xFF

#define DEFAULT_FRAMETIME 40   // for PAL
#define DVB_BUF_SIZE   (4* 256*1024)  // same value as in dvbplayer.c
#define AVG_FRAME_SIZE 15000         // dito 
#define PTS_COUNT       4             // history of four PTS values

struct PES_Header1 {
  unsigned int is_original:1;
  unsigned int has_copyright:1;
  unsigned int data_alignment:1;
  unsigned int priority:1;
  unsigned int scrambling_ctrl:2;
  unsigned int flag:2;
};

struct PES_Header2 {
  unsigned int extension_exists:1;
  unsigned int has_crc:1;
  unsigned int has_copy_info:1;
  unsigned int trick_mode_flag:1;
  unsigned int has_es_rate:1;
  unsigned int has_escr:1;
  unsigned int ptsdts_flags:2;
};

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

// Output device handler
class cStreamDecoder : public cThread {
private:

    // used by ParseStreamIntern
    unsigned int syncword;
    int payload;
    int state;
    int headerLength;
    unsigned char * hdr_ptr;

    struct PES_Header1 Header1;
    struct PES_Header2 Header2;


    bool freezeMode;
protected:
    unsigned int          streamID; // stream to filter
    int64_t               newPTS,
                          prevPTS,
                          historyPTS[PTS_COUNT],
                          pts;
    int                   frame,
                          historyPTSIndex;
    bool                  validPTS;
    unsigned char         header[MAX_HDR_LEN];
    AVCodec               *codec;
    AVCodecContext        *context;
    
    cMutex                mutex;
    cSoftRingBufferLinear *ringBuffer;
    bool                  active, running;
    
    int ParseStream(uchar *Data, int Length);
    int ParseStreamIntern(uchar *Data, int Length, unsigned int lowID, unsigned int highID);
    inline void ClearParseStream() {state=0;};
    
    virtual void Action(void);
public:
    virtual int DecodeData(uchar *Data, int Length) = 0;
    virtual void Write(uchar *Data, int Length);
    virtual void Clear(void);
    virtual void Freeze(void);
    virtual void Play(void);
    virtual void TrickSpeed(int Speed) {return;};
    virtual int StillPicture(uchar *Data, int Length) {return 0;};
    virtual int BufferFill(void);
    virtual uint64_t GetPTS()  {return pts;};

    virtual void Stop();
    cStreamDecoder(unsigned int StreamID);
    virtual ~cStreamDecoder();
};


class cAudioStreamDecoder : public cStreamDecoder {
private:
    uint8_t * audiosamples;
    cAudioOut *audioOut;
protected:
public:
    virtual int DecodeData(uchar *Data, int Length);
    cAudioStreamDecoder(unsigned int StreamID, cAudioOut *AudioOut);
    ~cAudioStreamDecoder();
    virtual uint64_t GetPTS();
};

class cVideoStreamDecoder : public cStreamDecoder {
  private:
    cAudioStreamDecoder *AudioStream;
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
    int64_t            lastTime;
    int                offset;
    int                delay;
    int                rtc_fd; 
    int                frametime;
    int32_t GetRelTime();
   
    void    resetCodec(void);
    uchar   *allocatePicBuf(uchar *pic_buf);
    void    deintLibavcodec(void);
    uchar   *freePicBuf(uchar *pic_buf);
#ifdef PP_LIBAVCODEC
    void    ppLibavcodec(void);
#endif //PP_LIBAVCODEC
    void    Mirror(void);

  public:
    cVideoStreamDecoder(unsigned int StreamID, cVideoOut *VideoOut,
       cAudioStreamDecoder *AudioStreamDecoder );
    ~cVideoStreamDecoder();

    virtual int   DecodeData(uchar *Data, int Length);
    virtual int   StillPicture(uchar *Data, int Length);
    virtual void TrickSpeed(int Speed);
};


class cMpeg2Decoder {
private:
    uint8_t * audiosamples;
    int  state, payload, streamtype;
    unsigned char header[6];     
    unsigned int syncword;
    cStreamDecoder  *vout, *aout;
    cAudioOut       *audioOut;
    cVideoOut       *videoOut;
    int             ac3Mode, ac3Parm, lpcmMode;
    bool running;
    bool decoding;
public:
    cMpeg2Decoder(cAudioOut *AudioOut, cVideoOut *VideoOut);
    ~cMpeg2Decoder();
    int PlayAudio(const uchar *Data, int Length);
    int Decode(const uchar *Data, int Length);
    int StillPicture(uchar *Data, int Length);
    void Start(void);
    void Play(void);
    void Freeze(void);
    void Stop(void);
    void Clear(void);
    void TrickSpeed(int Speed);
    bool BufferFilled(void);
    int64_t GetSTC(void);
};

#endif // MPEG2DECODER_H
