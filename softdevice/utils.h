/*
 * utils.h: A plugin for the Video Disk Recorder
 *
 * See the README file for copyright information and how to reach the author.
 *
 * $Id: utils.h,v 1.9 2006/06/06 20:55:19 lucke Exp $
 */
#ifndef UTILS_H
#define UTILS_H
//#include <vdr/plugin.h>
#include <stdint.h>
#include <sys/time.h>
#include "config.h"
#include "mmx.h"

// for MMX2 CPU's
#ifdef USE_MMX2
#define movntq(src,dest) do { movntq_r2m (src, dest); } while (0);
#else
// use this instead if you don't have a MMX2 CPU:
#define movntq(src,dest) do { movq_r2m (src, dest); } while (0);
#endif

// MMX - 3Dnow! defines

#undef PREFETCH
#undef MOVNTQ
#undef SFENCE
#undef EMMS

#ifdef USE_3DNOW
//#warning Using 3Dnow! extensions
#define PREFETCH(x) "prefetchnta " x
#define MOVNTQ   "movntq "
#define SFENCE   __asm__ __volatile__  (" sfence \n": : : "memory"  )
#define EMMS     __asm__ __volatile__  (" femms \n": : : "memory"  )

#elif defined ( USE_MMX2 )
//#warning Using MMX2 extensions
#define PREFETCH(x) "prefetchnta " x
#define MOVNTQ   "movntq "
#define SFENCE   __asm__ __volatile__  (" sfence \n": : : "memory"  )
#define EMMS     __asm__ __volatile__ (" emms \n": : : "memory"  )

#else
//#warning Using MMX extensions
#define PREFETCH(x) 
#define MOVNTQ   "movq "
#define SFENCE  
#define EMMS     __asm__ __volatile__ (" emms \n": : : "memory"  )
#endif


void yv12_to_yuy2_il_c(const uint8_t *py,
                       const uint8_t *pu,
                       const uint8_t *pv,
                       uint8_t *dst,
                       int width,int height,
                       int lumStride,int chromStride,
                       int dstStride);

void yv12_to_yuy2_il_mmx2(const uint8_t *py,
                          const uint8_t *pu,
                          const uint8_t *pv,
                          uint8_t *dst,
                          int width,int height,
                          int lumStride,int chromStride,
                          int dstStride);

void yv12_to_yuy2_fr_c(const uint8_t *py,
                       const uint8_t *pu,
                       const uint8_t *pv,
                       uint8_t *dst,
                       int width,int height,
                       int lumStride,int chromStride,
                       int dstStride);

void yv12_to_yuy2_fr_mmx2(const uint8_t *py,
                          const uint8_t *pu,
                          const uint8_t *pv,
                          uint8_t *dst,
                          int width,int height,
                          int lumStride,int chromStride,
                          int dstStride);

void yv12_to_yuy2( const uint8_t *ysrc,
                   const uint8_t *usrc,
                   const uint8_t *vsrc,
                   uint8_t *dst,
                   int width,
                   int height,
                   int lumStride,
                   int chromStride,
                   int dstStride
                 );

void yuv_to_rgb (uint8_t * image, uint8_t * py,
                 uint8_t * pu, uint8_t * pv,
                 int width, int height,
                 int rgb_stride, int y_stride, int uv_stride,
                 int dstW, int dstH,
                 int depth, unsigned char * mask, int deintMethod);

void AlphaBlend(uint8_t *dest,uint8_t *P1,uint8_t *P2,
       uint8_t *alpha,uint16_t count);
   // performes alpha blending in software

uint64_t getTimeMilis(void);

void mmx_unpack_16rgb (uint8_t * image, int lines, int stride);
void mmx_unpack_15rgb (uint8_t * image, int lines, int stride);
void mmx_unpack_24rgb (uint8_t * image, int lines, int stride);
extern void (*mmx_unpack)(uint8_t * image, int lines, int stride);


void * fast_memcpy(void * to, const void * from, size_t len);
#endif

