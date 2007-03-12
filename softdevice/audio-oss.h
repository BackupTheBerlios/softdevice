/*
 * audio-oss.h: A plugin for the Video Disk Recorder
 *
 * See the README file for copyright information and how to reach the author.
 *
 * Support for the Open Sound System contributed by Lubos Novak
 * 
 * $Id: audio-oss.h,v 1.2 2007/03/12 20:10:54 wachm Exp $
 */
#ifndef _AUDIO_OSS_H_
#define _AUDIO_OSS_H_

#include "audio.h"

class cOSSAudioOut : public cAudioOut {
private:
    cMutex handleMutex;
    volatile bool paused;
    int fdDSP;
    int fdMixer;
    int scale_Factor;
    cSetupStore *setupStore;
public:
    cOSSAudioOut(cSetupStore *setupStore);
    virtual ~cOSSAudioOut();
    virtual void Write(uchar *Data, int Length);
    virtual void WriteAC3(uchar *Data, int Length);
    virtual int SetParams(SampleContext &context);
    virtual int GetDelay(void); // returns delay in 0.1 ms
    virtual void Pause(void);
    virtual void Play(void);
    virtual void SetVolume(int vol);
};

#endif
