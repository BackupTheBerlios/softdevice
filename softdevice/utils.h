/*
 * utils.h: A plugin for the Video Disk Recorder
 *
 * See the README file for copyright information and how to reach the author.
 *
 * $Id: utils.h,v 1.1 2004/08/01 05:07:05 lucke Exp $
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
				  int dstW, int dstH, int depth, unsigned char * mask);
uint64_t getTimeMilis(void);

#endif

