/*
 * utils.h: A plugin for the Video Disk Recorder
 *
 * See the README file for copyright information and how to reach the author.
 *
 * $Id: utils.h,v 1.4 2005/06/12 20:45:20 wachm Exp $
 */
#ifndef UTILS_H
#define UTILS_H
#include <vdr/plugin.h>
#include <sys/time.h>
#include "mmx.h"

// for MMX2 CPU's
#ifdef USE_MMX2
#define movntq(src,dest) do { movntq_r2m (src, dest); } while (0);
#else
// use this instead if you don't have a MMX2 CPU:
#define movntq(src,dest) do { movq_r2m (src, dest); } while (0);
#endif

/*    else			\
	movq_r2m (src, dest);	\
} while (0)
*/
void yuv_to_rgb (uint8_t * image, uint8_t * py,
                 uint8_t * pu, uint8_t * pv,
                 int width, int height,
                 int rgb_stride, int y_stride, int uv_stride,
                 int dstW, int dstH,
                 int depth, unsigned char * mask, int deintMethod);
uint64_t getTimeMilis(void);

void mmx_unpack_16rgb (uint8_t * image, int lines, int stride);
void mmx_unpack_15rgb (uint8_t * image, int lines, int stride);
void mmx_unpack_24rgb (uint8_t * image, int lines, int stride);
extern void (*mmx_unpack)(uint8_t * image, int lines, int stride);


void * fast_memcpy(void * to, const void * from, size_t len);
#endif

