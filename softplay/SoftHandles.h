/*
 * Media Player plugin for VDR
 *
 * Copyright (C) 2005 Martin Wache
 *
 * This code is distributed under the terms and conditions of the
 * GNU GENERAL PUBLIC LICENSE. See the file COPYING for details.
 *
 * $Id: SoftHandles.h,v 1.1 2006/03/12 20:23:23 wachm Exp $
 */

#ifndef __SOFTHANDLES_H
#define __SOFTHANDLES_H

#include <avformat.h>

#if VDRVERSNUM >= 10330
#define SOFTDEVICE_VIDEO_STREAM  1
#define SOFTDEVICE_AUDIO_STREAM  2 
#define SOFTDEVICE_BOTH_STREAMS (SOFTDEVICE_VIDEO_STREAM | SOFTDEVICE_AUDIO_STREAM )
        
       typedef void (* fQueuePacket)(cDevice *Device, AVFormatContext *ic, AVPacket &pkt);

typedef int (* SoftdeviceHandle)(cDevice *Device,int Stream, int value);

typedef void (* SoftdeviceDrawStill)(cDevice *Device,
                                uint8_t *pY, uint8_t *pU, uint8_t *pV,
                                int w, int h, int yPitch, int uvPitch);

struct PacketHandlesV100{
        fQueuePacket  QueuePacket;
        SoftdeviceHandle ResetDecoder;
        SoftdeviceHandle BufferFill;
        SoftdeviceHandle Freeze;
        // value = 0 Play, value = 1 Pause
};
        

static const char *const GET_PACKET_HANDEL_IDV100="softdevice-GetPacketHandles-v1.0";

#else
#include "../softdevice/softdevice.h"
#endif


#endif
