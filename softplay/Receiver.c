/*
 * Media Player plugin for VDR
 *
 * Copyright (C) 2005 Martin Wache
 *
 * This code is distributed under the terms and conditions of the
 * GNU GENERAL PUBLIC LICENSE. See the file COPYING for details.
 *
 * $Id: Receiver.c,v 1.1 2006/03/12 20:23:23 wachm Exp $
 */

#include "Receiver.h"

#include <sys/time.h>

#include <vdr/channels.h>
#include <vdr/device.h>
#include <vdr/remux.h>
#include <vdr/ringbuffer.h>

//#define RECEIVER_DEB(out...) {printf("RECIEVER[%04d]:",(int)(getTimeMillis() % 10000));printf(out);}

#ifndef RECEIVER_DEB
#define RECEIVER_DEB(out...)
#endif

#define BUFFER_SIZE 1024*1024

static inline uint64_t getTimeMillis(void) {
        struct timeval tv;
        gettimeofday(&tv,NULL);
        return (int64_t)tv.tv_sec * 1000 + tv.tv_usec / 1000;
}

cSoftplayReceiver::cSoftplayReceiver(const cChannel *Channel, cDevice *Device,
                PacketHandlesV100 &SoftHandles) 
        : cReceiver(Channel->Ca(),0,Channel->Vpid()),startChannel(*Channel),
          currChannel(*Channel) {
                RECEIVER_DEB("cSoftplayReceiver");
                device=Device;
                softHandles=SoftHandles;
                buffer = new cRingBufferLinear(BUFFER_SIZE,TS_SIZE*2);

                remux = new cRemux(Channel->Vpid(), NULL, NULL, NULL,true);
                active=false;
        };

cSoftplayReceiver::~cSoftplayReceiver() {
        RECEIVER_DEB("~cSoftplayReceiver");
        if (active) {
                active = false;
                Cancel(3);
        };
        
        Detach();
        delete buffer;
        delete remux;
};


void cSoftplayReceiver::Activate(bool On) {
        RECEIVER_DEB("Activate %d\n",On);
        if (On) {
                Start();
                StartTransfer();
        } else if ( active ) {
                StopTransfer();
                active =false;
                Cancel(3);
        };
};


void cSoftplayReceiver::Receive(uchar *Data, int Length) {
        //RECEIVER_DEB("Receive data: %p, length %d transfer %d\n",
        //                Data,Length,transfer);
        if (!transfer)
                return;

        int ret = buffer->Put(Data, Length);
        if (ret != Length) {
                RECEIVER_DEB("buffer overflow in Receive ret=%d!\n",ret);
                fprintf(stderr,"buffer overflow in Receive!\n");
        };
};

void cSoftplayReceiver::Action() {
        uchar *pes=NULL;
        RECEIVER_DEB("cSoftplayReceiver thread started!\n");

        active = true;
        while (active) {
                int length;
                if ( buffer->Available() > 9 * BUFFER_SIZE/10 ) {
                        printf("clearing receiver buffers!!\n");
                        buffer->Clear();
                        remux->Clear();
                        softHandles.ResetDecoder(device,SOFTDEVICE_VIDEO_STREAM,0);
                };
        
                if (!transfer ) {
                        RECEIVER_DEB("Transfer stopped, waiting!!\n");
                        buffer->Clear();
                        remux->Clear();
                        softHandles.ResetDecoder(device,SOFTDEVICE_VIDEO_STREAM,0);
                        transfer_stopped=true;
                        while ( !transfer && active )
                                usleep(10000);
                        buffer->Clear();
                        transfer_stopped=false;
                        RECEIVER_DEB("Transfer restarted!!\n");
                };
                uchar *data=buffer->Get(length);
                if (!data) {
                        // no data in buffer
                        usleep(10000);
                        continue;
                };


                RECEIVER_DEB("got %d bytes from buffer\n",length);
                length=remux->Put(data,length);
                RECEIVER_DEB("put %d bytes into remux\n",length);
                if (length)
                        buffer->Del(length);
                
                pes=remux->Get(length);
                if (!pes) 
                        continue;
                RECEIVER_DEB("got %d (%p) from remuxer\n",length,data);

                int count=0;
                while (softHandles.BufferFill(device,SOFTDEVICE_VIDEO_STREAM,0) > 90  
                                && count < 10 ) {
                        usleep(10000);
                        count++;
                };
                if ( count > 9) {
                        printf("Poll timeout. Reset decoder!\n");
                        softHandles.ResetDecoder(device,SOFTDEVICE_VIDEO_STREAM,0);
                };
                
                int played = device->PlayPes(pes,length,true);
                RECEIVER_DEB("played %d\n",played);

                if ( played > 0 ) {
                       remux->Del(played);
                       if (length <= 0)
                               pes=NULL;
                } else  {
                        printf("PlayPes error: %d\n",played);
                        device->PlayPes(NULL,0,true);
                        usleep(10000);
                };
        };
        RECEIVER_DEB("cSoftplayReceiver thread ended!\n");
};

                
        
