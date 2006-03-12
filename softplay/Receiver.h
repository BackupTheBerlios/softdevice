/*
 * Media Player plugin for VDR
 *
 * Copyright (C) 2005 Martin Wache
 *
 * This code is distributed under the terms and conditions of the
 * GNU GENERAL PUBLIC LICENSE. See the file COPYING for details.
 *
 * $Id: Receiver.h,v 1.1 2006/03/12 20:23:23 wachm Exp $
 */
#ifndef __RECEIVER_H__
#define __RECEIVER_H__

#include "vdr/receiver.h"
#include "vdr/thread.h"
#include "vdr/device.h"
#include "vdr/remux.h"
#include "SoftHandles.h"
//#include "../softdevice/softdevice.h"

class cSoftplayReceiver : public cReceiver, public cThread {
        private: 
                friend class cSoftPlayer;
                cRingBufferLinear *buffer;
                cDevice *device;
                cRemux *remux;
                PacketHandlesV100 softHandles;

                bool active;

                bool transfer,transfer_stopped;
                cChannel startChannel;
                cChannel currChannel;
        public:
                cSoftplayReceiver( const cChannel *Channel, cDevice *Device,
                                PacketHandlesV100 &SoftHandles);
                virtual ~cSoftplayReceiver();
                
        protected:
                virtual void Activate(bool On);
                virtual void Receive(uchar *Data, int Length);
                virtual void Action(void);

        public:
                inline void StopTransfer() {
                      transfer=false;
                      while (!transfer_stopped)
                              usleep(10000);
                };

                inline void StartTransfer() {
                        transfer=true;
                };
                
};

#endif // __RECEIVER_H_
