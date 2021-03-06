/*
 * video-dummy.c: A plugin for the Video Disk Recorder
 *
 * See the README file for copyright information and how to reach the author.
 *
 * $Id: video-dummy.c,v 1.5 2007/05/10 19:54:44 wachm Exp $
 *
 * This is a dummy output driver.
 */

#include <vdr/plugin.h>
#include "video-dummy.h"


cDummyVideoOut::cDummyVideoOut(cSetupStore *setupStore, cSetupSoftlog *Softlog)
                : cVideoOut(setupStore,Softlog)
{
    printf("[video-dummy] Initializing Driver, no output supported. You have to configure a driver IN the Makefile\n");
}

void cDummyVideoOut::Pause(void)
{
}

void cDummyVideoOut::Refresh(cBitmap *Bitmap)
{
}

void cDummyVideoOut::YUV(sPicBuffer *buf)
{
}

cDummyVideoOut::~cDummyVideoOut() {
}

