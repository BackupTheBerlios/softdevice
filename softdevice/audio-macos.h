/*
 * audio-macos.h: A plugin for the Video Disk Recorder
 *
 * See the README file for copyright information and how to reach the author.
 *
 * $Id: audio-macos.h,v 1.1 2007/04/03 20:21:04 wachm Exp $
 */
#ifndef __AUDIO_MACOS_H__
#define __AUDIO_MACOS_H__
#include <vdr/plugin.h>
#include "audio.h"


namespace MacOs {
#include <CoreServices/CoreServices.h>
#include <AudioUnit/AudioUnit.h>
#include <AudioToolbox/AudioToolbox.h>
}


using namespace MacOs;

/* ---------------------------------------------------------------------------
 */
class cMacOsAudioOut : public cAudioOut  {
        private:
                int scale_Factor;

                /* AudioUnit */
                AudioUnit theOutputUnit;
                int packetSize;

                /* Ring-buffer */
                /* does not need explicit synchronization, but needs to allocate
                 * (num_chunks + 1) * chunk_size memory to store num_chunks * chunk_size
                 * data */
                unsigned char *buffer;
                unsigned int buffer_len; ///< must always be (num_chunks + 1) * chunk_size
                unsigned int num_chunks;
                unsigned int chunk_size;

                unsigned int buf_read_pos;
                unsigned int buf_write_pos;
        protected:
                int buf_free();
                int buf_used();
                int write_buffer(unsigned char* data, int len);
                int read_buffer(unsigned char* data, int len);

        public:
                cMacOsAudioOut(cSetupStore *setupStore);
                virtual ~cMacOsAudioOut();
                OSStatus Render(UInt32 inNumFrames, AudioBufferList *ioData); 

                virtual void  Write(uchar *Data, int Length);
                virtual void  WriteAC3(uchar *Data, int Length)
                {};
                virtual int   SetParams(SampleContext &context);
                virtual int   GetDelay(void);
                virtual void  Pause(void);
                virtual void  Play(void);
                virtual void  SetVolume(int vol);
/*                virtual void  Suspend(void);
                virtual bool  Resume(void);
*/};


#endif 
