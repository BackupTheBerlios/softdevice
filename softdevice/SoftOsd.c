/*
 * softdevice plugin for VDR
 *
 * Copyright (C) 2005 Martin Wache
 *
 * This code is distributed under the terms and conditions of the
 * GNU GENERAL PUBLIC LICENSE. See the file COPYING for details.
 *
 * $Id: SoftOsd.c,v 1.25 2006/12/03 19:55:15 wachm Exp $
 */
#include <assert.h>
#include "SoftOsd.h"
#include "utils.h"

#ifdef HAVE_CONFIG
# include "config.h"
#endif

//#define OSDDEB(out...) {printf("soft_osd[%04d]:",(int)(getTimeMilis() % 10000));printf(out);}

#ifndef OSDDEB
#define OSDDEB(out...)
#endif

#define COLOR_64BIT(x) ( ((x)<<32) | (x) )
#define ALPHA_VALUE(x) ( (x) << 24 )

// the same constants for MMX mode
static uint64_t transparent_thr= COLOR_64BIT(ALPHA_VALUE(TRANSPARENT_THRESHOLD>>1));
static uint64_t opacity_thr= COLOR_64BIT(ALPHA_VALUE(OPACITY_THRESHOLD>>1));
static uint64_t pseudo_transparent = COLOR_64BIT(COLOR_KEY);

int cSoftOsd::colorkey;

//#undef USE_MMX
//#undef USE_MMX2

#undef SPLAT_U16
#ifdef USE_MMX2
#define SPLAT_U16(X)   " pshufw $0b0, " X ", " X " \n"
#else
#define SPLAT_U16(X)   " punpcklwd " X ", " X " \n"\
                       " punpckldq " X ", " X " \n"
#endif

/* ---------------------------------------------------------------------------
 */

cSoftOsd::cSoftOsd(cVideoOut *VideoOut, int X, int Y)
        : cOsd(X, Y),active(false),close(false) {
        OSDDEB("cSoftOsd constructor\n");
        OutputConvert=&cSoftOsd::ARGB_to_ARGB32;
        pixelMask=NULL;
        bitmap_Format=PF_None; // forces a clear after first SetMode
        OSD_Bitmap=new uint32_t[OSD_STRIDE*(OSD_HEIGHT+4)];

        videoOut = VideoOut;
        xPan = yPan = 0;
        voutMutex.Lock();
        videoOut->OpenOSD();
        colorkey=videoOut->GetOSDColorkey();
        pseudo_transparent=(uint64_t)colorkey | ((uint64_t) colorkey)<<32;
        
        xOfs=X;yOfs=Y;
        ScreenOsdWidth=ScreenOsdHeight=0;
        int Depth=16; bool HasAlpha=false; bool AlphaInversed=false;
        bool IsYUV=false; uint8_t *PixelMask=NULL;
        videoOut->AdjustOSDMode();
        videoOut->GetOSDMode(Depth,HasAlpha,AlphaInversed,
                        IsYUV,PixelMask);
        SetMode(Depth,HasAlpha,AlphaInversed,
                        IsYUV,PixelMask);
        voutMutex.Unlock();
};

/*--------------------------------------------------------------------------*/
void cSoftOsd::Clear() {
        OSDDEB("Clear\n");
        uint32_t blank=0x00000000; //COLOR_KEY;
        ConvertPalette((tColor *)&blank,(tColor *)&blank,1);

        register uint32_t fill=blank;
        for (int i=OSD_STRIDE*(OSD_HEIGHT+2)-1; i!=0; i--)
                OSD_Bitmap[i]=fill;
        OSD_Bitmap[0]=fill;

        // no dirty lines, everything has to be redrawn anyway
        memset(dirty_lines,false,sizeof(dirty_lines));
}

/* --------------------------------------------------------------------------*/
cSoftOsd::~cSoftOsd() {
        OSDDEB("cSoftOsd destructor\n");
        close=true;
        active=false;
        Cancel(3);
        if (videoOut) {
                voutMutex.Lock();
                videoOut->CloseOSD();
#ifdef HAVE_YAEPGPATCH
                if (vidWin.bpp!=0) 
                        videoOut->SetVidWin(0,0,0,0,0);
#endif
                videoOut=0;
                voutMutex.Unlock();
        }
        delete[] OSD_Bitmap;
}

/* -------------------------------------------------------------------------*/
void cSoftOsd::Action() {
        OSDDEB("OSD thread started\n");
        active=true;
        while(active && videoOut && !close) {
                int newOsdWidth;
                int newOsdHeight;
                int newXPan, newYPan;

                voutMutex.Lock();
                if (!videoOut) {
                        voutMutex.Unlock();
                        continue;
                };

                videoOut->AdjustOSDMode();
                videoOut->GetOSDDimension(newOsdWidth,newOsdHeight,newXPan,newYPan);
                if ( newOsdWidth==-1 || newOsdHeight==-1 )
                {
                        newOsdWidth=OSD_FULL_WIDTH;
                        newOsdHeight=OSD_FULL_HEIGHT;
                }

                int Depth=16; bool HasAlpha=false; bool AlphaInversed=false;
                bool IsYUV=false; uint8_t *PixelMask=NULL;
                videoOut->GetOSDMode(Depth,HasAlpha,AlphaInversed,
                                IsYUV,PixelMask);
                bool modeChanged=SetMode(Depth,HasAlpha,AlphaInversed,
                                IsYUV,PixelMask);

                if (newXPan != xPan || newYPan != yPan) {
                        xPan = newXPan;
                        yPan = newYPan;
                        modeChanged = true;
                }

                if ( ScreenOsdWidth!=newOsdWidth  ||
                                ScreenOsdHeight!=newOsdHeight  ||
                                modeChanged ) {
                        OSDDEB("Resolution or mode changed!\n");
                        if (modeChanged)
                                videoOut->ClearOSD();
                        OsdCommit();
                }
                voutMutex.Unlock();

                usleep(17000);
        }
        OSDDEB("OSD thread ended\n");
}

/*--------------------------------------------------------------------------*/
void cSoftOsd::OsdCommit() {
        OSDDEB("OsdCommit()\n");
        int newX;
        int newY;
        int newXPan, newYPan;
        bool RefreshAll=false;

        videoOut->AdjustOSDMode();
        videoOut->GetOSDDimension(newX,newY,newXPan,newYPan);
        if ( newX==-1 || newY==-1 ) {
                newX=OSD_FULL_WIDTH;
                newY=OSD_FULL_HEIGHT;
        }
        if (newX!=ScreenOsdWidth || newY!=ScreenOsdHeight) {
                ScreenOsdWidth=newX;
                ScreenOsdHeight=newY;
                RefreshAll=true;
        };

        int Depth=16; bool HasAlpha=false; bool AlphaInversed=false;
        bool IsYUV=false; uint8_t *PixelMask=NULL;
        videoOut->GetOSDMode(Depth,HasAlpha,AlphaInversed,IsYUV,PixelMask);
        bool modeChanged=SetMode(Depth,HasAlpha,AlphaInversed,IsYUV,PixelMask);

        if (newXPan != xPan || newYPan != yPan) {
                xPan = newXPan;
                yPan = newYPan;
                RefreshAll = true;
                modeChanged = true;
        }

        if (modeChanged)
                videoOut->ClearOSD();

        if (IsYUV) {
                uint8_t *osdPy; uint8_t *osdPu; uint8_t *osdPv;
                uint8_t *osdPAlphaY; uint8_t *osdPAlphaUV;
                int strideY; int strideUV;
                videoOut->GetLockSoftOsdSurface(osdPy,osdPu,osdPv,
                                osdPAlphaY,osdPAlphaUV,strideY,strideUV);
                if (osdPy)
                        CopyToBitmap(osdPy,osdPv,osdPu,
                                        osdPAlphaY,osdPAlphaUV,
                                        strideY,strideUV,
                                        ScreenOsdWidth,ScreenOsdHeight,
                                        RefreshAll);
                videoOut->CommitUnlockSoftOsdSurface(ScreenOsdWidth,
                                ScreenOsdHeight);
        } else {
                uint8_t *osd; int stride; bool *dirtyLines;
                videoOut->GetLockOsdSurface(osd,stride,dirtyLines);
                if (osd)
                        CopyToBitmap(osd,stride,ScreenOsdWidth,ScreenOsdHeight,
                                        RefreshAll,dirtyLines);
                videoOut->CommitUnlockOsdSurface();
        }
};

/* --------------------------------------------------------------------------*/
bool cSoftOsd::SetMode(int Depth, bool HasAlpha, bool AlphaInversed,
                bool IsYUV,uint8_t *PixelMask) {
        //OSDDEB("SetMode Depth %d HasAlpha %d IsYUV %d AlphaInversed %d PixelMask %p\n",
        //                Depth,HasAlpha,IsYUV,AlphaInversed,PixelMask);

        if (IsYUV) {
                if ( bitmap_Format!= PF_AYUV) {
                        // format change, redraw everything
                        bitmap_Format= PF_AYUV;
                        Clear();
                        FlushBitmaps(false);
                        return true;
                };
                return false;
        };

        pixelMask=PixelMask;
        switch (Depth) {
                case 32: if (HasAlpha)
                                 OutputConvert=&cSoftOsd::ARGB_to_ARGB32;
                         else OutputConvert=&cSoftOsd::ARGB_to_RGB32;
                         break;
                case 24:
                         HasAlpha=false;
                         OutputConvert=&cSoftOsd::ARGB_to_RGB24;
                         break;
                case 16:
                         HasAlpha=false;
                         if (PixelMask)
                                 OutputConvert=&cSoftOsd::ARGB_to_RGB16_PixelMask;
                         else
                                 OutputConvert=&cSoftOsd::ARGB_to_RGB16;
                         break;
                default:
                         OutputConvert=&cSoftOsd::ARGB_to_RGB16;
        };

        PixFormat old_bitmap_Format=bitmap_Format;

        if (HasAlpha) {
                if ( AlphaInversed )
                        bitmap_Format=PF_inverseAlpha_ARGB32;
                else bitmap_Format=PF_ARGB32;
        } else bitmap_Format=PF_pseudoAlpha_ARGB32;

        // we have to redraw everything on format change...
        if (old_bitmap_Format != bitmap_Format) {
                Clear();
                FlushBitmaps(false);
                return true;
        };

        return false;
};


/* --------------------------------------------------------------------------*/
void cSoftOsd::Flush(void) {
        OSDDEB("SoftOsd::Flush \n");
        bool OSD_changed=FlushBitmaps(true);

        voutMutex.Lock();
#ifdef HAVE_YAEPGPATCH
        if (vidWin.bpp!=0) 
                videoOut->SetVidWin(1,vidWin.x1,vidWin.y1,vidWin.x2,vidWin.y2);
#endif
        if (OSD_changed)
                OsdCommit();
        voutMutex.Unlock();

        // give priority to the other threads
        pthread_yield();
        if (!active && !close)
                Start();
}

/* -------------------------------------------------------------------------*/
bool cSoftOsd::FlushBitmaps(bool OnlyDirty) {
        cBitmap *Bitmap;
        bool OSD_changed=false;
        OSDDEB("FlushBitmaps (OnlyDirty: %d)\n",OnlyDirty);

        for (int i = 0; (Bitmap = GetBitmap(i)) != NULL; i++) {
                OSD_changed |= DrawConvertBitmap(Bitmap,OnlyDirty);
        }
        return OSD_changed;
};

/*--------------------------------------------------------------------------*/
void cSoftOsd::ConvertPalette(tColor *palette, const tColor *orig_palette,
                int maxColors) {

        // Handle YUV format
        if (bitmap_Format == PF_AYUV) {
                ARGB_to_AYUV((uint32_t*)palette,(color *)orig_palette,
                                maxColors);
                return;
        };

        memcpy(palette,orig_palette,sizeof(tColor)*maxColors);

        if ( bitmap_Format == PF_pseudoAlpha_ARGB32 ) {
                for (int i=0; i<maxColors; i++) {
                        // replace black with first non-transparent color
                        if ( (uint32_t)palette[i] &&
                            ((uint32_t)palette[i] & 0x00FFFFFF) == 0x000000 ) {
                                palette[i] |= 0x00101010;
                        };

                       // replace transparent colors with color key
                        if ( IS_TRANSPARENT( GET_A((uint32_t)palette[i]) ) ) {
                        //if (((uint32_t)palette[i] & 0xFF000000) == 0x000000 ) {
                                palette[i] = 0x00000000; // color key;
                        };
                };
        } else if ( bitmap_Format == PF_inverseAlpha_ARGB32 ) {
                for (int i=0; i<maxColors; i++) {
                        int a=GET_A(((color*) palette)[i]);
                        ((color*) palette)[i]&=~SET_A(0xFF);
                        ((color*) palette)[i]|=SET_A(0xFF-a);
                        //((color*) palette)[i].a=0xFF-((color*) palette)[i].a;
                };
        };

};

/*--------------------------------------------------------------------------*/
bool cSoftOsd::DrawConvertBitmap(cBitmap *bitmap, bool OnlyDirty)  {
        int x1,x2,y1,y2;
        cMutexLock dirty(&dirty_Mutex);
        OSDDEB("DrawConvertBitmap %p, OnlyDirty %d\n",bitmap,OnlyDirty);

        if ( !bitmap->Dirty(x1,y1,x2,y2) && OnlyDirty)
                return false;

        if (!OnlyDirty) {
                x1=0;
                x2=bitmap->Width()-1;
                y1=0;
                y2=bitmap->Height()-1;
        };

        int maxColors;
        const tColor *orig_palette=bitmap->Colors(maxColors);
        tColor palette[256];
        ConvertPalette(palette,orig_palette,maxColors);
        /*
        for (int i =0; i< maxColors; i++)
           printf("color[%d]: 0x%08x \n",i,palette[i]);
          */
        OSDDEB("drawing bitmap %p at P0 (%d,%d) from (%d,%d) to (%d,%d) \n",
                        bitmap,bitmap->X0(),bitmap->Y0(),x1,y1,x2,y2);

        y2++;
        x2++;
        y2= yOfs+y2+bitmap->Y0() > OSD_HEIGHT ?
                OSD_HEIGHT-bitmap->Y0()-yOfs : y2;
        x2= xOfs+x2+bitmap->X0() > OSD_WIDTH ?
                OSD_WIDTH-bitmap->X0()-xOfs : x2;

        int bitmap_yOfs=yOfs+bitmap->Y0()+Y_OFFSET;
        uint32_t *OSD_pointer=&OSD_Bitmap[(bitmap_yOfs+y1)*OSD_STRIDE+
                xOfs+bitmap->X0()+x1+X_OFFSET];
        int missing_line_length=OSD_STRIDE-(x2-x1);
        bool *dirty_line=&dirty_lines[bitmap_yOfs+y1];

        for (int y=y1; y<y2; y++) {
                for (int x=x1; x<x2; x++) {
                        *(OSD_pointer++)=palette[*bitmap->Data(x,y)];
                }
                OSD_pointer+=missing_line_length;
                *(dirty_line++)=true;
        };
        bitmap->Clean();
        return true;
};

/*----------------------------------------------------------------------*/

void cSoftOsd::ARGB_to_AYUV(uint32_t * dest, color * pixmap, int Pixel) {
        int Y;
        int U;
        int V;
        int a,r,g,b;
        int c;
        while (Pixel>0) {
                a=GET_A(*pixmap); r=GET_R(*pixmap);
                g=GET_G(*pixmap); b=GET_B(*pixmap);

                //printf("ARGB: %02x,%02x,%02x,%02x ",a,r,g,b);
                // I got this formular from Wikipedia...
                Y = (( 66 * r + 129 * g + 25 * b + 128 )  >> 8)+16;
                U = (( -38 * r - 74 * g +112 * b + 128 )  >> 8)+128;
                V = (( 112 * r - 94 * g - 18 * b + 128 )  >> 8)+128;
                c = SET_A(a) | SET_R(Y) | SET_G(U) | SET_B(V);
                *dest++ = c;
                //printf("->AYUV: %0x,%02x,%02x,%02x\n",pixmap->a,Y,U,V);
                pixmap++;
                Pixel--;
        };
};
 
/*---------------------------------------------------------------------*/
// pseudo alpha blending macros 
#define PSEUDO_ALPHA_TO_RGB(rgb) \
        if ( !c ||       \
             (IS_BACKGROUND(GET_A(c)) && (!!((intptr_t)dest % (2*SIZE_##rgb)) ^ odd )) ) {   \
                WRITE_##rgb(dest,GET_R(colorkey),GET_G(colorkey),        \
                                GET_B(colorkey));             \
                /* color key! */                               \
        } else {                                               \
                WRITE_##rgb(dest,GET_R(c),GET_G(c),GET_B(c));  \
        };                                                     \
        dest+=SIZE_##rgb;

#define PSEUDO_ALPHA_MMX_EVEN \
                 /* second  and forth pixel */                  \
                 "movd 4(%0), %%mm1\n"  /* load even pixels  */ \
                 "punpckldq 12(%0), %%mm1\n"                    \
                                                                \
                 "movq %%mm1,%%mm0\n"                           \
                 "movq %%mm1,%%mm2\n"                           \
                 "psrlw $1,%%mm0 \n"                            \
                 "psrlw $1,%%mm2 \n"                            \
                 "pcmpgtb %%mm5,%%mm0 \n"                       \
                 "pcmpgtb %%mm6,%%mm2 \n"                       \
                                                                \
                 "pandn %%mm0,%%mm2 \n"                         \
                 "psraw $8,%%mm2 \n"                            \
                 "pshufw $0b11110101,%%mm2,%%mm3 \n"            \
                 "pshufw $0b11110101,%%mm2,%%mm4 \n"            \
                                                                \
                 "movd 0(%0), %%mm2\n" /* load odd pixels */    \
                 "pandn %%mm1,%%mm4 \n"                         \
                 "pand %%mm7,%%mm3 \n"                          \
                 "por %%mm3,%%mm4\n"                            \
                 "punpckldq 8(%0), %%mm3\n" /* load odd pixels*/\
                                                                \
                 "punpckldq %%mm4,%%mm2 \n"                     \
                 "punpckhdq %%mm4,%%mm3 \n"                     \

#define PSEUDO_ALPHA_MMX_ODD \
                 /* second  and forth pixel */                  \
                 "movd 0(%0), %%mm1\n"  /* load odd pixels  */ \
                 "punpckldq 8(%0), %%mm1\n"                    \
                                                                \
                 "movq %%mm1,%%mm0\n"                           \
                 "movq %%mm1,%%mm2\n"                           \
                 "psrlw $1,%%mm0 \n"                            \
                 "psrlw $1,%%mm2 \n"                            \
                 "pcmpgtb %%mm5,%%mm0 \n"                       \
                 "pcmpgtb %%mm6,%%mm2 \n"                       \
                                                                \
                 "pandn %%mm0,%%mm2 \n"                         \
                 "psraw $8,%%mm2 \n"                            \
                 "pshufw $0b11110101,%%mm2,%%mm3 \n"            \
                 "pshufw $0b11110101,%%mm2,%%mm2 \n"            \
                                                                \
                 "movd 4(%0), %%mm4\n" /* load odd pixels */    \
                 "pandn %%mm1,%%mm2 \n"                         \
                 "pand %%mm7,%%mm3 \n"                          \
                 "por %%mm3,%%mm2\n"                            \
                 "punpckldq 12(%0), %%mm4\n" /* load odd pixels*/\
                 "movq %%mm2, %%mm3 \n"                         \
                                                                \
                 "punpckldq %%mm4,%%mm2 \n"                     \
                 "punpckhdq %%mm4,%%mm3 \n"                     

#define REPLACE_COLORKEY_MMX           \
                 "pxor %%mm0, %%mm0 \n"                         \
                 "pxor %%mm1, %%mm1 \n"                         \
                 "pcmpeqd %%mm2, %%mm0 \n"                      \
                 "pcmpeqd %%mm3, %%mm1 \n"                      \
                 "movq %%mm0, %%mm4 \n"                         \
                 "pand %%mm7, %%mm0 \n"                         \
                 "pandn %%mm2, %%mm4 \n"                        \
                 "por %%mm4, %%mm0\n"                           \
                 "movq %%mm1, %%mm4 \n"                         \
                 "pand %%mm7, %%mm1 \n"                         \
                 "pandn %%mm3, %%mm4 \n"                        \
                 "por %%mm4, %%mm1\n"                           
 
/*---------------------------------------------------------------------*/
void cSoftOsd::ARGB_to_ARGB32(uint8_t * dest, color * pixmap, int Pixel,
                int odd) {
        memcpy(dest,pixmap,Pixel*4);
};

/*----------------------------------------------------------------------*/

void cSoftOsd::ARGB_to_RGB32(uint8_t * dest, color * pixmap, int Pixel,
                int odd) {
        uint8_t *end_dest=dest+4*Pixel;
#ifdef USE_MMX2
        end_dest-=16;
        __asm__(
                        "movq (%0),%%mm5 \n"
                        "movq (%1),%%mm6 \n"
                        "movq (%2),%%mm7 \n"
                        : :
                        "r" (&transparent_thr),"r" (&opacity_thr),
                        "r" (&pseudo_transparent)
               );

        if (odd) 
                while (end_dest>dest) {
                        // pseudo alpha blending
                        __asm__(
                                        PSEUDO_ALPHA_MMX_ODD
                                        REPLACE_COLORKEY_MMX

                                        "movq %%mm0, (%1) \n"
                                        "movq %%mm1, 8(%1) \n"
                                        : :
                                        "r" (pixmap),"r" (dest));
                        dest+=16;
                        pixmap+=4;
                }
        else while (end_dest>dest) {
                // pseudo alpha blending
                __asm__(
                                PSEUDO_ALPHA_MMX_EVEN
                                REPLACE_COLORKEY_MMX

                                "movq %%mm0, (%1) \n"
                                "movq %%mm1, 8(%1) \n"
                                : :
                                "r" (pixmap),"r" (dest));
                dest+=16;
                pixmap+=4;
        };
        EMMS;
        end_dest+=16;
#endif
        int c;
        while (end_dest>dest) {
                c=*pixmap;
                PSEUDO_ALPHA_TO_RGB(RGB32);
                pixmap++;
        };
};

/*---------------------------------------------------------------------------*/

void cSoftOsd::AYUV_to_AYUV420P(uint8_t *PY1, uint8_t *PY2,
                    uint8_t *PU, uint8_t *PV,
                    uint8_t *PAlphaY1,uint8_t *PAlphaY2,
                    uint8_t *PAlphaUV,
                    color * pixmap1, color * pixmap2, int Pixel) {

#ifdef USE_MMX2
        __asm__(
                " pxor %%mm7,%%mm7 \n" //mm7: 00 00 00 ...
                : : : "memory" );
        // "memory" is not really needed but g++-2.95 wants to have it :-(


        while (Pixel>8*30) {
                __asm__ (
                    " prefetchnta 96(%0) \n"
                    " prefetchnta 96(%1) \n"
                    : : "r" (pixmap1), "r" (pixmap2) );

                __asm__(
                    " movd  (%0),%%mm0\n"
                    " punpcklbw 4(%0),%%mm0\n"// mm0: 1A 2A 1Y 2Y 1U 2U 1V 2V
                    " movd  8(%0),%%mm1\n"
                    " punpcklbw 12(%0),%%mm1\n"// mm1: 3A 4A 3Y 4Y 3U 4U 3V 4V

                    " movq %%mm0, %%mm2 \n"
                    " punpckhwd %%mm1, %%mm2 \n" //mm2: 1A 2A 3A 4A 1Y 2Y 3Y 4Y
                    " movd %%mm2, (%2) \n" // store alpha  values 1-4 first line
                    " punpckhdq %%mm2, %%mm2 \n"
                    " movd %%mm2, (%1) \n" // store luminance values 1-4 first line
                    " punpcklwd %%mm1, %%mm0 \n" //mm0: 1U 2U 3U 4U 1V 2V 3V 4V
                    " movq %%mm0, %%mm1  \n"
                    " punpckhbw %%mm7, %%mm0 \n" //mm0: 1V 00 2V 00 3V 00 4V 00
                    " punpcklbw %%mm7, %%mm1 \n" //mm1: 1U 00 2U 00 3U 00 4U 00
                    : : "r" (pixmap1),
                        "r" (PAlphaY1),
                        "r" (PY1) : "memory");

                __asm__(
                    // second line
                    " movd  (%0),%%mm3\n"
                    " punpcklbw 4(%0),%%mm3\n"// mm3: 1A 2A 1Y 2Y 1U 2U 1V 2V
                    " movd  8(%0),%%mm4\n"
                    " punpcklbw 12(%0),%%mm4\n"//mm4: 3A 4A 3Y 4Y 3U 4U 3V 4V

                    " movq %%mm3, %%mm2 \n"
                    " punpckhwd %%mm4, %%mm2 \n" //mm2: 1A 2A 3A 4A 1Y 2Y 3Y 4Y
                    " movd %%mm2, (%2) \n" // store alpha 1-4 second line
                    " punpckhdq %%mm2, %%mm2 \n"
                    " movd %%mm2, (%1) \n" // store luminance 1-4 second line

                    " punpcklwd %%mm4, %%mm3 \n" //mm3: 1U 2U 3U 4U 1V 2V 3V 4V

                    " movq %%mm3, %%mm4  \n"
                    " punpckhbw %%mm7, %%mm3 \n" //mm0: 1V 00 2V 00 3V 00 4V 00
                    " punpcklbw %%mm7, %%mm4 \n" //mm1: 1U 00 2U 00 3U 00 4U 00

                    // add U and V component of first and second line
                    " paddusw %%mm3, %%mm0 \n"
                    " paddusw %%mm4, %%mm1 \n"

                    " pshufw $0b11011101,%%mm0,%%mm2 \n" //mm2: 2V 00 4V 00
                    " pshufw $0b10001000,%%mm0,%%mm6 \n" //mm6: 1V 00 3V 00
                    " paddusw %%mm2, %%mm6 \n" // mm6: 1V+2V  3V+4V
                    " psraw $2, %%mm6 \n" // mm6 div 4

                    " pshufw $0b11011101,%%mm1,%%mm2 \n" //mm2: 2U 00 4U 00
                    " pshufw $0b10001000,%%mm1,%%mm1 \n" //mm1: 1U 00 3U 00
                    " paddusw %%mm2, %%mm1 \n" // mm1: 1U+2U 3U+4U
                    " psraw $2, %%mm1 \n" // mm1 div 4

                    " packuswb %%mm1,%%mm6 \n" // mm6: 1u+2u 3u+4u XX XX 1v+2v 3v+4v XX XX
                    : : "r" (pixmap2),
                        "r" (PAlphaY2),
                        "r" (PY2): "memory");

                // next 4 pixels
                __asm__(
                    // inventory: mm6: 2 pixels u and v values
                    " movd  16(%0),%%mm0\n"
                    " punpcklbw 20(%0),%%mm0\n"// mm0: 1A 2A 1Y 2Y 1U 2U 1V 2V
                    " movd  24(%0),%%mm1\n"
                    " punpcklbw 28(%0),%%mm1\n"// mm1: 3A 4A 3Y 4Y 3U 4U 3V 4V

                    " movq %%mm0, %%mm2 \n"
                    " punpckhwd %%mm1, %%mm2 \n" //mm2: 1A 2A 3A 4A 1Y 2Y 3Y 4Y
                    " movd %%mm2, 4(%2) \n" // store alpha  values 1-4 first line
                    " punpckhdq %%mm2, %%mm2 \n"
                    " movd %%mm2, 4(%1) \n" // store luminance values 1-4 first line
                    " punpcklwd %%mm1, %%mm0 \n" //mm0: 1U 2U 3U 4U 1V 2V 3V 4V
                    " movq %%mm0, %%mm1  \n"
                    " punpckhbw %%mm7, %%mm0 \n" //mm0: 1V 00 2V 00 3V 00 4V 00
                    " punpcklbw %%mm7, %%mm1 \n" //mm1: 1U 00 2U 00 3U 00 4U 00
                    : : "r" (pixmap1),
                        "r" (PAlphaY1),
                        "r" (PY1) : "memory");

                __asm__(
                    // second line
                    " movd  16(%0),%%mm3\n"
                    " punpcklbw 20(%0),%%mm3\n"//mm3: 1A 2A 1Y 2Y 1U 2U 1V 2V
                    " movd  24(%0),%%mm4\n"
                    " punpcklbw 28(%0),%%mm4\n"//mm4: 3A 4A 3Y 4Y 3U 4U 3V 4V

                    " movq %%mm3, %%mm2 \n"
                    " punpckhwd %%mm4, %%mm2 \n" //mm2: 1A 2A 3A 4A 1Y 2Y 3Y 4Y
                    " movd %%mm2, 4(%2) \n" // store alpha 1-4 second line
                    " punpckhdq %%mm2, %%mm2 \n"
                    " movd %%mm2, 4(%1) \n" // store luminance 1-4 second line

                    " punpcklwd %%mm4, %%mm3 \n" //mm3: 1U 2U 3U 4U 1V 2V 3V 4V

                    " movq %%mm3, %%mm4  \n"
                    " punpckhbw %%mm7, %%mm3 \n" //mm0: 1V 00 2V 00 3V 00 4V 00
                    " punpcklbw %%mm7, %%mm4 \n" //mm1: 1U 00 2U 00 3U 00 4U 00

                    // add U and V component of first and second line
                    " paddusw %%mm3, %%mm0 \n"
                    " paddusw %%mm4, %%mm1 \n"

                    " pshufw $0b11011101,%%mm0,%%mm2 \n" //mm2: 2V 00 4V 00
                    " pshufw $0b10001000,%%mm0,%%mm0 \n" //mm0: 1V 00 3V 00
                    " paddusw %%mm2, %%mm0 \n" // mm0: 1V+2V  3V+4V
                    " psraw $2, %%mm0 \n" // mm0 div 4

                    " pshufw $0b11011101,%%mm1,%%mm2 \n" //mm2: 2U 00 4U 00
                    " pshufw $0b10001000,%%mm1,%%mm1 \n" //mm1: 1U 00 3U 00
                    " paddusw %%mm2, %%mm1 \n" // mm1: 1U+2U 3U+4U
                    " psraw $2, %%mm1 \n" // mm1 div 4

                    " packuswb %%mm1,%%mm0 \n" // mm0: 1u+2u 3u+4u XX XX 1v+2v 3v+4v XX XX
                    : : "r" (pixmap2),
                        "r" (PAlphaY2),
                        "r" (PY2): "memory");

                // now care about the missing u and v components
                __asm__(
                    // inventory:
                    // mm6: first 2 pixels u and v values
                    // mm0: second 2 pixels u and v values
                    " movq %%mm6, %%mm2 \n"
                    " punpckhwd %%mm0, %%mm6\n"
                    " punpcklwd %%mm0, %%mm2\n"
                    " movd %%mm2, (%1)\n"
                    " movd %%mm6, (%0) \n"
                    : : "r" (PU), "r" (PV) : "memory" );

                // and calculate PAlphaUV from PAlphaY1 and PAlphaY2
                __asm__(
                    " movd (%0), %%mm0 \n"
                    " punpcklbw %%mm7, %%mm0 \n"
                    " movd (%1), %%mm1 \n"
                    " punpcklbw %%mm7, %%mm1 \n"
                    " paddusw %%mm1, %%mm0 \n" // mm1: PAlphaY1+PAlphaY2

                    " pshufw $0b11011101,%%mm0,%%mm1 \n" //mm2: 2A 00 4A 00
                    " pshufw $0b10001000,%%mm0,%%mm0 \n" //mm0: 1A 00 3A 00
                    " paddusw %%mm1, %%mm0 \n" // mm0: 1A+2A  3A+4A
                    " psraw $2, %%mm0 \n" // mm0 div 4
                    " packuswb %%mm0,%%mm0 \n"

                    " movd 4(%0), %%mm2 \n"
                    " punpcklbw %%mm7, %%mm2 \n"
                    " movd 4(%1), %%mm3 \n"
                    " punpcklbw %%mm7,%%mm3 \n"
                    " paddusw %%mm3, %%mm2 \n" // mm2: PAlphaY1+PAlphaY2

                    " pshufw $0b11011101,%%mm2,%%mm1 \n" //mm2: 6A 00 8A 00
                    " pshufw $0b10001000,%%mm2,%%mm3 \n" //mm0: 5A 00 7A 00
                    " paddusw %%mm1, %%mm3 \n" // mm0: 5A+6A  7A+8A
                    " psraw $2, %%mm3 \n" // mm0 div 4
                    " packuswb %%mm3,%%mm3 \n"

                    " punpckhwd %%mm3, %%mm0\n"
                    " movd %%mm0, (%2)\n"

                     : : "r" (PAlphaY1), "r" (PAlphaY2),
                         "r" (PAlphaUV)
                                 : "memory" );

                pixmap1+=8;
                pixmap2+=8;
                PAlphaY1+=8;PAlphaY2+=8;
                PY1+=8;PY2+=8;
                PU+=4;PV+=4;
                PAlphaUV+=4;
                Pixel-=8;
        };
        EMMS;
#endif
        int p10,p11;
        int p20,p21;
        while (Pixel>1) {
                p10=pixmap1[0]; p11=pixmap1[1];
                p20=pixmap2[0]; p21=pixmap2[1];

                *(PAlphaY1++)=GET_A(p10); *(PAlphaY1++)=GET_A(p11);
                *(PAlphaY2++)=GET_A(p20); *(PAlphaY2++)=GET_A(p21);
                *(PAlphaUV++)=(uint8_t)((unsigned int)
                        (GET_A(p10)+GET_A(p11)+
                         GET_A(p20)+GET_A(p21))>>2);

                *(PY1++)=GET_R(p10); *(PY1++)=GET_R(p11);
                *(PY2++)=GET_R(p20); *(PY2++)=GET_R(p21);

                *(PU++)=(uint8_t)((unsigned int)
                                (GET_B(p10) + GET_B(p11) +
                                  GET_B(p20)+ GET_B(p21))>>2);
                *(PV++)=(uint8_t)((unsigned int)
                                  (GET_G(p10)+GET_G(p11)+
                                   GET_G(p20)+GET_G(p21))>>2);

                pixmap1+=2;
                pixmap2+=2;
                Pixel-=2;
        };
};

/*---------------------------------------------------------------------------*/

void cSoftOsd::ARGB_to_RGB24(uint8_t * dest, color * pixmap, int Pixel,
                int odd) {
        int c;
        while (Pixel) {
                c = *pixmap;
                PSEUDO_ALPHA_TO_RGB(RGB24);
                Pixel--;
                pixmap++;
        };
};

/*---------------------------------------------------------------------------*/

void cSoftOsd::ARGB_to_RGB16(uint8_t * dest, color * pixmap, int Pixel,
                int odd) {
        uint8_t *end_dest=dest+2*Pixel;
#ifdef USE_MMX2
        static uint64_t rb_mask =   {0x00f800f800f800f8LL};
        static uint64_t g_mask =    {0xfc00fc00fc00fc00LL};
        //static uint64_t g_mask =    {0xf800f800f800f800LL};

        end_dest-=8;
        __asm__(
                        "movq (%0),%%mm5 \n"
                        "movq (%1),%%mm6 \n"
                        "movq (%2),%%mm7 \n"
                        : :
                        "r" (&transparent_thr),"r" (&opacity_thr),
                        "r" (&pseudo_transparent)
               );
 
        if (odd)
                while (end_dest>dest) {
                        // pseudo alpha blending
                        __asm__(
                            PSEUDO_ALPHA_MMX_ODD
                            REPLACE_COLORKEY_MMX
                            : :
                            "r" (pixmap) );

                        // ARGB to RGB16
                        __asm__(
                            // mm0: 1A 1R 1G 1B 2A 2R 2G 2B
                            // mm1: 3A 3R 3G 3B 4A 4R 4G 4B
                            "pshufw $0b11011101,%%mm0,%%mm2\n"
                            "pshufw $0b11011101,%%mm1,%%mm3\n"
                            "punpckldq %%mm3, %%mm2\n" //mm2: alpha and r channels
                            "pshufw $0b00101000,%%mm0,%%mm3\n"
                            "pshufw $0b00101000,%%mm1,%%mm1\n"
                            "punpckldq %%mm1, %%mm3\n" //mm3: g and b channels

                            "pand (%3), %%mm2\n" //mm2 : r-komponente
                            "psllw $8,%%mm2\n"    //mm2 : r-komponente
                            "movq (%2), %%mm1\n"
                            "pand %%mm3, %%mm1\n" //mm1 : g-komponente
                            "pand (%3), %%mm3\n" //mm3: b-komponente
                            "psrlw $5,%%mm1\n" //mm3 : g-komponente ok
                            "psrlw $3,%%mm3\n" //mm1 : b-komponente

                            "por %%mm3,%%mm2\n"
                            "por %%mm1,%%mm2\n"

                            " movq %%mm2,(%1) \n"
                            : : "r" (pixmap), "r" (dest),
                            "r" (&g_mask),"r" (&rb_mask)
                            : "memory");
                        pixmap+=4;
                        dest+=8;
                }
        else while (end_dest>dest) {
                // pseudo alpha blending
                __asm__(
                            PSEUDO_ALPHA_MMX_EVEN
                            REPLACE_COLORKEY_MMX
                            : :
                            "r" (pixmap) );

                // ARGB to RGB16
                __asm__(
                       // mm0: 1A 1R 1G 1B 2A 2R 2G 2B
                       // mm1: 3A 3R 3G 3B 4A 4R 4G 4B
                       "pshufw $0b11011101,%%mm0,%%mm2\n"
                       "pshufw $0b11011101,%%mm1,%%mm3\n"
                       "punpckldq %%mm3, %%mm2\n" //mm2: alpha and r channels
                       "pshufw $0b00101000,%%mm0,%%mm3\n"
                       "pshufw $0b00101000,%%mm1,%%mm1\n"
                       "punpckldq %%mm1, %%mm3\n" //mm3: g and b channels

                       "pand (%3), %%mm2\n" //mm2 : r-komponente
                       "psllw $8,%%mm2\n"    //mm2 : r-komponente
                       "movq (%2), %%mm1\n"
                       "pand %%mm3, %%mm1\n" //mm1 : g-komponente
                       "pand (%3), %%mm3\n" //mm3: b-komponente
                       "psrlw $5,%%mm1\n" //mm3 : g-komponente ok
                       "psrlw $3,%%mm3\n" //mm1 : b-komponente

                       "por %%mm3,%%mm2\n"
                       "por %%mm1,%%mm2\n"

                       " movq %%mm2,(%1) \n"
                       : : "r" (pixmap), "r" (dest),
                       "r" (&g_mask),"r" (&rb_mask)
                       : "memory");
                pixmap+=4;
                dest+=8;
        };
        EMMS;
        end_dest+=8;
#endif
        int c;
        while (end_dest>dest) {
                c = *pixmap;
                PSEUDO_ALPHA_TO_RGB(RGB16);
                pixmap++;
        };
};
/*---------------------------------------------------------------------------*/

void cSoftOsd::ARGB_to_RGB16_PixelMask(uint8_t * dest, color * pixmap,
                int Pixel, int odd) {
        uint8_t *end_dest=dest+2*Pixel;
        int PixelCount=0;
        int c;
        while (end_dest>dest) {
                c=*pixmap;
                if ( IS_BACKGROUND(GET_A(c)) && (((intptr_t)pixmap) & 0x4)
                                || IS_TRANSPARENT(GET_A(c)) ) {
                        // transparent, don't draw anything !
                } else {
                        // FIXME 15/16bit mode? Probably broken!
                        dest[0] = ((GET_B(c) >> 3)& 0x1F) |
                                ((GET_G(c) & 0x1C) << 3);
                        dest[1] = ((GET_R(c) ) & 0xF8)
                                | ((GET_G(c) >> 5) );
                }
                dest+=2;
                pixmap++;
                PixelCount++;
        };
};

void cSoftOsd::CreatePixelMask(uint8_t * dest, color * pixmap, int Pixel) {
        int CurrPixel=0;
        int c;
        while (CurrPixel<Pixel) {
                c = *pixmap;
                if ( (IS_BACKGROUND(GET_A(c)) && !(((intptr_t)pixmap) & 0x4)  )
                                || IS_OPAQUE(GET_A(c)) )
                        dest[CurrPixel/8]|=(1<<CurrPixel%8);
                 else dest[CurrPixel/8]&=~(1<<CurrPixel%8);
                pixmap++;
                CurrPixel++;
        };
};
//---------------------------YUV modes ----------------------
void cSoftOsd::CopyToBitmap(uint8_t *PY,uint8_t *PU, uint8_t *PV,
                    uint8_t *PAlphaY,uint8_t *PAlphaUV,
                    int Ystride, int UVstride,
                    int dest_Width, int dest_Height, bool RefreshAll) {
        OSDDEB("CopyToBitmap destsize: %d,%d\n",dest_Width,dest_Height);
        if (dest_Height < 40 || dest_Width < 40)
                return;

        if (dest_Height & 0x1 !=0) {
                //printf("warning dest_Height not even!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!\n");
                dest_Height &= ~0x1;
        };

        if (dest_Height==OSD_HEIGHT)
                NoVScaleCopyToBitmap(PY,PU,PV,PAlphaY,PAlphaUV,Ystride,UVstride,
                                dest_Width,dest_Height,RefreshAll);
        else
                ScaleVDownCopyToBitmap(PY,PU,PV,PAlphaY,PAlphaUV,Ystride,UVstride,
                                dest_Width,dest_Height,RefreshAll);
};

void cSoftOsd::NoVScaleCopyToBitmap(uint8_t *PY,uint8_t *PU, uint8_t *PV,
                    uint8_t *PAlphaY,uint8_t *PAlphaUV,
                    int Ystride, int UVstride,
                    int dest_Width, int dest_Height, bool RefreshAll) {
        OSDDEB("CopyToBitmap YUV no Vscale\n");
        cMutexLock dirty(&dirty_Mutex);
        color *pixmap=(color*) OSD_Bitmap;
        int dest_Stride=(dest_Width+32) & ~0xf;
        color tmp_pixmap1[2*dest_Stride];
        color *tmp_pixmap=tmp_pixmap1;
        uint8_t *pY;uint8_t *pU;uint8_t *pV;
        uint8_t *pAlphaY; uint8_t *pAlphaUV;
        void (cSoftOsd::*ScaleHoriz)(uint32_t * dest, int dest_Width, color * pixmap,int Pixel);
        ScaleHoriz = ( dest_Width<OSD_WIDTH ? &cSoftOsd::ScaleDownHoriz_MMX :
                      ( dest_Width==OSD_WIDTH ? &cSoftOsd::NoScaleHoriz_MMX :
                        &cSoftOsd::ScaleUpHoriz_MMX));

        if (dest_Width==OSD_WIDTH)
                dest_Stride=OSD_STRIDE;

        for (int y=0; y<dest_Height; y+=2) {

                int is_dirty=RefreshAll;
                is_dirty |= dirty_lines[y] || dirty_lines[y+1];

                if (!is_dirty)
                        continue;

                if (dest_Width==OSD_WIDTH) {
                        tmp_pixmap=&pixmap[y*OSD_STRIDE];
                } else {
                        //printf("Horiz scaling line %d\n",start_row+i);
                        (this->*ScaleHoriz)((uint32_t*)tmp_pixmap,dest_Width,
                                            &pixmap[y*OSD_STRIDE],OSD_WIDTH-1);
                        (this->*ScaleHoriz)((uint32_t*)&tmp_pixmap[dest_Stride],
                                            dest_Width,
                                            &pixmap[(y+1)*OSD_STRIDE],OSD_WIDTH-1);
                };
                // convert and copy to video out OSD layer
                pY=PY+(y+yPan)*Ystride+xPan;
                pU=PU+((y+yPan)*UVstride+xPan)/2;
                pV=PV+((y+yPan)*UVstride+xPan)/2;
                pAlphaY=PAlphaY+(y+yPan)*Ystride+xPan;
                pAlphaUV=PAlphaUV+((y+yPan)*UVstride+xPan)/2;
                AYUV_to_AYUV420P(pY,pY+Ystride,pU,pV,
                                pAlphaY,pAlphaY+Ystride,pAlphaUV,
                                //              &pixmap[y*OSD_STRIDE],&pixmap[(y+1)*OSD_STRIDE],
                                //              dest_Width);
                        tmp_pixmap,&tmp_pixmap[dest_Stride],dest_Width);
        };
        memset(dirty_lines,false,sizeof(dirty_lines));
        OSDDEB("CopyToBitmap YUV no Vscale end \n");
};

#define SCALEH_IDX(x) ((scaleH_strtIdx+(x))%lines_count)
void cSoftOsd::ScaleVDownCopyToBitmap(uint8_t *PY,uint8_t *PU, uint8_t *PV,
                    uint8_t *PAlphaY,uint8_t *PAlphaUV,
                    int Ystride, int UVstride,
                    int dest_Width, int dest_Height, bool RefreshAll) {

        OSDDEB("CopyToBitmap YUV down %d,%d\n",dest_Width,dest_Height);
        // upscaleing is not supported for YUV modes
        if (dest_Width>OSD_WIDTH)
                dest_Width=OSD_WIDTH;
        if (dest_Height>OSD_HEIGHT)
                dest_Height=OSD_HEIGHT;

        if (bitmap_Format != PF_AYUV) {
                // convert bitmap to ayuv
                bitmap_Format = PF_AYUV;
                Clear();
                FlushBitmaps(false);
        };

        cMutexLock dirty(&dirty_Mutex);
        color *pixmap=(color*) OSD_Bitmap;
        color tmp_pixmap[2*OSD_STRIDE];
        uint8_t *pY;uint8_t *pU;uint8_t *pV;
        uint8_t *pAlphaY; uint8_t *pAlphaUV;

        //printf("Scale to %d,%d\n",dest_Width,dest_Height);
        const int ScaleFactor=1<<6;
        uint32_t new_pixel_height=(OSD_HEIGHT*ScaleFactor)/dest_Height;
        int lines_count=(new_pixel_height/ScaleFactor*2+4);
        color scaleH_pixmap[lines_count*OSD_STRIDE];
        color *scaleH_Reference[lines_count];
        int scaleH_lines[lines_count];
        int scaleH_strtIdx=0;
        memset(scaleH_lines,-1,sizeof(scaleH_lines));
        memset(scaleH_Reference,0,sizeof(scaleH_Reference));
        OSDDEB("new_pixel_height %d lines_count %d\n",
                        new_pixel_height,lines_count);

        for (int y=0; y<dest_Height; y+=2) {
                int start_row=new_pixel_height*y/ScaleFactor;
                int32_t start_pos=new_pixel_height*y%ScaleFactor-ScaleFactor;
                //int32_t start_pos=new_pixel_height*y-ScaleFactor*(start_row+1);

                int is_dirty=RefreshAll;
                for (int i=0; i<lines_count; i++)
                        is_dirty|=dirty_lines[start_row+i];

                if (!is_dirty)
                        continue;

                scaleH_strtIdx=SCALEH_IDX(lines_count-2);
                for (int i=0; i<lines_count; i++) {
                        scaleH_Reference[i]=&scaleH_pixmap[SCALEH_IDX(i)*OSD_STRIDE];
                        if (scaleH_lines[SCALEH_IDX(i)]!=start_row+i){
                                //printf("Horiz scaling line %d\n",start_row+i);
                                ScaleDownHoriz_MMX((uint32_t*)scaleH_Reference[i],dest_Width,
                                                &pixmap[(start_row+i)*OSD_STRIDE],OSD_WIDTH-1);
                                scaleH_lines[SCALEH_IDX(i)]=start_row+i;
                        }
                };
               /*
                   printf("strt_Idx %d scaleH_lines: ",scaleH_strtIdx);
                   for (int i=0; i<lines_count; i++)
                   printf("%d ",scaleH_lines[i]);
                   printf("\n");fflush(stdout);
                 */
                OSDDEB("start_pos %d start_row %d\n",start_pos,start_row);
                ScaleDownVert_MMX((uint32_t*)tmp_pixmap,0,
                                new_pixel_height,start_pos,
                                scaleH_Reference,dest_Width);
                                //scaleH_Reference,OSD_WIDTH-1);
                int new_start_row=new_pixel_height*(y+1)/ScaleFactor;
                int start_row_idx=new_start_row-start_row;
                start_pos=new_pixel_height*(y+1)%ScaleFactor-ScaleFactor;

                if (scaleH_lines[SCALEH_IDX(start_row_idx)]!=
                                new_start_row)
                        printf("start_row mismatch new_start_row %d : %d\n",
                                        new_start_row,
                                        scaleH_lines[SCALEH_IDX(start_row_idx)]);

                OSDDEB("start_pos %d new_start_row %d\n",start_pos,new_start_row);
                ScaleDownVert_MMX((uint32_t*)&tmp_pixmap[OSD_STRIDE],0,
                                new_pixel_height,start_pos,
                                &scaleH_Reference[start_row_idx],
                                dest_Width);
                                //OSD_WIDTH-1);

                // convert and copy to video out OSD layer
                pY=PY+(y+yPan)*Ystride+xPan;
                pU=PU+((y+yPan)*UVstride+xPan)/2;
                pV=PV+((y+yPan)*UVstride+xPan)/2;
                pAlphaY=PAlphaY+(y+yPan)*Ystride+xPan;
                pAlphaUV=PAlphaUV+((y+yPan)*UVstride+xPan)/2;
                AYUV_to_AYUV420P(pY,pY+Ystride,pU,pV,
                              pAlphaY,pAlphaY+Ystride,pAlphaUV,
                //              &pixmap[y*OSD_STRIDE],&pixmap[(y+1)*OSD_STRIDE],
                //              dest_Width);
                              tmp_pixmap,&tmp_pixmap[OSD_STRIDE],dest_Width);
        };
        memset(dirty_lines,false,sizeof(dirty_lines));
        OSDDEB("CopyToBitmap YUV down end \n");
};

//---------------------------RGB modes ----------------------

void cSoftOsd::CopyToBitmap(uint8_t *dest, int linesize,
                int dest_Width, int dest_Height, bool RefreshAll,
                bool *dirtyLines) {
        if (dest_Height < 40 || dest_Width < 40)
                return;

        if ( dest_Height==OSD_HEIGHT )
                NoVScaleCopyToBitmap(dest,linesize,dest_Width,dest_Height,
                                RefreshAll,dirtyLines);
        else if ( dest_Height>OSD_HEIGHT )
                ScaleVUpCopyToBitmap(dest,linesize,dest_Width,dest_Height,
                                RefreshAll,dirtyLines);
        else ScaleVDownCopyToBitmap(dest,linesize,dest_Width,dest_Height,
                        RefreshAll,dirtyLines);
};

// -------------------------- scale vertical up -----------------------
#define SCALEH_IDX(x) ((scaleH_strtIdx+(x))%lines_count)
void cSoftOsd::ScaleVUpCopyToBitmap(uint8_t *dest, int linesize,
                int dest_Width, int dest_Height, bool RefreshAll,
                bool *dest_dirtyLines) {

        OSDDEB("CopyToBitmap RGB up\n");
        if ( bitmap_Format == PF_AYUV ) {
                fprintf(stderr,"cSoftOsd error did not call SetMode()!\n");
                // convert bitmap to argb
                bitmap_Format = PF_ARGB32;
                Clear();
                FlushBitmaps(false);
        };

        cMutexLock dirty(&dirty_Mutex);
        const int dest_stride=(dest_Width+32)&~0xF;
        void (cSoftOsd::*ScaleHoriz)(uint32_t * dest, int dest_Width, color * pixmap,int Pixel);
        ScaleHoriz= dest_Width<OSD_WIDTH ? &cSoftOsd::ScaleDownHoriz_MMX :
                &cSoftOsd::ScaleUpHoriz_MMX;

        uint8_t *buf;
        color *pixmap=(color*) OSD_Bitmap;

        //printf("Scale to %d,%d\n",dest_Width,dest_Height);
        const int ScaleFactor=1<<6;
        uint32_t new_pixel_height=(OSD_HEIGHT*ScaleFactor)/dest_Height;
        color scaleH_pixmap[2*dest_stride];//dest_Width];
        color *scaleH_Reference[2];
        int scaleH_lines[2];
        int scaleH_strtIdx=0;
        memset(scaleH_lines,-1,sizeof(scaleH_lines));

        // FIXME why 20*dest_stride?
        color tmp_pixmap[20*dest_stride];//dest_Width];
        //color tmp_pixmap[ScaleFactor/dest_Height*dest_stride];//dest_Width];

        for (int y=0; y<dest_Height; y++) {
                int start_row=new_pixel_height*y/ScaleFactor;

                int is_dirty=RefreshAll;
                is_dirty|=dirty_lines[start_row] | dirty_lines[start_row+1];

                if (!is_dirty)
                        continue;

                int32_t start_pos=new_pixel_height*y%ScaleFactor;
                //int32_t start_pos=new_pixel_height*y-ScaleFactor*(start_row+1);
                //printf("Scaling to line %d from start_row: %d start_pos %d\n",
                //        y,start_row,start_pos);
                scaleH_strtIdx=start_row & 0x1;

                scaleH_Reference[0]=
                        &scaleH_pixmap[scaleH_strtIdx*dest_stride];
                scaleH_Reference[1]=
                        &scaleH_pixmap[!scaleH_strtIdx*dest_stride];

                if ( scaleH_lines[scaleH_strtIdx] != start_row ) {
                        (this->*ScaleHoriz)
                                ((uint32_t*)scaleH_Reference[0],dest_Width,
                                 &pixmap[(start_row+0)*OSD_STRIDE],OSD_WIDTH);
                        scaleH_lines[scaleH_strtIdx] = start_row;
                };

                (this->*ScaleHoriz)
                        ((uint32_t*)scaleH_Reference[1],dest_Width,
                         &pixmap[(start_row+1)*OSD_STRIDE],OSD_WIDTH);

                scaleH_lines[!scaleH_strtIdx] = start_row+1;
                ScaleUpVert_MMX((uint32_t*)tmp_pixmap,dest_Width,
                                new_pixel_height,start_pos,
                                scaleH_Reference,dest_Width);

                //printf("Copy to destination y: %d\n",y);
                buf=dest+y*linesize;
                (*OutputConvert)(buf,tmp_pixmap+1,dest_Width-2,y&1);
                if (pixelMask)
                        CreatePixelMask(pixelMask+y*linesize/16,
                                        tmp_pixmap+1,dest_Width-2);
                if (dest_dirtyLines)
                        dest_dirtyLines[y]=true;

                start_pos+=new_pixel_height;
                uint8_t *src=((uint8_t*)tmp_pixmap)+dest_Width*4;
                while (start_pos<ScaleFactor) {
                        //printf("Copy to destination y: %d\n",y);
                        y++;
                        buf=dest+y*linesize;
                        (*OutputConvert)(buf,((color*)src)+1,dest_Width-2,y&1);
                        if (pixelMask)
                                CreatePixelMask(pixelMask+y*linesize/16,
                                                ((color*)src)+1,dest_Width-2);
                        if (dest_dirtyLines)
                                dest_dirtyLines[y]=true;
                        start_pos+=new_pixel_height;
                        src+=dest_Width*4;
                };
        };
        memset(dirty_lines,false,sizeof(dirty_lines));
        OSDDEB("CopyToBitmap RGB up end\n");
};


//--------------------------- no vertical scale -----------------------------
void cSoftOsd::NoVScaleCopyToBitmap(uint8_t *dest, int linesize,
                int dest_Width, int dest_Height, bool RefreshAll,
                bool *dest_dirtyLines) {

        OSDDEB("CopyToBitmap RGB down\n");
        if ( bitmap_Format == PF_AYUV ) {
                fprintf(stderr,"cSoftOsd error did not call SetMode()!\n");
                // convert bitmap to argb
                bitmap_Format = PF_ARGB32;
                Clear();
                FlushBitmaps(false);
        };

        cMutexLock dirty(&dirty_Mutex);
        uint8_t *buf;
        color *pixmap=(color*) OSD_Bitmap;
        const int dest_stride=(dest_Width+32)&~0xF;
        color tmp_pixmap1[2*dest_stride];
        color *tmp_pixmap=tmp_pixmap1;

       void (cSoftOsd::*ScaleHoriz)(uint32_t * dest, int dest_Width, color * pixmap,int Pixel);
        ScaleHoriz= dest_Width<OSD_WIDTH ? &cSoftOsd::ScaleDownHoriz_MMX :
                &cSoftOsd::ScaleUpHoriz_MMX;

        for (int y=0; y<OSD_HEIGHT; y++) {

                if (!RefreshAll && !dirty_lines[y])
                        continue;

                if (dest_Width==OSD_WIDTH) {
                        tmp_pixmap=&pixmap[y*OSD_STRIDE];
                } else {
                        (this->*ScaleHoriz)((uint32_t*)tmp_pixmap,dest_Width,
                                      &pixmap[y*OSD_STRIDE],OSD_WIDTH-1);
                };

                buf=dest+y*linesize;
                //printf("copy to destination %d\n",y);
                (*OutputConvert)(buf,tmp_pixmap+1,dest_Width-2,y&1);
                if (pixelMask)
                        CreatePixelMask(pixelMask+y*linesize/16,
                                        tmp_pixmap+1,dest_Width-2);
                if (dest_dirtyLines)
                        dest_dirtyLines[y]=true;

        };
        memset(dirty_lines,false,sizeof(dirty_lines));
        OSDDEB("CopyToBitmap RGB down end\n");
};

//--------------------------- scale vertical down -----------------------------
#define SCALEH_IDX(x) ((scaleH_strtIdx+(x))%lines_count)
void cSoftOsd::ScaleVDownCopyToBitmap(uint8_t *dest, int linesize,
                int dest_Width, int dest_Height, bool RefreshAll,
                bool *dest_dirtyLines) {

        OSDDEB("CopyToBitmap RGB down\n");
        if ( bitmap_Format == PF_AYUV ) {
                fprintf(stderr,"cSoftOsd error did not call SetMode()!\n");
                // convert bitmap to argb
                bitmap_Format = PF_ARGB32;
                Clear();
                FlushBitmaps(false);
        };

        cMutexLock dirty(&dirty_Mutex);
        uint8_t *buf;
        color *pixmap=(color*) OSD_Bitmap;
        const int dest_stride=(dest_Width+32)&~0xF;
        color tmp_pixmap[2*dest_stride];

       void (cSoftOsd::*ScaleHoriz)(uint32_t * dest, int dest_Width, color * pixmap,int Pixel);
        ScaleHoriz= dest_Width<OSD_WIDTH ? &cSoftOsd::ScaleDownHoriz_MMX :
                &cSoftOsd::ScaleUpHoriz_MMX;

        //printf("Scale to %d,%d\n",dest_Width,dest_Height);
        const int ScaleFactor=1<<6;
        uint32_t new_pixel_height=(OSD_HEIGHT*ScaleFactor)/dest_Height;
        int lines_count=new_pixel_height/ScaleFactor+2;
        color scaleH_pixmap[lines_count*dest_stride];
        color *scaleH_Reference[lines_count];
        int scaleH_lines[lines_count];
        int scaleH_strtIdx=0;
        memset(scaleH_lines,-1,sizeof(scaleH_lines));

        for (int y=0; y<dest_Height; y++) {
                int start_row=new_pixel_height*y/ScaleFactor;

                int is_dirty=RefreshAll;
                for (int i=0; i<lines_count; i++)
                        is_dirty|=dirty_lines[start_row+i];

                if (!is_dirty)
                        continue;

                int32_t start_pos=new_pixel_height*y%ScaleFactor-ScaleFactor;
                //int32_t start_pos=new_pixel_height*y-ScaleFactor*(start_row+1);
                //printf("Scaling to line %d from start_row: %d lines_count %d\n",
                //        y,start_row,lines_count);
                scaleH_strtIdx=SCALEH_IDX(lines_count-2);
                for (int i=0; i<lines_count; i++) {
                        scaleH_Reference[i]=&scaleH_pixmap[SCALEH_IDX(i)*dest_stride];
                        if (scaleH_lines[SCALEH_IDX(i)]!=start_row+i){
                                //printf("Horiz scaling line %d\n",start_row+i);
                                (this->*ScaleHoriz)((uint32_t*)scaleH_Reference[i],dest_Width,
                                                &pixmap[(start_row+i)*OSD_STRIDE],OSD_WIDTH-1);
                                scaleH_lines[SCALEH_IDX(i)]=start_row+i;
                        }
                };
                ScaleDownVert_MMX((uint32_t*)tmp_pixmap,linesize,
                                new_pixel_height,start_pos,
                                scaleH_Reference,dest_Width);
                buf=dest+y*linesize;
                //printf("copy to destination %d\n",y);
                (*OutputConvert)(buf,tmp_pixmap+1,dest_Width-2,y&1);
                if (pixelMask)
                        CreatePixelMask(pixelMask+y*linesize/16,
                                        tmp_pixmap+1,dest_Width-2);
                if (dest_dirtyLines)
                        dest_dirtyLines[y]=true;

        };
        memset(dirty_lines,false,sizeof(dirty_lines));
        OSDDEB("CopyToBitmap RGB down end\n");
};

//------------------------ lowlevel scaling functions ------------------------
//#define SCALEDEBV(out...) printf(out)
#define SCALEDEBV(out...)

void cSoftOsd::ScaleDownVert_MMX(uint32_t * dest, int linesize,
                int32_t new_pixel_height, int start_pos,
                color ** pixmap, int Pixel) {
#define SHIFT_BITS "6"
#define SHIFT_BITS_NUM 6
        //const int ScaleFactor=100;
        const int ScaleFactor=1<<SHIFT_BITS_NUM;
#ifndef USE_MMX
        unsigned int a_sum=0;
        unsigned int b_sum=0;
        unsigned int g_sum=0;
        unsigned int r_sum=0;
        unsigned int c;
#endif

//        uint32_t new_pixel_height=(OSD_HEIGHT*ScaleFactor)/dest_Height;
//        uint32_t new_pixel_height_rec=(dest_Height*ScaleFactor)/OSD_HEIGTH;
        int new_pixel_height_rec=ScaleFactor*ScaleFactor/new_pixel_height;

        int32_t pos;

        int row;
#ifdef USE_MMX
        __asm__(
                        " pxor %%mm0,%%mm0 \n" //mm0: dest pixel
                        " movd %0,%%mm6  \n"
                        SPLAT_U16( "%%mm6 " )
                        " pxor %%mm7,%%mm7 \n" //mm7: 00 00 00 ...
                        : : "r" (new_pixel_height_rec)  );
#endif
        SCALEDEBV("OSD_HEIGHT: %d start_pos: %d new_pixel_height: %d\n",
                        OSD_HEIGHT,start_pos,new_pixel_height);
        while (Pixel>0) {
                row=0;
                pos=start_pos;
                SCALEDEBV("\nstartpixel a_sum: %d pos: %d pixmap->a: %d,%d,%d,%d\n",
                          a_sum,pos,
                          pixmap[row][Pixel].a,pixmap[row][Pixel].r,
                          pixmap[row][Pixel].g,pixmap[row][Pixel].b);
                int32_t a_pos=abs(pos);
                //int32_t a_pos=-(pos);
#ifndef USE_MMX
                a_sum=b_sum=g_sum=r_sum=0;
                c = pixmap[row][Pixel];
                a_sum= GET_A(c)*a_pos;
                b_sum= GET_B(c)*a_pos;
                g_sum= GET_G(c)*a_pos;
                r_sum= GET_R(c)*a_pos;
#else
                __asm__(
                      " pxor %%mm0,%%mm0 \n"
                      " movd (%0),%%mm1 \n"
                      " movd %1,%%mm2 \n"
                      " punpcklbw %%mm7, %%mm1 \n"
                      SPLAT_U16( "%%mm2" )
                      " pmullw %%mm2,%%mm1 \n"
                      " paddw %%mm1,%%mm0 \n"
                      : : "r" (&pixmap[row][Pixel]),"r" (a_pos)  );
#endif
                row++;
                pos += new_pixel_height;

                while (pos>ScaleFactor) {
                        SCALEDEBV("while loop a_sum: %d pos: %d pixmap->a: %d,%d,%d,%d\n",
                                        a_sum,pos,pixmap[row][Pixel].a,
                                        pixmap[row][Pixel].r,pixmap[row][Pixel].g,pixmap[row][Pixel].b);

#ifndef USE_MMX
                        c=pixmap[row][Pixel];
                        a_sum+=GET_A(c)*ScaleFactor;
                        b_sum+=GET_B(c)*ScaleFactor;
                        g_sum+=GET_G(c)*ScaleFactor;
                        r_sum+=GET_R(c)*ScaleFactor;
#else
                        __asm__(
                               " movd (%0),%%mm1 \n"
                               " punpcklbw %%mm7, %%mm1 \n"
                               " psllw $"SHIFT_BITS",%%mm1 \n"
                               " paddw %%mm1,%%mm0 \n"
                               : : "r" (&pixmap[row][Pixel])  );
#endif

                        pos -=ScaleFactor;
                        row++;
                };
                SCALEDEBV("end while Pixel: %d a_sum: %d pixmap->a: %d,%d,%d,%d pos: %d\n",Pixel,
                                        a_sum,pixmap[row][Pixel].a,
                                        pixmap[row][Pixel].r,pixmap[row][Pixel].g,pixmap[row][Pixel].b,pos);
#ifndef USE_MMX
                c=pixmap[row][Pixel];
                a_sum+=GET_A(c)*pos;
                b_sum+=GET_B(c)*pos;
                g_sum+=GET_G(c)*pos;
                r_sum+=GET_R(c)*pos;
#else
                __asm__(
                      " movd (%0),%%mm1 \n"
                      " movd %1,%%mm2 \n"
                      " punpcklbw %%mm7, %%mm1 \n"
                      SPLAT_U16( "%%mm2" )
                      " pmullw %%mm2,%%mm1 \n"
                      " paddw %%mm1,%%mm0 \n"
                      : : "r" (&pixmap[row][Pixel]),"r" (pos)  );
#endif

                SCALEDEBV("a_sum: %d new_pixel_height_rec: %d a pixel: %d",
                                a_sum,new_pixel_height_rec,
                                a_sum/ScaleFactor*
                                new_pixel_height_rec/ScaleFactor);
#ifndef USE_MMX
                a_sum=a_sum/ScaleFactor*new_pixel_height_rec/ScaleFactor;
                b_sum=b_sum/ScaleFactor*new_pixel_height_rec/ScaleFactor;
                g_sum=g_sum/ScaleFactor*new_pixel_height_rec/ScaleFactor;
                r_sum=r_sum/ScaleFactor*new_pixel_height_rec/ScaleFactor;

                c = SET_B(b_sum) | SET_G(g_sum) | SET_R(r_sum) | SET_A(a_sum);
                dest[Pixel]=c;
#else
                __asm__(
                      " psrlw $"SHIFT_BITS",%%mm0 \n"
                      " pmullw %%mm6,%%mm0 \n"
                      " psrlw $"SHIFT_BITS",%%mm0 \n"
                      " packuswb %%mm0,%%mm0 \n"
                      " movd %%mm0,(%0) \n"
                      : : "r"(&dest[Pixel]) );
#endif
                SCALEDEBV(", %d, %d, %d\n",r_sum,g_sum,b_sum);
                //dest-=4;
                Pixel--;

        };
        EMMS;
};

void cSoftOsd::NoScaleHoriz_MMX(uint32_t * dest, int dest_Width,
                color * pixmap, int Pixel) {
        memcpy(dest,pixmap,Pixel*sizeof(color));
};

//-----------------------------------------------------------------
#define SCALEDEBH(out...)

void cSoftOsd::ScaleDownHoriz_MMX(uint32_t * dest, int dest_Width,
                color * pixmap, int Pixel) {
#define SHIFT_BITS "6"
#define SHIFT_BITS_NUM 6
        const int ScaleFactor=1<<SHIFT_BITS_NUM;
#ifndef USE_MMX
        unsigned int a_sum=0;
        unsigned int b_sum=0;
        unsigned int g_sum=0;
        unsigned int r_sum=0;
        unsigned int c;
#endif
        uint32_t new_pixel_width=(OSD_WIDTH*ScaleFactor)/dest_Width;
        uint32_t new_pixel_width_rec=(dest_Width*ScaleFactor)/OSD_WIDTH;

        int32_t pos=new_pixel_width;

#ifdef USE_MMX
        __asm__ __volatile__ (
                 " pxor %%mm0,%%mm0 \n" //mm0: dest pixel
                 " movd %0,%%mm6  \n"
                 SPLAT_U16( "%%mm6" )
                 " pxor %%mm7,%%mm7 \n" //mm7: 00 00 00 ...
                 : : "r" (new_pixel_width_rec)  );
#endif
        SCALEDEBH("OSD_WIDTH: %d dest_width: %d new_pixel_width: %d\n",
                        OSD_WIDTH,dest_Width,new_pixel_width);
        color *end_pixmap=pixmap+Pixel;
        while (pixmap<end_pixmap) {
                while (pos>ScaleFactor) {
                        SCALEDEBH("while loop a_sum: %d pixmap->a: %d,%d,%d,%d\n",
                                        a_sum,pixmap->a,
                                        pixmap->r,pixmap->g,pixmap->b);

#ifndef USE_MMX
                        c=*pixmap;
                        a_sum+=GET_A(c)*ScaleFactor;
                        b_sum+=GET_B(c)*ScaleFactor;
                        g_sum+=GET_G(c)*ScaleFactor;
                        r_sum+=GET_R(c)*ScaleFactor;
#else
                        __asm__ __volatile__(
                               " movd (%0),%%mm1 \n"
                               " punpcklbw %%mm7, %%mm1 \n"
                               " psllw $"SHIFT_BITS",%%mm1 \n"
                               " paddw %%mm1,%%mm0 \n"
                               : : "r" (pixmap)  );
#endif

                        pos -=ScaleFactor;
                        pixmap++;
                };
                SCALEDEBH("end while a_sum: %d pixmap->a: %d,%d,%d,%d pos: %d\n",
                                        a_sum,pixmap->a,
                                        pixmap->r,pixmap->g,pixmap->b,pos);
#ifndef USE_MMX
                c=*pixmap;
                a_sum+=GET_A(c)*pos;
                b_sum+=GET_B(c)*pos;
                g_sum+=GET_G(c)*pos;
                r_sum+=GET_R(c)*pos;
#else
                __asm__ __volatile__(
                      " movd (%0),%%mm1 \n"
                      " movd %1,%%mm2 \n"
                      " punpcklbw %%mm7, %%mm1 \n"
                      SPLAT_U16( "%%mm2" )
                      " pmullw %%mm2,%%mm1 \n"
                      " paddw %%mm1,%%mm0 \n"
                      : : "r" (pixmap),"r" (pos)  );
#endif

                SCALEDEBH("a_sum: %d new_pixel_width_rec: %d a pixel: %d",
                                a_sum,new_pixel_width_rec,
                                a_sum/ScaleFactor*
                                new_pixel_width_rec/ScaleFactor);
#ifndef USE_MMX
                a_sum=a_sum/ScaleFactor*new_pixel_width_rec/ScaleFactor;
                b_sum=b_sum/ScaleFactor*new_pixel_width_rec/ScaleFactor;
                g_sum=g_sum/ScaleFactor*new_pixel_width_rec/ScaleFactor;
                r_sum=r_sum/ScaleFactor*new_pixel_width_rec/ScaleFactor;

                c = SET_B(b_sum) | SET_G(g_sum) | SET_R(r_sum) | SET_A(a_sum);
                *dest=c;
                /*
                dest[0]=b_sum;
                dest[1]=g_sum;
                dest[2]=r_sum;
                dest[3]=a_sum;*/
                a_sum=b_sum=g_sum=r_sum=0;
#else
                __asm__ __volatile__ (
                      " psrlw $"SHIFT_BITS",%%mm0 \n"
                      " pmullw %%mm6,%%mm0 \n"
                      " psrlw $"SHIFT_BITS",%%mm0 \n"
                      " packuswb %%mm0,%%mm0 \n"
                      " movd %%mm0,(%0) \n"
                      " pxor %%mm0,%%mm0 \n"
                      : : "r"(dest) : "memory" );
#endif
                SCALEDEBH(", %d, %d, %d\n",r_sum,g_sum,b_sum);
                dest++;

                pos-=ScaleFactor;
                SCALEDEBH("\nnext pixel a_sum: %d pixmap->a:%d,%d,%d,%d pos: %d\n",
                                        a_sum,pixmap->a,
                                        pixmap->r,pixmap->g,pixmap->b
                                        ,pos);
                //uint32_t apos=-(pos);
                uint32_t apos=abs(pos);
#ifndef USE_MMX
                c=*pixmap;
                a_sum=GET_A(c)*apos;
                b_sum=GET_B(c)*apos;
                g_sum=GET_G(c)*apos;
                r_sum=GET_R(c)*apos;
#else
                __asm__ __volatile__ (
                      " movd (%0),%%mm1 \n"
                      " movd %1,%%mm2 \n"
                      " punpcklbw %%mm7, %%mm1 \n"
                      SPLAT_U16( "%%mm2" )
                      " pmullw %%mm2,%%mm1 \n"
                      " paddw %%mm1,%%mm0 \n"
                      : : "r" (pixmap),"r" (apos)  );
#endif
                pixmap++;
                pos += new_pixel_width;
        };
        EMMS;
};

//-----------------------------------------------------------------------
#define SCALEUPDEBH(out...)
//#define SCALEUPDEBH(out...) printf(out)


void cSoftOsd::ScaleUpHoriz_MMX(uint32_t * dest, int dest_Width,
                color * pixmap, int Pixel) {
#define SHIFT_BITS "6"
#define SHIFT_BITS_NUM 6
        const int ScaleFactor=1<<SHIFT_BITS_NUM;
        //const int ScaleFactor=100;
#ifndef USE_MMX
        unsigned int c=*pixmap;
        unsigned int a1=GET_A(c);
        unsigned int b1=GET_B(c);
        unsigned int g1=GET_G(c);
        unsigned int r1=GET_R(c);
        pixmap++;
        c=*pixmap;
        unsigned int a2=GET_A(c);
        unsigned int b2=GET_B(c);
        unsigned int g2=GET_G(c);
        unsigned int r2=GET_R(c);
#else
        __asm__(
                " pxor %%mm7,%%mm7 \n" //mm7: 00 00 00 ...
                " movd (%0), %%mm1 \n"
                " movd 4(%0), %%mm2 \n"
                " punpcklbw %%mm7, %%mm1 \n"  // mm1 pixel1
                " punpcklbw %%mm7, %%mm2 \n"
                " movq %%mm2, %%mm4 \n"  // mm4 save pixel2
                " psubsw %%mm1, %%mm2 \n"// mm2 pixel2-pixel1
                //" pxor %%mm3, %%mm3 \n"// mm2: pos copy
                : : "r" (pixmap) );
        SCALEUPDEBH("new pixel: 0x%04x\n",*pixmap);
        pixmap++;
#endif
        uint32_t new_pixel_width=(OSD_WIDTH*ScaleFactor)/dest_Width;
        color *end_pixmap=pixmap+new_pixel_width*dest_Width/ScaleFactor;
        int32_t pos=0;
        SCALEUPDEBH("Scale up OSD_WIDTH: %d dest_width: %d new_pixel_width: %d\n",
                        OSD_WIDTH,dest_Width,new_pixel_width);
        while (pixmap<end_pixmap) {
                while (pos<ScaleFactor) {
                        SCALEUPDEBH("while loop pos: %d pixmap: 0x%04x\n",
                                        pos,*pixmap);

#ifndef USE_MMX
                        // funny that's the same formula we use for
                        // alpha blending ;-)
                        c  = SET_B(b1+(pos*(b2-b1)/ScaleFactor));
                        c |= SET_G(g1+(pos*(g2-g1)/ScaleFactor));
                        c |= SET_R(r1+(pos*(r2-r1)/ScaleFactor));
                        c |= SET_A(a1+(pos*(a2-a1)/ScaleFactor));
                        *dest=c;
#else
                        __asm__(
                                " movd %0,%%mm3 \n" //mm3 load pos
                                " movq %%mm2,%%mm0 \n" // mm0 pixel2-pixel1
                                SPLAT_U16( "%%mm3" )
                                " pmullw %%mm3, %%mm0 \n" //mm0 *pos
                                " psraw $"SHIFT_BITS",%%mm0 \n"
                                " paddsw %%mm1, %%mm0 \n" // mm0 + pixel1
                                " packuswb %%mm0,%%mm0 \n"
                                " movd %%mm0,(%1) \n"
                                : : "r" (pos),"r" (dest) : "memory" );
#endif

                        pos +=new_pixel_width;
                        SCALEUPDEBH("dest: 0x%04x\n",*dest);
                        dest++;
                };
                pixmap++;
                pos -= ScaleFactor;
#ifndef USE_MMX
                a1=a2;
                b1=b2;
                g1=g2;
                r1=r2;
                c=*pixmap;
                a2=GET_A(c);
                b2=GET_B(c);
                g2=GET_G(c);
                r2=GET_R(c);
#else
                __asm__(
                        " movq %%mm4, %%mm1 \n"
                        " movd (%0), %%mm2 \n"
                        " punpcklbw %%mm7, %%mm2 \n"
                        " movq %%mm2, %%mm4 \n"
                        " psubsw %%mm1, %%mm2 \n"// mm2 pixel2-pixel1
                        : : "r" (pixmap) : "memory" );
                SCALEUPDEBH("new pixel: 0x%04x\n",*pixmap);
#endif
        };
        EMMS;
};



//-------------------------------------------------------------------------
#define SCALEUPDEBV(out...)

void cSoftOsd::ScaleUpVert_MMX(uint32_t *dest, int linesize,
                int32_t new_pixel_height, int start_pos,
                color **pixmap, int Pixel) {
#define SHIFT_BITS "6"
#define SHIFT_BITS_NUM 6
        const int ScaleFactor=1<<SHIFT_BITS_NUM;
        //const int ScaleFactor=100;
#ifndef USE_MMX
        int a1=0;
        int b1=0;
        int g1=0;
        int r1=0;

        int a2=0;
        int b2=0;
        int g2=0;
        int r2=0;
        unsigned int c;
#else
        __asm__(
                " pxor %%mm7,%%mm7 \n" //mm7: 00 00 00 ...
                : : : "memory" );
#endif

        int32_t pos=start_pos;
        int currPixel=0;
        SCALEUPDEBV("Scale up OSD_WIDTH: %d  new_pixel_height: %d\n",
                        OSD_HEIGHT,new_pixel_height);
        while (currPixel<Pixel) {
#ifndef USE_MMX
                c=pixmap[0][currPixel];
                a1=GET_A(c);
                b1=GET_B(c);
                g1=GET_G(c);
                r1=GET_R(c);

                c=pixmap[1][currPixel];
                a2=GET_A(c)-a1;
                b2=GET_B(c)-b1;
                g2=GET_G(c)-g1;
                r2=GET_R(c)-r1;
#else
                __asm__(
                      " movd (%0),%%mm1 \n"
                      " movd (%1),%%mm2 \n"
                      " punpcklbw %%mm7, %%mm1 \n"  // mm1 pixel1
                      " punpcklbw %%mm7, %%mm2 \n"
                      //" psubw %%mm1, %%mm2 \n"// mm2 pixel2-pixel1
                      " psubsw %%mm1, %%mm2 \n"// mm2 pixel2-pixel1
                      : : "r" (&pixmap[0][currPixel]),
                          "r" (&pixmap[1][currPixel]));
#endif
                int ypos=0;
                pos=start_pos;
                while (pos<ScaleFactor) {
                        //SCALEUPDEBV("while loop pixel: %d pos: %d, row %d\n",
                        //              currPixel,pos,ypos);
#ifndef USE_MMX
                        c  = SET_B(b1+(pos*(b2)/ScaleFactor));
                        c |= SET_G(g1+(pos*(g2)/ScaleFactor));
                        c |= SET_R(r1+(pos*(r2)/ScaleFactor));
                        c |= SET_A(a1+(pos*(a2)/ScaleFactor));
                        dest[ypos*linesize+currPixel]=c;
#else
                        __asm__(
                                " movd %0,%%mm3 \n"
                                " movq %%mm2,%%mm0 \n"
                                SPLAT_U16( "%%mm3" )
                                " pmullw %%mm3, %%mm0 \n"
                                " psraw $"SHIFT_BITS",%%mm0 \n"
                                " paddsw %%mm1, %%mm0 \n"
                                " packuswb %%mm0,%%mm0 \n"
                                " movd %%mm0,(%1) \n"
                                : : "r" (pos),
                                "r" (&dest[ypos*linesize+currPixel])
                                : "memory" );
#endif

                        pos +=new_pixel_height;
                        ypos+=1;
                };
                currPixel++;
        };
        EMMS;
};


