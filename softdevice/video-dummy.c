/*
 * video-dummy.c: A plugin for the Video Disk Recorder
 *
 * See the README file for copyright information and how to reach the author.
 *
 * $Id: video-dummy.c,v 1.1 2004/08/01 05:07:05 lucke Exp $
 *
 * This is a dummy output driver.
 */

#include <vdr/plugin.h>
#include "video-dummy.h"


cDummyVideoOut::cDummyVideoOut()
{
    printf("[video-dummy] Initializing Driver, no output supported. You have to configure a driver IN the Makefile\n");
}

void cDummyVideoOut::Pause(void) 
{
}

#if VDRVERSNUM >= 10307
void cDummyVideoOut::Refresh(cBitmap *Bitmap)
{
}
#else
void cDummyVideoOut::Refresh()
{
}
#endif

void cDummyVideoOut::YUV(uint8_t *Py, uint8_t *Pu, uint8_t *Pv, int Width, int Height, int Ystride, int UVstride)
{
}

cDummyVideoOut::~cDummyVideoOut() {
}

