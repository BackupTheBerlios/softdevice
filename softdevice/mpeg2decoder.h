/*
 * mpeg2decoder.h: A plugin for the Video Disk Recorder
 *
 * See the README file for copyright information and how to reach the author.
 *
 * $Id: mpeg2decoder.h,v 1.1 2004/08/01 05:07:03 lucke Exp $
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

// Output device handler
class cStreamDecoder  {
private:
    unsigned int streamID; // stream to filter
    unsigned int syncword;
    int payload;
    int state;
    int headerLength;
    unsigned char * hdr_ptr;
protected:
    uint64_t *cPTS;
    int64_t pts;
    int frame;
    bool validPTS;
    unsigned char header[MAX_HDR_LEN];     
    AVCodec *codec;
    AVCodecContext *context;
    int ParseStream(uchar *Data, int Length);
    int ParseStreamIntern(uchar *Data, int Length, unsigned int lowID, unsigned int highID);
public:
    virtual int DecodeData(uchar *Data, int Length) = 0;
    virtual void Write(uchar *Data, int Length) = 0;
    virtual int StillPicture(uchar *Data, int Length) {return 0;};

    cStreamDecoder(unsigned int StreamID);
    virtual ~cStreamDecoder();
    void SyncPTS(uint64_t spts);
};


class cAudioStreamDecoder : public cStreamDecoder {
private:
    uint8_t * audiosamples;
    cAudioOut *audioOut;
protected:
public:
    virtual int DecodeData(uchar *Data, int Length);
    cAudioStreamDecoder(unsigned int StreamID, cAudioOut *AudioOut, uint64_t *commonPTS);
    ~cAudioStreamDecoder();
    virtual void Write(uchar *Data, int Length);

};

class cVideoStreamDecoder : public cStreamDecoder , cThread {
  private:
    cMutex              mutex;
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
    uint64_t            vpts;
    int                 avgOffset;
    cRingBufferLinear   *ringBuffer;
    bool                active, running;

    void    resetCodec(void);
    uchar   *allocatePicBuf(uchar *pic_buf);
    void    deintLibavcodec(void);
    uchar   *freePicBuf(uchar *pic_buf);
#ifdef PP_LIBAVCODEC
    void    ppLibavcodec(void);
#endif //PP_LIBAVCODEC
    void    Mirror(void);

  protected:
    virtual void Action(void);

  public:
    cVideoStreamDecoder(unsigned int StreamID, cVideoOut *VideoOut, uint64_t *commonPTS);
    ~cVideoStreamDecoder();

    virtual int   DecodeData(uchar *Data, int Length);
    virtual int   StillPicture(uchar *Data, int Length);
    virtual void  Write(uchar *Data, int Length);
};


class cMpeg2Decoder {
private:
    uint8_t * audiosamples;
    int  state, payload, streamtype;
    unsigned char header[6];     
    unsigned int syncword;
    cStreamDecoder *vout, *aout;
    cAudioOut *audioOut;
    cVideoOut *videoOut;
    uint64_t commonPTS;
    bool running;
    bool decoding;
public:
    cMpeg2Decoder(cAudioOut *AudioOut, cVideoOut *VideoOut);
    ~cMpeg2Decoder();
    int Decode(const uchar *Data, int Length);
    int StillPicture(uchar *Data, int Length);
    void Start(void);
    void Stop(void);
};

#endif // MPEG2DECODER_H
