/*
 * softdevice plugin for VDR
 *
 * Copyright (C) 2005 Martin Wache
 *
 * This code is distributed under the terms and conditions of the
 * GNU GENERAL PUBLIC LICENSE. See the file COPYING for details.
 *
 * $Id: SoftOsd.c,v 1.5 2006/02/03 22:34:54 wachm Exp $
 */
#include <assert.h>
#include "SoftOsd.h"
#include "utils.h"
#if VDRVERSNUM >= 10307 // only for the new osd interface...

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


/* ---------------------------------------------------------------------------
 */

cSoftOsd::cSoftOsd(cVideoOut *VideoOut, int X, int Y) 
	: cOsd(X, Y),active(false) {
        OSDDEB("cSoftOsd constructor\n");
        OutputConvert=&cSoftOsd::ARGB_to_ARGB32;
        pixelMask=NULL;
	bitmap_Format=PF_None; // forces a clear after first SetMode
        OSD_Bitmap=new uint32_t[OSD_STRIDE*(OSD_HEIGHT+2)];
        
        videoOut = VideoOut;
	videoOut->OpenOSD();
	xOfs=X;yOfs=Y;
        int Depth; bool HasAlpha; bool AlphaInversed; bool IsYUV; 
        uint8_t *PixelMask;
        videoOut->GetOSDMode(Depth,HasAlpha,AlphaInversed,
                        IsYUV,PixelMask);
        SetMode(Depth,HasAlpha,AlphaInversed,
                        IsYUV,PixelMask);
};

/*--------------------------------------------------------------------------*/
void cSoftOsd::Clear() {
        OSDDEB("Clear\n");
        uint32_t blank=COLOR_KEY;
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
        active=false;
        Cancel(3);
        if (videoOut) {
                videoOut->CloseOSD();
                videoOut=0;
        }
        delete OSD_Bitmap;
}

/* -------------------------------------------------------------------------*/
void cSoftOsd::Action() {
        OSDDEB("OSD thread started\n");
        active=true;
        while(active && videoOut) {
                int newOsdWidth;
                int newOsdHeight;

                videoOut->GetOSDDimension(newOsdWidth,newOsdHeight);
                if ( newOsdWidth==-1 || newOsdHeight==-1 )
                {
                        newOsdWidth=OSD_FULL_WIDTH;
                        newOsdHeight=OSD_FULL_HEIGHT;
                }
                
                int Depth; bool HasAlpha; bool AlphaInversed; bool IsYUV; 
                uint8_t *PixelMask;
                videoOut->GetOSDMode(Depth,HasAlpha,AlphaInversed,
                                IsYUV,PixelMask);
                bool modeChanged=SetMode(Depth,HasAlpha,AlphaInversed,
                                IsYUV,PixelMask);

                if ( ScreenOsdWidth!=newOsdWidth  || 
                                ScreenOsdHeight!=newOsdHeight  || 
                                modeChanged ) {
                        OSDDEB("Resolution or mode changed!\n");
                        if (modeChanged)
                                videoOut->ClearOSD();
                        OsdCommit();
                }
                usleep(17000);
        }
        OSDDEB("OSD thread ended\n");
}

/*--------------------------------------------------------------------------*/
void cSoftOsd::OsdCommit() {
        int newX;
        int newY;
        bool RefreshAll=false;
        videoOut->GetOSDDimension(newX,newY);
        if ( newX==-1 || newY==-1 ) {
                newX=OSD_FULL_WIDTH;
                newY=OSD_FULL_HEIGHT;
        }
        if (newX!=ScreenOsdWidth || newY!=ScreenOsdHeight) {
                ScreenOsdWidth=newX;
                ScreenOsdHeight=newY;
                RefreshAll=true;
        };

        int Depth; bool HasAlpha; bool AlphaInversed; bool IsYUV; 
        uint8_t *PixelMask;
        videoOut->GetOSDMode(Depth,HasAlpha,AlphaInversed,IsYUV,PixelMask);
        bool modeChanged=SetMode(Depth,HasAlpha,AlphaInversed,IsYUV,PixelMask);
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
                videoOut->CommitUnlockSoftOsdSurface();
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
	
	if (OSD_changed)
		OsdCommit();

	// give priority to the other threads
	pthread_yield();
        if (!active)
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
		ARGB_to_AYUV((uint8_t*)palette,(color *)orig_palette,
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
                        if ( IS_TRANSPARENT( (uint32_t)palette[i] >> 24  ) ) {
                        //if (((uint32_t)palette[i] & 0xFF000000) == 0x000000 ) {
                                palette[i] = COLOR_KEY; // color key;
                        };
		};
	} else if ( bitmap_Format == PF_inverseAlpha_ARGB32 ) {
		for (int i=0; i<maxColors; i++) {
                        ((color*) palette)[i].a=0xFF-((color*) palette)[i].a;
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

void cSoftOsd::ARGB_to_AYUV(uint8_t * dest, color * pixmap, int Pixel) {
        int Y;
        int U;
        int V;
        while (Pixel>0) {
                //printf("ARGB: %02x,%02x,%02x,%02x ",pixmap->a,pixmap->r,
                //                pixmap->g,pixmap->b);
                // I got this formular from Wikipedia...
                Y = (( 66 * (int)pixmap->r + 129 * (int)pixmap->g 
                               + 25 * (int)pixmap->b + 128 )  >> 8)+16;
                U = (( -38 * (int)pixmap->r - 74 * (int)pixmap->g 
                               +112 * (int)pixmap->b + 128 )  >> 8)+128;
                V = (( 112 * (int)pixmap->r - 94 * (int)pixmap->g 
                               - 18 * (int)pixmap->b + 128 )  >> 8)+128;
                *dest++=(uint8_t)V;
                *dest++=(uint8_t)U;
                *dest++=(uint8_t)Y;
                *dest++=pixmap->a;
                //printf("->AYUV: %0x,%02x,%02x,%02x\n",pixmap->a,Y,U,V);
                pixmap++;
                Pixel--;
        };
};

/*---------------------------------------------------------------------*/
void cSoftOsd::ARGB_to_ARGB32(uint8_t * dest, color * pixmap, int Pixel) {
	memcpy(dest,pixmap,Pixel*4);
};

/*----------------------------------------------------------------------*/

void cSoftOsd::ARGB_to_RGB32(uint8_t * dest, color * pixmap, int Pixel) {
	uint8_t *end_dest=dest+4*Pixel;
#ifdef USE_MMX 
	end_dest-=16;
        __asm__( 
			"movq (%0),%%mm5 \n"
			"movq (%1),%%mm6 \n"
			"movq (%2),%%mm7 \n"
			: :
			"r" (&transparent_thr),"r" (&opacity_thr),
			"r" (&pseudo_transparent)
	       );

        while (end_dest>dest) {
              // pseudo alpha blending
               __asm__( 
                 // second  and forth pixel
                 "movd 4(%0), %%mm4\n"  // load even pixels 
                 "punpckldq 12(%0), %%mm4\n"
                 
                 "movq %%mm4,%%mm3\n"
                 "psrlw $1,%%mm3 \n"
                 "pcmpgtb %%mm5,%%mm3 \n"
                 //"pcmpgtb (%1),%%mm3 \n"

                 "movq %%mm4,%%mm2\n"
                 "psrlw $1,%%mm2 \n"
                 "pcmpgtb %%mm6,%%mm2 \n"
                 //"pcmpgtb (%2),%%mm2 \n"

                 "pandn %%mm3,%%mm2 \n"
                 "psraw $8,%%mm2 \n"
                 "pshufw $0b11110101,%%mm2,%%mm3 \n"
                 "pshufw $0b11110101,%%mm2,%%mm2 \n"

                 "pand %%mm7,%%mm3 \n"
                 //"pand (%3),%%mm3 \n"
                 "pandn %%mm4,%%mm2 \n"
                 "por %%mm3,%%mm2\n"

		 "movd 0(%0), %%mm4\n" // load odd pixels 
		 "punpckldq 8(%0), %%mm3\n"

		 "punpckldq %%mm2,%%mm4 \n"
		 "punpckhdq %%mm2,%%mm3 \n"
		 "movq %%mm4, (%1) \n"
		 "movq %%mm3, 8(%1) \n"
		 
                : :
                 "r" (pixmap),"r" (dest));
	       dest+=16;
	       pixmap+=4;
	};
	EMMS;
	end_dest+=16;
#endif
        while (end_dest>dest) {
                if ( IS_BACKGROUND(pixmap->a) && (((intptr_t)dest) & 0x4) ) {
                        dest[0] = 0; dest[1] = 0; dest[2] = 0; dest[3] = 0x00;
                        // color key!
                } else {
                        dest[0]=pixmap->b;
                        dest[1]=pixmap->g;
                        dest[2]=pixmap->r;
                        dest[3]=pixmap->a;
                }
                dest+=4;
                pixmap++;
        };
};

/*---------------------------------------------------------------------------*/

void cSoftOsd::AYUV_to_AYUV420P(uint8_t *PY1, uint8_t *PY2,
		    uint8_t *PU, uint8_t *PV,
		    uint8_t *PAlphaY1,uint8_t *PAlphaY2,
		    uint8_t *PAlphaUV,
		    color * pixmap1, color * pixmap2, int Pixel) {
	while (Pixel>1) {
		*(PAlphaY1++)=pixmap1[0].a; *(PAlphaY1++)=pixmap1[1].a;
		*(PAlphaY2++)=pixmap2[0].a; *(PAlphaY2++)=pixmap2[1].a;
		*(PY1++)=pixmap1[0].r; *(PY1++)=pixmap1[1].r;
		*(PY2++)=pixmap2[0].r; *(PY2++)=pixmap2[1].r;
                
		*(PU++)=(uint8_t)((uint16_t)
                                ((uint16_t)pixmap1[0].b+(uint16_t)pixmap1[1].b+
				  (uint16_t)pixmap2[0].b+(uint16_t)pixmap2[1].b)
			>>2);
		*(PV++)=(uint8_t)((uint16_t)(
                                  (uint16_t)pixmap1[0].g+(uint16_t)pixmap1[1].g+
				  (uint16_t)pixmap2[0].g+(uint16_t)pixmap2[1].g)
			>>2);
		*(PAlphaUV++)=(uint8_t)((uint16_t)
			((uint16_t)pixmap1[0].a+(uint16_t)pixmap1[1].a+
			(uint16_t)pixmap2[0].a+(uint16_t)pixmap2[1].a)
			>>2);
                        
                pixmap1+=2;
                pixmap2+=2;
                Pixel-=2;
        };
};				
               
/*---------------------------------------------------------------------------*/

void cSoftOsd::ARGB_to_RGB24(uint8_t * dest, color * pixmap, int Pixel) {
        while (Pixel) {
                if ( IS_BACKGROUND(pixmap->a) && (Pixel & 0x1) ) {
                        dest[0] = 0x0; dest[1] = 0x0; dest[1] = 0x0; // color key!
                } else {
                        dest[0]=pixmap->b;
                        dest[1]=pixmap->g;
                        dest[2]=pixmap->r;
                }
                dest+=3;
                Pixel--;
                pixmap++;
        };
};

/*---------------------------------------------------------------------------*/

void cSoftOsd::ARGB_to_RGB16(uint8_t * dest, color * pixmap, int Pixel) {
	uint8_t *end_dest=dest+2*Pixel;
#ifdef USE_MMX2
        static uint64_t rb_mask =   {0x00f800f800f800f8LL};
        static uint64_t g_mask =    {0xf800f800f800f800LL};
        

        end_dest-=8;
        __asm__(
                 "movq (%0),%%mm6\n"
                 "movq (%1),%%mm5\n"
                        : :
                        "r" (&rb_mask),"r" (&g_mask) );
        while (end_dest>dest) {
              // pseudo alpha blending
               __asm__( 
                 // second  and forth pixel
                 "movd 4(%0), %%mm4\n"  // load even pixels 
                 "punpckldq 12(%0), %%mm4\n"
                 
                 "movq %%mm4,%%mm3\n"
                 "psrlw $1,%%mm3 \n"
                 //"pcmpgtb %%mm5,%%mm3 \n"
                 "pcmpgtb (%2),%%mm3 \n"

                 "movq %%mm4,%%mm2\n"
                 "psrlw $1,%%mm2 \n"
                 //"pcmpgtb %%mm6,%%mm2 \n"
                 "pcmpgtb (%3),%%mm2 \n"

                 "pandn %%mm3,%%mm2 \n"
                 "psraw $8,%%mm2 \n"
                 "pshufw $0b11110101,%%mm2,%%mm3 \n"
                 "pshufw $0b11110101,%%mm2,%%mm2 \n"

                 //"pand %%mm7,%%mm3 \n"
                 "pand (%4),%%mm3 \n"
                 "pandn %%mm4,%%mm2 \n"
                 "por %%mm3,%%mm2\n"

		 "movd 0(%0), %%mm0\n" // load odd pixels 
		 "punpckldq 8(%0), %%mm1\n"

		 "punpckldq %%mm2,%%mm0 \n"
		 "punpckhdq %%mm2,%%mm1 \n"
		 "movq %%mm4, (%1) \n"
		 "movq %%mm3, 8(%1) \n"		 
                : :
                "r" (pixmap),"r" (dest),
                "r" (&transparent_thr),"r" (&opacity_thr),
		"r" (&pseudo_transparent)
                                );
              // ARGB to RGB16 
              __asm__(
                 // "movq  (%0),%%mm0\n"  // mm0: 1A 1R 1G 1B 2A 2R 2G 2B 
                 // "movq  8(%0),%%mm1\n"  // mm1: 3A 3R 3G 3B 4A 4R 4G 4B
                  "pshufw $0b11011101,%%mm0,%%mm2\n"
                  "pshufw $0b11011101,%%mm1,%%mm3\n"
                  "punpckldq %%mm3, %%mm2\n" //mm2: alpha and r channels 
                  "pshufw $0b00101000,%%mm0,%%mm3\n"
                  "pshufw $0b00101000,%%mm1,%%mm1\n"
                  "punpckldq %%mm1, %%mm3\n" //mm3: g and b channels
                    
                  "pand %%mm6, %%mm2\n" //mm2 : r-komponente 
                  "psllw $8,%%mm2\n"    //mm2 : r-komponente 
                  "movq %%mm5, %%mm1\n" 
                  "pand %%mm3, %%mm1\n" //mm1 : g-komponente
                  "pand %%mm6, %%mm3\n" //mm3: b-komponente
                  "psrlw $5,%%mm1\n" //mm3 : g-komponente ok 
                  "psrlw $3,%%mm3\n" //mm1 : b-komponente 

                  "por %%mm3,%%mm2\n"
                  "por %%mm1,%%mm2\n"
                    
                  " movq %%mm2,(%1) \n" 
                  : : "r" (pixmap), "r" (dest)
                  : "memory");
              pixmap+=4;
              dest+=8;
        };
	EMMS;
        end_dest+=8;
#endif
        while (end_dest>dest) {
                if ( IS_BACKGROUND(pixmap->a) && (((intptr_t)dest) & 0x2) ) {
                        dest[0] = 0x0; dest[1] = 0x0; // color key!
                } else {
                        dest[0] = ((pixmap->b >> 3)& 0x1F) | 
                                ((pixmap->g & 0x1C) << 3);
                        dest[1] = (pixmap->r & 0xF8) | (pixmap->g >> 5);
                }
                dest+=2;
                pixmap++;
        };
};
/*---------------------------------------------------------------------------*/

void cSoftOsd::ARGB_to_RGB16_PixelMask(uint8_t * dest, color * pixmap, 
                int Pixel) {
	uint8_t *end_dest=dest+2*Pixel;
	int PixelCount=0;
        while (end_dest>dest) {
                if ( IS_BACKGROUND(pixmap->a) && (((intptr_t)pixmap) & 0x4)
				|| IS_TRANSPARENT(pixmap->a) ) {
                        // transparent, don't draw anything !
                } else {
                        // FIXME 15/16bit mode? Probably broken!
			dest[0] = ((pixmap->b >> 3)& 0x1F) | 
                                ((pixmap->g & 0x1C) << 3);
                        dest[1] = ((pixmap->r ) & 0xF8) 
			 	| ((pixmap->g >> 5) );
                }
                dest+=2;
                pixmap++;
		PixelCount++;
        };
};

void cSoftOsd::CreatePixelMask(uint8_t * dest, color * pixmap, int Pixel) {
	int CurrPixel=0;
        while (CurrPixel<Pixel) {
                if ( (IS_BACKGROUND(pixmap->a) && !(((intptr_t)pixmap) & 0x4)  )
                                || IS_OPAQUE(pixmap->a) ) 
                        dest[CurrPixel/8]|=(1<<CurrPixel%8);
                 else dest[CurrPixel/8]&=~(1<<CurrPixel%8);
                pixmap++;
                CurrPixel++;
        };
};

//---------------------------YUV modes ----------------------
#define SCALEH_IDX(x) ((scaleH_strtIdx+(x))%lines_count)
void cSoftOsd::CopyToBitmap(uint8_t *PY,uint8_t *PU, uint8_t *PV,
		    uint8_t *PAlphaY,uint8_t *PAlphaUV,
                    int Ystride, int UVstride,
                    int dest_Width, int dest_Height, bool RefreshAll) {

        OSDDEB("CopyToBitmap YUV down\n");
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
        int lines_count=(new_pixel_height/ScaleFactor*2+2);
        color scaleH_pixmap[lines_count*OSD_STRIDE];
        color *scaleH_Reference[lines_count];
        int scaleH_lines[lines_count];
        int scaleH_strtIdx=0;
        memset(scaleH_lines,-1,sizeof(scaleH_lines));
        
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
                                ScaleDownHoriz_MMX((uint8_t *)scaleH_Reference[i],dest_Width,
                                                &pixmap[(start_row+i)*OSD_STRIDE],OSD_WIDTH-1); 
                                scaleH_lines[SCALEH_IDX(i)]=start_row+i;
                        }
                };
                /*
                   printf("strt_Idx %d scaleH_lines: ",scaleH_strtIdx);
                   for (int i=0; i<lines_count; i++)
                   printf("%d ",scaleH_lines[i]);
                   printf("\n");
                   */
                ScaleDownVert_MMX((uint8_t*) tmp_pixmap,0,
				new_pixel_height,start_pos,
                                scaleH_Reference,OSD_WIDTH-1);
                int new_start_row=new_pixel_height*(y+1)/ScaleFactor;
                int start_row_idx=new_start_row-start_row;
                start_pos=new_pixel_height*(y+1)%ScaleFactor-ScaleFactor;

                if (scaleH_lines[SCALEH_IDX(start_row_idx)]!= 
                                new_start_row)
                        printf("start_row mismatch new_start_row %d : %d\n",
                                        new_start_row,
                                        scaleH_lines[SCALEH_IDX(start_row_idx)]);
                
                ScaleDownVert_MMX((uint8_t*) &tmp_pixmap[OSD_STRIDE],0,
				new_pixel_height,start_pos,
                                &scaleH_Reference[start_row_idx],
                                OSD_WIDTH-1);

                // convert and copy to video out OSD layer
                pY=PY+y*Ystride;
                pU=PU+y*UVstride/2;
                pV=PV+y*UVstride/2;
                pAlphaY=PAlphaY+y*Ystride;
                pAlphaUV=PAlphaUV+y*UVstride/2;
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
	if (dest_Height>=OSD_HEIGHT) 
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
	const int dest_stride=dest_Width+16;
	void (cSoftOsd::*ScaleHoriz)(uint8_t * dest, int dest_Width, color * pixmap,int Pixel);
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
				((uint8_t *)scaleH_Reference[0],dest_Width,
				 &pixmap[(start_row+0)*OSD_STRIDE],OSD_WIDTH);
			scaleH_lines[scaleH_strtIdx] = start_row;
		};
		
		(this->*ScaleHoriz)
			((uint8_t *)scaleH_Reference[1],dest_Width,
			 &pixmap[(start_row+1)*OSD_STRIDE],OSD_WIDTH);
	
		scaleH_lines[!scaleH_strtIdx] = start_row+1;
		ScaleUpVert_MMX((uint8_t*) tmp_pixmap,dest_Width*4,
				new_pixel_height,start_pos,
                                scaleH_Reference,dest_Width);
		
		//printf("Copy to destination y: %d\n",y);
		buf=dest+y*linesize;
		(*OutputConvert)(buf,tmp_pixmap,dest_Width);
                if (pixelMask)
                        CreatePixelMask(pixelMask+y*linesize/16,
                                        tmp_pixmap,dest_Width);
                if (dest_dirtyLines)
                        dest_dirtyLines[y]=true;
                
		start_pos+=new_pixel_height;
		uint8_t *src=((uint8_t*)tmp_pixmap)+dest_Width*4;
		while (start_pos<ScaleFactor) {
			//printf("Copy to destination y: %d\n",y);
			y++;
			buf=dest+y*linesize;
			(*OutputConvert)(buf,(color*)src,dest_Width);
                        if (pixelMask)
                                CreatePixelMask(pixelMask+y*linesize/16,
                                                tmp_pixmap,dest_Width);
                        if (dest_dirtyLines)
                                dest_dirtyLines[y]=true;
			start_pos+=new_pixel_height;
			src+=dest_Width*4;
		};
        };
        memset(dirty_lines,false,sizeof(dirty_lines));
        OSDDEB("CopyToBitmap RGB up end\n");
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
	const int dest_stride=dest_Width+16;
        color tmp_pixmap[2*dest_stride];

       void (cSoftOsd::*ScaleHoriz)(uint8_t * dest, int dest_Width, color * pixmap,int Pixel);
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
                                (this->*ScaleHoriz)((uint8_t *)scaleH_Reference[i],dest_Width,
                                                &pixmap[(start_row+i)*OSD_STRIDE],OSD_WIDTH-1); 
                                scaleH_lines[SCALEH_IDX(i)]=start_row+i;
                        }
                };
                ScaleDownVert_MMX((uint8_t*) tmp_pixmap,linesize,
				new_pixel_height,start_pos,
                                scaleH_Reference,dest_Width);
                buf=dest+y*linesize;
                //printf("copy to destination %d\n",y);
                (*OutputConvert)(buf,tmp_pixmap,dest_Width);
                if (pixelMask)
                        CreatePixelMask(pixelMask+y*linesize/16,
                                        tmp_pixmap,dest_Width);
                if (dest_dirtyLines)
                        dest_dirtyLines[y]=true;

        };
        memset(dirty_lines,false,sizeof(dirty_lines));
        OSDDEB("CopyToBitmap RGB down end\n");
};



//------------------------ lowlevel scaling functions ------------------------
//#define SCALEDEBV(out...) printf(out) 
#define SCALEDEBV(out...)

void cSoftOsd::ScaleDownVert_MMX(uint8_t * dest, int linesize,
		int32_t new_pixel_height, int start_pos,
                color ** pixmap, int Pixel) {
#define SHIFT_BITS "6"
#define SHIFT_BITS_NUM 6
        //const int ScaleFactor=100;
        const int ScaleFactor=1<<SHIFT_BITS_NUM;
#ifndef USE_MMX2
        uint16_t a_sum=0;
        uint16_t b_sum=0;
        uint16_t g_sum=0;
        uint16_t r_sum=0;
#endif

//        uint32_t new_pixel_height=(OSD_HEIGHT*ScaleFactor)/dest_Height;
//        uint32_t new_pixel_height_rec=(dest_Height*ScaleFactor)/OSD_HEIGTH;
        int new_pixel_height_rec=ScaleFactor*ScaleFactor/new_pixel_height;
        
        int32_t pos;

        int row;
#ifdef USE_MMX2
        __asm__(
                        " pxor %%mm0,%%mm0 \n" //mm0: dest pixel
                        " movd %0,%%mm6  \n"
                        " pshufw $0,%%mm6,%%mm6 \n"// mm6: new_pixel_height_rec
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
#ifndef USE_MMX2                       
                a_sum=b_sum=g_sum=r_sum=0;
                a_sum=((int) pixmap[row][Pixel].a)*a_pos;
                b_sum=((int) pixmap[row][Pixel].b)*a_pos;
                g_sum=((int) pixmap[row][Pixel].g)*a_pos;
                r_sum=((int) pixmap[row][Pixel].r)*a_pos;
#else
                __asm__(
                      " pxor %%mm0,%%mm0 \n"
                      " movd (%0),%%mm1 \n"
                      " movd %1,%%mm2 \n"
                      " punpcklbw %%mm7, %%mm1 \n"
                      " pshufw $0b0,%%mm2,%%mm2 \n"
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

#ifndef USE_MMX2                       
                        a_sum+=((int) pixmap[row][Pixel].a)*ScaleFactor;
                        b_sum+=((int) pixmap[row][Pixel].b)*ScaleFactor;
                        g_sum+=((int) pixmap[row][Pixel].g)*ScaleFactor;
                        r_sum+=((int) pixmap[row][Pixel].r)*ScaleFactor;
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
#ifndef USE_MMX2                       
                a_sum+=((int) pixmap[row][Pixel].a)*pos;
                b_sum+=((int) pixmap[row][Pixel].b)*pos;
                g_sum+=((int) pixmap[row][Pixel].g)*pos;
                r_sum+=((int) pixmap[row][Pixel].r)*pos;
#else
                __asm__(
                      " movd (%0),%%mm1 \n"
                      " movd %1,%%mm2 \n"
                      " punpcklbw %%mm7, %%mm1 \n"
                      " pshufw $0b0,%%mm2,%%mm2 \n"
                      " pmullw %%mm2,%%mm1 \n"
                      " paddw %%mm1,%%mm0 \n"
                      : : "r" (&pixmap[row][Pixel]),"r" (pos)  );
#endif
                
                SCALEDEBV("a_sum: %d new_pixel_height_rec: %d a pixel: %d",
                                a_sum,new_pixel_height_rec,
                                a_sum/ScaleFactor*
                                new_pixel_height_rec/ScaleFactor);
#ifndef USE_MMX2                       
                a_sum=a_sum/ScaleFactor*new_pixel_height_rec/ScaleFactor;
                b_sum=b_sum/ScaleFactor*new_pixel_height_rec/ScaleFactor;
                g_sum=g_sum/ScaleFactor*new_pixel_height_rec/ScaleFactor;
                r_sum=r_sum/ScaleFactor*new_pixel_height_rec/ScaleFactor;
                
                dest[Pixel*4+0]=b_sum;
                dest[Pixel*4+1]=g_sum;
                dest[Pixel*4+2]=r_sum;
                dest[Pixel*4+3]=a_sum;
#else
                __asm__(
                      " psrlw $"SHIFT_BITS",%%mm0 \n"
                      " pmullw %%mm6,%%mm0 \n"
                      " psrlw $"SHIFT_BITS",%%mm0 \n"
                      " packuswb %%mm0,%%mm0 \n"
                      " movd %%mm0,(%0) \n"
                      : : "r"(&dest[Pixel*4]) );
#endif
                SCALEDEBV(", %d, %d, %d\n",r_sum,g_sum,b_sum);
                //dest-=4;
                Pixel--;

        };
        EMMS;
};


//-----------------------------------------------------------------
#define SCALEDEBH(out...) 

void cSoftOsd::ScaleDownHoriz_MMX(uint8_t * dest, int dest_Width, 
                color * pixmap, int Pixel) {
#define SHIFT_BITS "6"
#define SHIFT_BITS_NUM 6
        const int ScaleFactor=1<<SHIFT_BITS_NUM;
#ifndef USE_MMX2
        uint16_t a_sum=0;
        uint16_t b_sum=0;
        uint16_t g_sum=0;
        uint16_t r_sum=0;
#endif
        uint32_t new_pixel_width=(OSD_WIDTH*ScaleFactor)/dest_Width;
        uint32_t new_pixel_width_rec=(dest_Width*ScaleFactor)/OSD_WIDTH;
        
        int32_t pos=new_pixel_width;
        
#ifdef USE_MMX2
        __asm__ __volatile__ (
                 " pxor %%mm0,%%mm0 \n" //mm0: dest pixel
                 " movd (%0),%%mm6  \n"
                 " pshufw $0,%%mm6,%%mm6 \n"// mm6: new_pixel_width_rec
                 " pxor %%mm7,%%mm7 \n" //mm7: 00 00 00 ...
                 : : "r" (&new_pixel_width_rec)  );
#endif
        SCALEDEBH("OSD_WIDTH: %d dest_width: %d new_pixel_width: %d\n",
                        OSD_WIDTH,dest_Width,new_pixel_width);
        color *end_pixmap=pixmap+Pixel;
        while (pixmap<end_pixmap) {
                while (pos>ScaleFactor) {
                        SCALEDEBH("while loop a_sum: %d pixmap->a: %d,%d,%d,%d\n",
                                        a_sum,pixmap->a,
                                        pixmap->r,pixmap->g,pixmap->b);

#ifndef USE_MMX2                       
                        a_sum+=((int) pixmap->a)*ScaleFactor;
                        b_sum+=((int) pixmap->b)*ScaleFactor;
                        g_sum+=((int) pixmap->g)*ScaleFactor;
                        r_sum+=((int) pixmap->r)*ScaleFactor;
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
#ifndef USE_MMX2                       
                a_sum+=((int) pixmap->a)*pos;
                b_sum+=((int) pixmap->b)*pos;
                g_sum+=((int) pixmap->g)*pos;
                r_sum+=((int) pixmap->r)*pos;
#else
                __asm__ __volatile__(
                      " movd (%0),%%mm1 \n"
                      " movd %1,%%mm2 \n"
                      " punpcklbw %%mm7, %%mm1 \n"
                      " pshufw $0b0,%%mm2,%%mm2 \n"
                      " pmullw %%mm2,%%mm1 \n"
                      " paddw %%mm1,%%mm0 \n"
                      : : "r" (pixmap),"r" (pos)  );
#endif
                
                SCALEDEBH("a_sum: %d new_pixel_width_rec: %d a pixel: %d",
                                a_sum,new_pixel_width_rec,
                                a_sum/ScaleFactor*
                                new_pixel_width_rec/ScaleFactor);
#ifndef USE_MMX2                       
                a_sum=a_sum/ScaleFactor*new_pixel_width_rec/ScaleFactor;
                b_sum=b_sum/ScaleFactor*new_pixel_width_rec/ScaleFactor;
                g_sum=g_sum/ScaleFactor*new_pixel_width_rec/ScaleFactor;
                r_sum=r_sum/ScaleFactor*new_pixel_width_rec/ScaleFactor;
                
		dest[0]=b_sum;
                dest[1]=g_sum;
                dest[2]=r_sum;
                dest[3]=a_sum;
                a_sum=b_sum=g_sum=r_sum=0;
#else
                __asm__ __volatile__ (
                      " psrlw $"SHIFT_BITS",%%mm0 \n"
                      " pmullw %%mm6,%%mm0 \n"
                      " psrlw $"SHIFT_BITS",%%mm0 \n"
                      " packuswb %%mm0,%%mm0 \n"
                      " movd %%mm0,(%0) \n"
                      " pxor %%mm0,%%mm0 \n"
                      : : "r"(dest) );
#endif
                SCALEDEBH(", %d, %d, %d\n",r_sum,g_sum,b_sum);
                dest+=4;

                pos-=ScaleFactor;
                SCALEDEBH("\nnext pixel a_sum: %d pixmap->a:%d,%d,%d,%d pos: %d\n",
                                        a_sum,pixmap->a,
                                        pixmap->r,pixmap->g,pixmap->b
                                        ,pos);
                //uint32_t apos=-(pos);
                uint32_t apos=abs(pos);
#ifndef USE_MMX2                       
                a_sum=((int) pixmap->a)*apos;
                b_sum=((int) pixmap->b)*apos;
                g_sum=((int) pixmap->g)*apos;
                r_sum=((int) pixmap->r)*apos;
#else
                __asm__ __volatile__ (
                      " movd (%0),%%mm1 \n"
                      " movd %1,%%mm2 \n"
                      " punpcklbw %%mm7, %%mm1 \n"
                      " pshufw $0b0,%%mm2,%%mm2 \n"
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

void cSoftOsd::ScaleUpHoriz_MMX(uint8_t * dest, int dest_Width, 
                color * pixmap, int Pixel) {
#define SHIFT_BITS "6"
#define SHIFT_BITS_NUM 6
        const int ScaleFactor=1<<SHIFT_BITS_NUM;
        //const int ScaleFactor=100;
        color *end_pixmap=pixmap+Pixel;
#ifndef USE_MMX2
	uint16_t a1=pixmap->a;
	uint16_t b1=pixmap->b;
	uint16_t g1=pixmap->g;
	uint16_t r1=pixmap->r;
	pixmap++;
	uint16_t a2=pixmap->a;
	uint16_t b2=pixmap->b;
	uint16_t g2=pixmap->g;
	uint16_t r2=pixmap->r;
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
	pixmap++;
#endif
        uint32_t new_pixel_width=(OSD_WIDTH*ScaleFactor)/dest_Width;
        int32_t pos=0;
        SCALEDEBH("Scale up OSD_WIDTH: %d dest_width: %d new_pixel_width: %d\n",
                        OSD_WIDTH,dest_Width,new_pixel_width);
        while (pixmap<end_pixmap) {
                while (pos<ScaleFactor) {
                        SCALEDEBH("while loop a_sum: %d pixmap->a: %d,%d,%d,%d\n",
                                        a_sum,pixmap->a,
                                        pixmap->r,pixmap->g,pixmap->b);

#ifndef USE_MMX2                       
			// funny that's the same formula we use for
			// alpha blending ;-)
			dest[0]=(uint8_t) (b1+(pos*(b2-b1)/ScaleFactor));
			dest[1]=(uint8_t) (g1+(pos*(g2-g1)/ScaleFactor));
			dest[2]=(uint8_t) (r1+(pos*(r2-r1)/ScaleFactor));
			dest[3]=(uint8_t) (a1+(pos*(a2-a1)/ScaleFactor));
#else
			__asm__(
				" movd %0,%%mm3 \n" //mm3 load pos
				" movq %%mm2,%%mm0 \n" // mm0 pixel2-pixel1
				" pshufw $0b0,%%mm3,%%mm3 \n"
				" pmullw %%mm3, %%mm0 \n" //mm0 *pos
				" psraw $"SHIFT_BITS",%%mm0 \n"
				" paddsw %%mm1, %%mm0 \n" // mm0 + pixel1
				" packuswb %%mm0,%%mm0 \n"
				" movd %%mm0,(%1) \n"
				: : "r" (pos),"r" (dest) );
#endif
                       
                        pos +=new_pixel_width;
                        dest+=4;
                };
		pixmap++;
                pos -= ScaleFactor;
#ifndef USE_MMX2                       
                a1=a2;
		b1=b2;
		g1=g2;
		r1=r2;
		a2=pixmap->a;
		b2=pixmap->b;
		g2=pixmap->g;
		r2=pixmap->r;
#else
		__asm__(
			" movq %%mm4, %%mm1 \n"
			" movd (%0), %%mm2 \n"
			" punpcklbw %%mm7, %%mm2 \n"
			" movq %%mm2, %%mm4 \n"
			" psubsw %%mm1, %%mm2 \n"// mm2 pixel2-pixel1
			: : "r" (pixmap) );	
#endif
        };
	EMMS;
};
 


//-------------------------------------------------------------------------
#define SCALEUPDEBV(out...)  

void cSoftOsd::ScaleUpVert_MMX(uint8_t *dest, int linesize, 
		int32_t new_pixel_height, int start_pos,
                color **pixmap, int Pixel) {
#define SHIFT_BITS "6"
#define SHIFT_BITS_NUM 6
        const int ScaleFactor=1<<SHIFT_BITS_NUM;
        //const int ScaleFactor=100;
#ifndef USE_MMX2
        int16_t a1=0;
        int16_t b1=0;
        int16_t g1=0;
        int16_t r1=0;

	int16_t a2=0;
        int16_t b2=0;
        int16_t g2=0;
        int16_t r2=0;
#else
        __asm__(
                 " pxor %%mm7,%%mm7 \n" //mm7: 00 00 00 ...
                 : :   );	
#endif
        
        int32_t pos=start_pos;
	int currPixel=0;
        SCALEUPDEBV("Scale up OSD_WIDTH: %d  new_pixel_height: %d\n",
                        OSD_HEIGHT,new_pixel_height);
        while (currPixel<Pixel) {
#ifndef USE_MMX2
		a1=pixmap[0][currPixel].a;
		b1=pixmap[0][currPixel].b;
		g1=pixmap[0][currPixel].g;
		r1=pixmap[0][currPixel].r;

		a2=pixmap[1][currPixel].a-a1;
		b2=pixmap[1][currPixel].b-b1;
		g2=pixmap[1][currPixel].g-g1;
		r2=pixmap[1][currPixel].r-r1;
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
			//		currPixel,pos,ypos);
#ifndef USE_MMX2                       
			dest[ypos*linesize+currPixel*4+0]=
				(uint8_t) (b1+(pos*(b2)/ScaleFactor));
			dest[ypos*linesize+currPixel*4+1]=
				(uint8_t) (g1+(pos*(g2)/ScaleFactor));
			dest[ypos*linesize+currPixel*4+2]=
				(uint8_t) (r1+(pos*(r2)/ScaleFactor));
			dest[ypos*linesize+currPixel*4+3]=
				(uint8_t) (a1+(pos*(a2)/ScaleFactor));
#else
			__asm__(
				" movd %0,%%mm3 \n"
				" movq %%mm2,%%mm0 \n"
				" pshufw $0b0,%%mm3,%%mm3 \n"
				" pmullw %%mm3, %%mm0 \n"
				" psraw $"SHIFT_BITS",%%mm0 \n"
				" paddsw %%mm1, %%mm0 \n"
				" packuswb %%mm0,%%mm0 \n"
				" movd %%mm0,(%1) \n"
				: : "r" (pos),
				"r" (&dest[ypos*linesize+currPixel*4]) );
#endif
                       
                        pos +=new_pixel_height;
                        ypos+=1;
                };
		currPixel++;
        };
	EMMS;
};
      
#endif   // VDRVERSNUM >= 10307
                        
