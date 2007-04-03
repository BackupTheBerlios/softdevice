/*
 * audio-macos.c: A plugin for the Video Disk Recorder
 *
 * See the README file for copyright information and how to reach the author.
 *
 * $Id: audio-macos.c,v 1.1 2007/04/03 20:21:04 wachm Exp $
 */

/*
 * This file is based on MPlayers ao_macosx.c with the original copyright 
 * notice:
 * 
 *      Original Copyright (C) Timothy J. Wood - Aug 2000
 *
 */

#include "audio-macos.h"


#define AUDIODEB(out...) {printf("AUDIO-MACOS: ");printf(out);}


#ifndef AUDIODEB
#define AUDIODEB(out...)
#endif

/* This is large, but best (maybe it should be even larger).
 * CoreAudio supposedly has an internal latency in the order of 2ms */
#define NUM_BUFS 32

static cMacOsAudioOut *macosAO = NULL;

/**
 * \brief return number of free bytes in the buffer
 *    may only be called by mplayer's thread
 * \return minimum number of free bytes in buffer, value may change between
 *    two immediately following calls, and the real number of free bytes
 *    might actually be larger!
 */
int cMacOsAudioOut::buf_free() {
  int free = buf_read_pos - buf_write_pos - chunk_size;
  if (free < 0) free += buffer_len;
  return free;
}

/**
 * \brief return number of buffered bytes
 *    may only be called by playback thread
 * \return minimum number of buffered bytes, value may change between
 *    two immediately following calls, and the real number of buffered bytes
 *    might actually be larger!
 */
int cMacOsAudioOut::buf_used() {
  int used = buf_write_pos - buf_read_pos;
  if (used < 0) used += buffer_len;
  return used;
}

/**
 * \brief add data to ringbuffer
 */
int cMacOsAudioOut::write_buffer(unsigned char* data, int len){
  int first_len = buffer_len - buf_write_pos;
  int free = buf_free();
  if (len > free) {
          len = free;
          //AUDIODEB("write_buffer buffer full!\n");
  }
  if (first_len > len) first_len = len;
  // till end of buffer
  memcpy (&buffer[buf_write_pos], data, first_len);
  if (len > first_len) { // we have to wrap around
    // remaining part from beginning of buffer
    //AUDIODEB("write_buffer wrap around len %d first_len %d\n",len,first_len);
    memcpy (buffer, &data[first_len], len - first_len);
  }
  buf_write_pos = (buf_write_pos + len) % buffer_len;
  return len;
}

/**
 * \brief remove data from ringbuffer
 */
int cMacOsAudioOut::read_buffer(unsigned char* data,int len){
  int first_len = buffer_len - buf_read_pos;
  int buffered = buf_used();
  if (len > buffered) len = buffered;
  if (first_len > len) first_len = len;
  // till end of buffer
  memcpy (data, &buffer[buf_read_pos], first_len);
  if (len > first_len) { // we have to wrap around
    // remaining part from beginning of buffer
    //AUDIODEB("read_buffer wrap around len %d first_len %d\n",len,first_len);
    memcpy (&data[first_len], buffer, len - first_len);
  }
  buf_read_pos = (buf_read_pos + len) % buffer_len;
  return len;
}

OSStatus theRenderProc(void *inRefCon, AudioUnitRenderActionFlags *inActionFlags, const AudioTimeStamp *inTimeStamp, UInt32 inBusNumber, UInt32 inNumFrames, AudioBufferList *ioData) {
       return  (macosAO ? macosAO->Render(inNumFrames,ioData) : -1 );
};


OSStatus cMacOsAudioOut::Render(UInt32 inNumFrames, AudioBufferList *ioData) {
        int amt=buf_used();
        int req=(inNumFrames)*packetSize;

	if(amt>req)
 		amt=req;

	if(amt)
		read_buffer((unsigned char *)ioData->mBuffers[0].mData, amt);
	else Pause();
	ioData->mBuffers[0].mDataByteSize = amt;

 	return noErr;
}

cMacOsAudioOut::cMacOsAudioOut(cSetupStore *store) : cAudioOut() {
        ComponentDescription desc; 
        Component comp; 
        OSStatus err;

        if (macosAO) {
                fprintf(stderr,"MacOsAudioOut already initialized!\n");
                exit(-1);
        };
        macosAO=this;	
        buffer = NULL;

        desc.componentType = kAudioUnitType_Output;
	desc.componentSubType = kAudioUnitSubType_DefaultOutput;
	desc.componentManufacturer = kAudioUnitManufacturer_Apple;
	desc.componentFlags = 0;
	desc.componentFlagsMask = 0;
				
	comp = FindNextComponent(NULL, &desc);  //Finds an component that meets the desc spec's
	if (comp == NULL) {
		fprintf(stderr, "Unable to find Output Unit component\n");
		exit(-1);
	}
		
	err = OpenAComponent(comp, &(theOutputUnit));  //gains access to the services provided by the component
	if (err) {
		fprintf(stderr,"Unable to open Output Unit component (err=%d)\n", err);
		exit(-1);
	}

	// Initialize AudioUnit 
	err = AudioUnitInitialize(theOutputUnit);
	if (err) {
		fprintf(stderr, "Unable to initialize Output Unit component (err=%d)\n", err);
		exit(-1);
	}
        scale_Factor=0x7FFF;
}


cMacOsAudioOut::~cMacOsAudioOut() {
        AudioOutputUnitStop(theOutputUnit);
        AudioUnitUninitialize(theOutputUnit);
        CloseComponent(theOutputUnit);

        free(buffer);
}

int cMacOsAudioOut::SetParams(SampleContext &context) {

        if (currContext.samplerate == context.samplerate &&
                        currContext.channels == context.channels ) {
                context=currContext;
                return 0;
        };
        AUDIODEB("alsa-macos : SetParams samplerate %d channels %d\n",context.samplerate,context.channels);
        currContext=context;

        AudioStreamBasicDescription inDesc, outDesc;
        AURenderCallbackStruct renderCallback;
        OSStatus err;
        UInt32 size, maxFrames;

	// Build Description for the input format
	inDesc.mSampleRate=currContext.samplerate;
	inDesc.mFormatID=kAudioFormatLinearPCM;
	inDesc.mChannelsPerFrame=currContext.channels;
        inDesc.mBitsPerChannel=16;

        inDesc.mFormatFlags = kAudioFormatFlagIsSignedInteger|kAudioFormatFlagIsPacked;
        /*if((format&AF_FORMAT_POINT_MASK)==AF_FORMAT_F) {
	// float
                inDesc.mFormatFlags = kAudioFormatFlagIsFloat|kAudioFormatFlagIsPacked;
        }
        else if((format&AF_FORMAT_SIGN_MASK)==AF_FORMAT_SI) {
                // signed int
                inDesc.mFormatFlags = kAudioFormatFlagIsSignedInteger|kAudioFormatFlagIsPacked;
        }
        else {
                // unsigned int
                inDesc.mFormatFlags = kAudioFormatFlagIsPacked;
        }*/

#ifdef CPU_BIGENDIAN 
        inDesc.mFormatFlags |= kAudioFormatFlagIsBigEndian; //Intel processors???
#endif
        inDesc.mFramesPerPacket = 1;
        packetSize = inDesc.mBytesPerPacket = inDesc.mBytesPerFrame = inDesc.mFramesPerPacket*currContext.channels*(inDesc.mBitsPerChannel/8);


	size =  sizeof(AudioStreamBasicDescription);
	err = AudioUnitSetProperty(theOutputUnit, kAudioUnitProperty_StreamFormat, kAudioUnitScope_Input, 0, &inDesc, size);

	if (err) {
		printf( "Unable to set the input format (err=%d)\n", err);
		exit(-1);
	}

	size = sizeof(UInt32);
	err = AudioUnitGetProperty(theOutputUnit, kAudioDevicePropertyBufferSize, kAudioUnitScope_Input, 0, &maxFrames, &size);
	
	if (err) {
		printf( "AudioUnitGetProperty returned %d when getting kAudioDevicePropertyBufferSize\n", (int)err);
		exit(-1);
	}

	chunk_size = maxFrames;//*inDesc.mBytesPerFrame;
	printf("%5d chunk size\n", (int)chunk_size);
    
	num_chunks = NUM_BUFS;
        buffer_len = (num_chunks + 1) * chunk_size;
        buffer = buffer ? (unsigned char *)realloc(buffer,(num_chunks + 1)*chunk_size)
			: (unsigned char *)calloc(num_chunks + 1, chunk_size);

        buf_read_pos=0;
	buf_write_pos=0;
    
/*	
        ao_data.samplerate = inDesc.mSampleRate;
        ao_data.channels = inDesc.mChannelsPerFrame;
        ao_data.outburst = ao_data.buffersize = ao->chunk_size;
        ao_data.bps = ao_data.samplerate * inDesc.mBytesPerFrame;
*/
        renderCallback.inputProc = theRenderProc;
        renderCallback.inputProcRefCon = 0;
        err = AudioUnitSetProperty(theOutputUnit, kAudioUnitProperty_SetRenderCallback, kAudioUnitScope_Input, 0, &renderCallback, sizeof(AURenderCallbackStruct));
	if (err) {
		fprintf(stderr, "Unable to set the render callback (err=%d)\n", err);
		exit(-1);
	}

        return 0;
}

/* stop playing, keep buffers (for pause) */
void cMacOsAudioOut::Pause() {
  OSErr status=noErr;

  /* stop callback */
  status=AudioOutputUnitStop(theOutputUnit);
  if (status)
    fprintf(stderr, "AudioOutputUnitStop returned %d\n",(int)status);
}


/* resume playing, after audio_pause() */
void cMacOsAudioOut::Play() {
  OSErr status=noErr;
  
  status=AudioOutputUnitStart(theOutputUnit);
  if (status)
    fprintf(stderr,"AudioOutputUnitStart returned %d\n",(int)status);
}

void cMacOsAudioOut::Write(uchar *Data, int Length) {
        Play();
        Scale((int16_t*)Data,Length/2,scale_Factor);
        while (Length >0) {
                int len = write_buffer(Data,Length);
                Data +=len;
                Length-=len;
                usleep(13000);
        }
};

int cMacOsAudioOut::GetDelay() {
        if ( ! currContext.samplerate*currContext.channels )
                return -1;

        int res = buffer_len - chunk_size - buf_free();
        res = res*10000 / (currContext.samplerate*currContext.channels*2) ;
        //AUDIODEB("GetDelay %d\n",res);
        return res;
};

void cMacOsAudioOut::SetVolume( int vol ) {
        scale_Factor = CalcScaleFactor(vol);
};
