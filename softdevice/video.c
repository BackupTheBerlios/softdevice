/*
 * video.c: A plugin for the Video Disk Recorder
 *
 * See the README file for copyright information and how to reach the author.
 *
 * $Id: video.c,v 1.14 2005/03/20 15:46:49 lucke Exp $
 */

#include <sys/mman.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <vdr/plugin.h>
#include "video.h"
#include "utils.h"
#include "setup-softdevice.h"


cVideoOut::cVideoOut(cSetupStore *setupStore)
{
#if VDRVERSNUM >= 10307
  OsdWidth=OSD_FULL_WIDTH;
  OsdHeight=OSD_FULL_HEIGHT;
#endif
  sxoff = syoff = lxoff = lyoff = 0;
  PixelMask=NULL;
  this->setupStore=setupStore;
 //start thread
 // active=true;
 // Start();
};

cVideoOut::~cVideoOut()
{
  active=false;
  Cancel(3);
  dsyslog("[VideoOut]: Good bye");
}

/*----------------------------------------------------------------------------*/
void cVideoOut::Action() 
{
#if VDRVERSNUM >= 10307
  while(active)
  {
    int newOsdWidth;
    int newOsdHeight;
    bool changeMode=false;
    int newOsdMode=0;
    
    OsdRefreshCounter++;
    
    changeMode=(current_osdMode != setupStore->osdMode);
    newOsdMode=setupStore->osdMode;
    // if software osd has not been shown for some time fall back
    // to pseudo osd..
    if ( OsdRefreshCounter > 80 && setupStore->osdMode == OSDMODE_SOFTWARE ) {
        changeMode= (current_osdMode != OSDMODE_PSEUDO);
        newOsdMode=OSDMODE_PSEUDO;
    }
    
    GetOSDDimension(newOsdWidth,newOsdHeight);
    if ( newOsdWidth==-1 || newOsdHeight==-1 )
    {
      newOsdWidth=OSD_FULL_WIDTH;
      newOsdHeight=OSD_FULL_HEIGHT;
    }
    else 
    {
      if (newOsdWidth > OSD_FULL_WIDTH)
        newOsdWidth=OSD_FULL_WIDTH;
      if (newOsdHeight > OSD_FULL_HEIGHT)
        newOsdHeight=OSD_FULL_HEIGHT;
    }
    if (OSDpresent && osd 
       && ( OsdWidth!=newOsdWidth  || OsdHeight!=newOsdHeight  || 
           changeMode )
        )
    {
      OSDdirty=true;
      //printf("OsdWidth %d newOsdWidth %d OsdHeight %d newOsdHeight %d \n",
      //   OsdWidth,newOsdWidth,OsdHeight,newOsdHeight);
      if (changeMode) {
        cOsd *osdSave=osd;
        CloseOSD();
        current_osdMode=newOsdMode;
        OpenOSD(OSDxOfs,OSDyOfs);
        osd=osdSave;
      }
      osd->Flush();
    }
    usleep(50000);
  }
#endif
}

/* ---------------------------------------------------------------------------
 */
void cVideoOut::CheckAspect(int new_afd, float new_asp)
{
    int           new_aspect,
                  screenWidth, screenHeight;
    double        d_asp, afd_asp;

  /* -------------------------------------------------------------------------
   * check if there are some aspect ratio constraints
   * flags & XV_FORMAT_.. check if our ouput aspect is
   *                        NORMAL 4:3 or WIDE 16:9
   *
   */
  new_aspect = (new_asp > 1.4) ? DV_FORMAT_WIDE : DV_FORMAT_NORMAL;
  new_afd &= 0x07;
  /* -------------------------------------------------------------------------
   * override afd value with crop value from setup
   */
  new_afd = (setupStore->cropMode) ? setupStore->cropMode : new_afd;

  /* -------------------------------------------------------------------------
   * check for changes of screen width/height change
   */

  if (screenPixelAspect != setupStore->screenPixelAspect)
  {
    screenPixelAspect = setupStore->screenPixelAspect;
    /* -----------------------------------------------------------------------
     * force recalculation for aspect ration handling
     */
    current_aspect = -1;
  }

  if (new_aspect == current_aspect && new_afd == current_afd)
  {
    aspect_changed = 0;
    return;
  }

  setupStore->getScreenDimension (screenWidth, screenHeight);
  aspect_changed = 1;

  d_asp = (double) dwidth / (double) dheight;
  switch (new_afd) {
  /* --------------------------------------------------------------------------
   * these are still TODOs. in general I focus on 2 cases where there
   * is a mismatch (4:3 frame encoded as 16:9, and 16:9 enc as 4:3
   */
    case 0: afd_asp = new_asp; break;
    case 1: afd_asp = 4.0 / 3.0; break;
    case 2: afd_asp = 16.0 / 9.0; break;
    case 3: afd_asp = 14.0 / 9.0; break;
    case 4: afd_asp = new_asp; break;
    case 5: afd_asp = 4.0 / 3.0; break;
    case 6: afd_asp = 16.0 / 9.0; break;
    case 7: afd_asp = 16.0 / 9.0; break;
    default: afd_asp = new_asp; break;
  }

  sheight = fheight;
  swidth = fwidth;
  if (afd_asp <= new_asp) {
    swidth = (int) (0.5 + ((double) fwidth * afd_asp / new_asp));
  } else {
    sheight = (int) (0.5 + ((double) fheight * new_asp / afd_asp));
  }
  sxoff = (fwidth - swidth) / 2;
  syoff = (fheight - sheight) / 2;

  /* --------------------------------------------------------------------------
   * handle screen aspect support now
   */
  afd_asp *= ((double) screenWidth / (double) screenHeight) * (3.0 / 4.0);

  if (d_asp > afd_asp) {
    /* ------------------------------------------------------------------------
     * display aspect is wider than frame aspect
     * so we have to pillar-box
     */
    lheight = dheight;
    lwidth = (int) (0.5 + ((double) dheight * afd_asp));
    lxoff = (dwidth - lwidth) / 2;
    lyoff = 0;
  } else {
    /* ------------------------------------------------------------------------
     * display aspect is taller or equal than frame aspect
     * so we have to letter-box
     */
    lwidth = dwidth;
    lheight = (int) (0.5 + ((double) dwidth / afd_asp));
    lyoff = (dheight - lheight) / 2;
    lxoff = 0;
  }

  dsyslog("[VideoOut]: %dx%d [%d,%d %dx%d] -> %dx%d [%d,%d %dx%d]",
          fwidth, fheight, sxoff, syoff, swidth, sheight,
          dwidth, dheight, lxoff, lyoff, lwidth, lheight);

  current_aspect = new_aspect;
  current_afd = new_afd;
}

/* ---------------------------------------------------------------------------
 */
void cVideoOut::CheckAspectDimensions(AVFrame *picture,
                                        AVCodecContext *context)
{
    static volatile float new_asp, aspect_F = -100.0;
    static int            aspect_I = -100;

  /* --------------------------------------------------------------------------
   * check and handle changes of dimensions first
   */
  if (fwidth != context->width || fheight != context->height)
  {
    dsyslog("[VideoOut]: resolution changed: W(%d -> %d); H(%d ->%d)\n",
             fwidth, context->width, fheight, context->height);
    fwidth = context->width; fheight = context->height;
    current_aspect = -1;
  }

#if LIBAVCODEC_BUILD > 4686
  /* --------------------------------------------------------------------------
   * removed aspect ratio calculation based on picture->pan_scan->width
   * as this value seems to be wrong on some dvds.
   */
  new_asp = (float) (context->width * context->sample_aspect_ratio.num) /
              (float) (context->height * context->sample_aspect_ratio.den);
#else
  new_asp = context->aspect_ratio;
#endif

  /* --------------------------------------------------------------------------
   * aspect_F and new_asp are now static volatile float. Due to above
   * code removal, gcc-3.3.1 from suse compiles comparison wrong.
   * it compares the 32bit float value with it's temprary new_asp value
   * from above calculation which has even a higher precision than double :-( ,
   * and would result not_equal every time.
   */
  if (aspect_I != context->dtg_active_format ||
      aspect_F != new_asp)
  {
    dsyslog("[VideoOut]: aspect changed (%d -> %d ; %f -> %f)",
             aspect_I,context->dtg_active_format,
             aspect_F,new_asp);
#if LIBAVCODEC_BUILD > 4686
    if (picture->pan_scan->width) {
      dsyslog("[VideoOut]: PAN/SCAN info present ([%d] %d - %d, %d %d)",
               picture->pan_scan->id,
               picture->pan_scan->width,
               picture->pan_scan->height,
               context->sample_aspect_ratio.num,
               context->sample_aspect_ratio.den);
      for (int i = 0; i < 3; i++) {
        dsyslog("[VideoOut]: PAN/SCAN  position  %d (%d %d)",
                 i,
                 picture->pan_scan->position[i] [0],
                 picture->pan_scan->position[i] [1]);
      }
    }
#endif

    aspect_I = context->dtg_active_format;
    aspect_F = new_asp;
  }

  CheckAspect (aspect_I, aspect_F);
}

#define OPACITY_THRESHOLD 0x8F
#define TRANSPARENT_THRESHOLD 0x0F
#define IS_BACKGROUND(a) (((a) < OPACITY_THRESHOLD) && (a > TRANSPARENT_THRESHOLD))

/* ---------------------------------------------------------------------------
 */
void cVideoOut::OSDStart()
{
  osdMutex.Lock();
  //fprintf (stderr, "+");

#if VDRVERSNUM >= 10307
  if (current_osdMode==OSDMODE_SOFTWARE) 
  {
    int Ysize=(OSD_FULL_WIDTH*OSD_FULL_HEIGHT);
    if (!OsdPy)
       OsdPy=(uint8_t*)malloc(Ysize+8);
    if (!OsdPAlphaY) 
    {
       OsdPAlphaY=(uint8_t*)malloc(Ysize+8);
       memset(OsdPAlphaY,0,Ysize);
    };
    if (!OsdPu)
       OsdPu=(uint8_t*)malloc(Ysize/4+8);
    if (!OsdPv)
       OsdPv=(uint8_t*)malloc(Ysize/4+8);
    if (!OsdPAlphaUV)
    {
       OsdPAlphaUV=(uint8_t*)malloc(Ysize/4+8);
       memset(OsdPAlphaUV,0,Ysize/4);
    }
  }

  int newX,newY;
  GetOSDDimension(newX,newY);
  if ( newX==-1 || newY==-1 )
  {
    newX=OSD_FULL_WIDTH;
    newY=OSD_FULL_HEIGHT;
  }
  else 
  {
    if (newX > OSD_FULL_WIDTH)
      newX=OSD_FULL_WIDTH;
    if (newY > OSD_FULL_HEIGHT)
      newY=OSD_FULL_HEIGHT;
  }
  if (newX!=OsdWidth || newY!=OsdHeight)
  {
    OsdWidth=newX;
    OsdHeight=newY;
    OSDdirty=true;
  };

  if (OSDdirty)
    ClearOSD();
#endif
} 

/* ---------------------------------------------------------------------------
 */
void cVideoOut::OSDCommit()
{
  //fprintf (stderr, "-");
  OSDdirty=false;
  OSDpresent=true;
  osdMutex.Unlock();
}

#if VDRVERSNUM >= 10307

void cVideoOut::OpenOSD(int X, int Y)
{
  OSDxOfs = X;
  OSDyOfs = Y;
  OSDdirty=true;
}

void cVideoOut::CloseOSD()
{
  osdMutex.Lock();
  if (OsdPAlphaY)
       memset(OsdPAlphaY,0,OSD_FULL_WIDTH*OSD_FULL_HEIGHT);
  if (OsdPAlphaUV)
       memset(OsdPAlphaUV,0,OSD_FULL_WIDTH*OSD_FULL_HEIGHT/4);

  osd=NULL;
  OSDpresent=false;
  osdMutex.Unlock();
}

/* ---------------------------------------------------------------------------
 */
void cVideoOut::ClearOSD()
{
  if (current_osdMode==OSDMODE_SOFTWARE) 
  {
    if (OsdPAlphaY)
       memset(OsdPAlphaY,0,OSD_FULL_WIDTH*OSD_FULL_HEIGHT);
    if (OsdPAlphaUV)
       memset(OsdPAlphaUV,0,OSD_FULL_WIDTH*OSD_FULL_HEIGHT/4);
  };
};

/* ---------------------------------------------------------------------------
 */

void ScaleBitmap(cBitmap *Bitmap,
              int &a, int &r, int &g, int &b,
              int x, int y, int newX, int newY) {
// scales a bitmap down... no upscaling...
  const tIndex  *adr;
  struct color {
    unsigned char b;
    unsigned char g;
    unsigned char r;
    unsigned char a;
    } pixel;
 
  if ( OSD_FULL_HEIGHT == newY &&
      OSD_FULL_WIDTH == newX) 
  {
     adr = Bitmap->Data(x,y);
     *((uint32_t *) &pixel) = (uint32_t) Bitmap->Color(*adr);
     a = pixel.a;
     b = pixel.b;
     r = pixel.r;
     g = pixel.g;
     return;
  };
  
  int minPY=OSD_FULL_HEIGHT*y/newY;
  int maxPY=OSD_FULL_HEIGHT*(y+1)/newY;
  int minPX=OSD_FULL_WIDTH*x/newX;
  int maxPX=OSD_FULL_WIDTH*(x+1)/newX;
  int sumA=0;
  int sumR=0;
  int sumG=0;
  int sumB=0;
  int weightYf=0;
  int weightYl=0;
  int weightXf=0;
  int weightXl=0;
  int weightX=0;
  int weight;
  int weights=0;
   

  weightYf=-OSD_FULL_HEIGHT*100*y/newY+(minPY+1)*100;
  weightYl=-(maxPY)*100+OSD_FULL_HEIGHT*100*(y+1)/newY;
  weightXf=-OSD_FULL_WIDTH*100*x/newX+(minPX+1)*100;
  weightXl=-(maxPX)*100+OSD_FULL_WIDTH*100*(x+1)/newX;

  for (int i=minPX; i<= maxPX; i++) {
    if (i==minPX)
      weightX=weightXf;
    else if (i==maxPX)
      weightX=weightXl;
    else weight=100;
    
    for (int j=minPY; j<= maxPY; j++) {
      if (j==minPY)
        weight=weightX*weightYf;
      else if (j==maxPY)
        weight=weightX*weightYl;
      else weight=weightX*100;

//      printf("minPX %d, maxPX %d, minPY %d maxPY %d, weightX %d,weightYf %d \n",
//       minPX,maxPX,minPY,maxPY,weightX,weightYf);
      adr = Bitmap->Data(i,j);
      *((uint32_t *) &pixel) = (uint32_t) Bitmap->Color(*adr);
      sumA+=weight* pixel.a;
      sumB+=weight* pixel.b;
      sumR+=weight* pixel.r;
      sumG+=weight* pixel.g;
      weights+=weight;
    };
  };

  a= sumA/weights;
  b= sumB/weights;
  r= sumR/weights;
  g= sumG/weights;
};
  
void cVideoOut::Draw(cBitmap *Bitmap,
                     unsigned char *osd_buf,
                     int linelen,
                     bool inverseAlpha)
{
    int           depth = (Bpp + 7) / 8;
    int           a, r, g, b;
    bool          prev_pix = false, do_dither;
    tIndex        *buf;
    int           x1,x2,y1,y2;
    uint8_t       *PixelMaskPtr;

    
//  printf( "Draw: OSDWidth %d %d Bitmap %d %d \n",
//   OsdWidth,OsdHeight,Bitmap->Width(),Bitmap->Height()); 
    // if bitmap didn't change, return
    if (!Bitmap->Dirty(x1,y1,x2,y2) && !OSDdirty )
      return;

// printf("dirty area (%d,%d) (%d,%d) \n",x1,y1,x2,y2);
  
  if (OSDdirty)
  {
    y1=x1=0;
    x2=Bitmap->Width()-1;
    y2=Bitmap->Height()-1;
  };

#define SCALEX(x) ((x) * OsdWidth/OSD_FULL_WIDTH)
#define SCALEY(y) ((y) * OsdHeight/OSD_FULL_HEIGHT)
  for (int y =SCALEY(y1); y <= SCALEY(y2); y++)
  {
    buf = (tIndex *) osd_buf +
            linelen * ( OSDyOfs + y + SCALEY(Bitmap->Y0())) +
            (OSDxOfs + SCALEX(Bitmap->X0() + x1) ) * depth;
    PixelMaskPtr=PixelMask +
           linelen/16 * ( OSDyOfs + y + SCALEY(Bitmap->Y0())) +
            (OSDxOfs + SCALEX(Bitmap->X0() + x1))/8;
    prev_pix = false;

    for (int x = SCALEX(x1); x <= SCALEX(x2); x++)
    {
      do_dither = ((x % 2 == 1 && y % 2 == 1) ||
                    x % 2 == 0 && y % 2 == 0 || prev_pix);
     /* 
      adr = Bitmap->Data(x, y);
      c = Bitmap->Color(*adr);
      a = (c >> 24) & 255; //Alpha
      r = (c >> 16) & 255; //Red
      g = (c >> 8) & 255;  //Green
      b = c & 255;         //Blue
    */ 
     ScaleBitmap(Bitmap,a,r,g,b,x,y,OsdWidth,OsdHeight);

      if (PixelMask) {
        if (a>TRANSPARENT_THRESHOLD)
          PixelMaskPtr[x/8]|=(1<<x%8);
      }

      /* ---------------------------------------------------------------------
       * pseudo spu drawing looks better if rgb values are set to colorkey
       * so that they are full transparent in case of zero alpha value
       */
      if (!a)
        r = g = b = 0;

      switch (depth) {
        case 4:
          if ((do_dither && IS_BACKGROUND(a) && OSDpseudo_alpha) ||
              (a == 255 && r == 0 && g == 0 && b == 0))
          {
            buf[0] = 1; buf[1] = 1; buf[2] = 1; buf[3] = 255;
          } else {
            if (inverseAlpha)
              a = 255 - a;

            buf[0] = b;
            buf[1] = g;
            buf[2] = r;
            buf[3] = a;
          }

          buf += 4;
          break;
        case 3:
          if ((do_dither && IS_BACKGROUND(a)) ||
              (a == 255 && r == 0 && g == 0 && b == 0))
          {
            buf[0] = 1; buf[1] = 1; buf[2] = 1;
          } else {
            buf[0] = b;
            buf[1] = g;
            buf[2] = r;
          }

          buf += 3;
          break;
        case 2:
          if ((do_dither && IS_BACKGROUND(a)) ||
              (a == 255 && r == 0 && g == 0 && b == 0))
          {
              buf[0] = 0x21; buf[1] = 0x08;
          } else {
            buf[0] = ((b >> 3)& 0x1F) | ((g & 0x1C) << 3);
            buf[1] = (r & 0xF8) | (g >> 5);
          }

          buf += 2;
          break;
        default:
            //dsyslog("[VideoOut] OSD: unsupported depth %d exiting",depth);
            //exit(1);
          break;
      }
      prev_pix = !IS_BACKGROUND(a);
    }
  }
  Bitmap->Clean();
}

void cVideoOut::ToYUV(cBitmap *Bitmap)
{
    int      a1, r1, g1, b1;
    int      a2, r2, g2, b2;
    uint8_t       Y1,U1,V1;
    uint8_t       Y2,U2,V2;
    int linelen=OSD_FULL_WIDTH;
    int offset;
  //  printf("ToYUV... OsdPy: 0%x Res:(%d,%d)\n",OsdPy,linelen,lines);

  
 // printf( "YUV:OSDWidth %d %d Bitmap %d %d \n",
 //   OsdWidth,OsdHeight,Bitmap->Width(),Bitmap->Height()); 
 
    int           x1,x2,y1,y2;
    // if bitmap didn't change, return
    if (!Bitmap->Dirty(x1,y1,x2,y2) && !OSDdirty)
      return;
    
   //printf( "----------------------------\nOSDWidth %d %d Bitmap %d %d \n",
   //     OsdWidth,OsdHeight,Bitmap->Width(),Bitmap->Height()); 
   //printf("dirty area (%d,%d) (%d,%d) \n",x1,y1,x2,y2);
    
    if ( OSDdirty )
    {
      //printf("++++++++++++++++++++++new OsdHeight+++++++ OSDdirty %d+++++\n",
      //  OSDdirty);
      x1=y1=0;
      x2=Bitmap->Width()-1;
      y2=Bitmap->Height()-1;
   }

#define SCALEX(x) ((x) * OsdWidth/OSD_FULL_WIDTH)
#define SCALEY(y) ((y) * OsdHeight/OSD_FULL_HEIGHT)

  y1=SCALEY(y1); 
  y2=SCALEY(y2);
  x1=SCALEX(x1);
  x2=SCALEX(x2);
  int  x0=sxoff+SCALEX(OSDxOfs+Bitmap->X0());
  int  y0=(syoff+SCALEY(OSDyOfs+Bitmap->Y0())) & ~1; // we need an even offset

  // we need a even starting point
  y1&=~1;
  y2+=y2!=Bitmap->Height()?1:0;//FIXME funzt nicht mehr
  // two rows at a time...
  for (int y = y1; y < y2; y+=2) 
  {
    offset = (y0+y)*linelen;
    for (int x = x1; x <= x2; x++)
    {
     ScaleBitmap(Bitmap,a1,r1,g1,b1,x,y,OsdWidth,OsdHeight);
     ScaleBitmap(Bitmap,a2,r2,g2,b2,x,y+1,OsdWidth,OsdHeight);
  
      Y1 = (( 66 * r1 + 129 * g1 + 25 * b1 + 128 )  >> 8)+16;
      Y2 = (( 66 * r2 + 129 * g2 + 25 * b2 + 128 )  >> 8)+16;
      OsdPy[offset+x+x0]=Y1;
      OsdPAlphaY[offset+x+x0]=a1;
      OsdPy[offset+linelen+x+x0]=Y2;
      OsdPAlphaY[offset+linelen+x+x0]=a2;
      
      // even columns have U and V
      if ( (x&1)==0 ) 
      {
        // I got this formular from Wikipedia...
        U1 = (( -38 * r1 - 74 * g1 +112 * b1 + 128 )  >> 8)+128;
        V1 = (( 112 * r1 - 94 * g1 - 18 * b1 + 128 )  >> 8)+128;

        U2 = (( -38 * r2 - 74 * g2 +112 * b2 + 128 )  >> 8)+128;
        V2 = (( 112 * r2 - 94 * g2 - 18 * b2 + 128 )  >> 8)+128;

        OsdPu[offset/4+(x+x0)/2]=(U1+U2)/2;
        OsdPv[offset/4+(x+x0)/2]=(V1+V2)/2;
        OsdPAlphaUV[offset/4+(x+x0)/2]=(a1+a2)/2;
      }
     }
    }
  Bitmap->Clean();

}
void cVideoOut::AlphaBlend(uint8_t *dest,uint8_t *P1,uint8_t *P2,
          uint8_t *alpha,uint16_t count) {
     // printf("%x %x %x \n",P1,P2,alpha);
 
#ifdef USE_MMX
        __asm__(" pxor %%mm3,%%mm3\n"
#ifdef USE_MMX2
                PREFETCH"(%0)\n"
                PREFETCH"(%1)\n"
                PREFETCH"(%2)\n"
#endif //USE_MMX2
                : : "r" (P1), "r" (P2), "r" (alpha) : "memory");

        // I guess this can be further improved...
        while (count>8 ) {
         __asm__(
#ifdef USE_MMX2
                PREFETCH" 32(%0)\n"
                PREFETCH" 32(%1)\n"
                PREFETCH" 32(%2)\n"
#endif //USE_MMX2
                "  movq  (%0),%%mm0\n"
                "  movq  (%1),%%mm1\n"
                "  movq  (%2),%%mm2\n"
                "  movq  %%mm0,%%mm4\n"
                "  movq  %%mm1,%%mm5\n"
                "  movq  %%mm2,%%mm6\n"
                "  punpcklbw %%mm3, %%mm0\n"
                "  punpcklbw %%mm3, %%mm1\n"
                "  punpcklbw %%mm3, %%mm2\n"
                "  punpckhbw %%mm3, %%mm4\n"
                "  punpckhbw %%mm3, %%mm5\n"
                "  punpckhbw %%mm3, %%mm6\n"
                "  psubw %%mm1, %%mm0 \n"
                "  psubw %%mm5, %%mm4 \n"
                "  psraw $1,%%mm0\n"
                "  psraw $1,%%mm4\n"
                "  pmullw %%mm2, %%mm0 \n"
                "  pmullw %%mm6, %%mm4 \n"
                "  psraw $7,%%mm0\n"
                "  psraw $7,%%mm4\n"
                "  paddw %%mm1, %%mm0 \n"
                "  paddw %%mm5, %%mm4 \n"
                "  packuswb %%mm4, %%mm0 \n"
                "  movq %%mm0,(%3)\n"
                : : "r" (P1), "r" (P2), "r" (alpha),"r"(dest) : "memory");
                count-=8;
                P1+=8;
                P2+=8;
                alpha+=8;
                dest+=8;
       }
#endif //USE_MMX

       //fallback version and the last missing bytes...
       for (int i=0; i < count; i++){
          dest[i]=(((uint16_t) P1[i] *(uint16_t) alpha[i]) +
             ((uint16_t) P2[i] *(256-(uint16_t) alpha[i])))  >>8 ;
       }
}


#else

bool cVideoOut::OpenWindow(cWindow *Window) {
    layer[Window->Handle()]= new cWindowLayer(Window->X0()+OSDxOfs,
                                              Window->Y0()+OSDyOfs,
                                              Window->Width(),
                                              Window->Height(),
                                              Bpp,
                                              Xres, Yres, OSDpseudo_alpha);
    return true;
}

void cVideoOut::OpenOSD(int X, int Y)
{
  // initialize Layers.
  OSDxOfs = (Xres - 720) / 2 + X;
  OSDyOfs = (Yres - 576) / 2 + Y;
  osdMutex.Lock();
  for (int i = 0; i < MAXNUMWINDOWS; i++)
  {
    layer[i]=0;
  }
  OSDpresent=true;
  osdMutex.Unlock();
}

void cVideoOut::CloseOSD()
{
  osdMutex.Lock();
  OSDpresent=false;
  for (int i = 0; i < MAXNUMWINDOWS; i++)
  {
    if (layer[i])
    {
      delete(layer[i]);
      layer[i]=0;
    }
  }
  osdMutex.Unlock();
}

void cVideoOut::CommitWindow(cWindow *Window) {
    layer[Window->Handle()]->Render(Window);
    Refresh();
}

void cVideoOut::ShowWindow(cWindow *Window) {
    layer[Window->Handle()]->visible=true;
    layer[Window->Handle()]->Render(Window);
    Refresh();
}
void cVideoOut::HideWindow(cWindow *Window, bool Hide) {
    layer[Window->Handle()]->visible= ! Hide ;
    Refresh();
}

void cVideoOut::MoveWindow(cWindow *Window, int x, int y) {
    layer[Window->Handle()]->Move(x,y);
    layer[Window->Handle()]->Render(Window);
    Refresh();
}

void cVideoOut::CloseWindow(cWindow *Window) {
    delete (layer[Window->Handle()]);
    Refresh();
}


// --- cWindowLayer --------------------------------------------------
cWindowLayer::cWindowLayer(int X, int Y, int W, int H, int Bpp,
                           int Xres, int Yres, bool alpha) {
    left=X;
    top=Y;
    width=W;
    height=H;
    bpp=Bpp;
    xres=Xres;
    yres=Yres;
    visible=false;
    OSDpseudo_alpha = alpha;
    imagedata=(unsigned char *)malloc(W*H*4); // RGBA Screen memory
    printf("[video] Creating WindowLayer at %d x %d, (%d x %d)\n",X,Y,W,H);
}

void cWindowLayer::Region (int *x, int *y, int *w, int *h) {
   *x = left;
   *y = top;
   *w = width;
   *h = height;
}


cWindowLayer::~cWindowLayer() {
    free(imagedata);
}


void cWindowLayer::Render(cWindow *Window) {
    unsigned char * buf;
    buf=imagedata;

  for (int yp = 0; yp < height; yp++) {
    for (int ix = 0; ix < width; ix++) {
      eDvbColor c = Window->GetColor(*Window->Data(ix,yp));
      buf[0]=c & 255; //Red
      buf[1]=(c >> 8) & 255; //Green
      buf[2]=(c >> 16) & 255; //Blue
      buf[3]=(c >> 24) & 255; //Alpha*/
      buf+=4;
    }
  }
}

void cWindowLayer::Move(int x, int y) {
    left=x;
    top=y;
}

void cWindowLayer::Draw(unsigned char * buf, int linelen, unsigned char * keymap) {
    unsigned char * im;
    im = imagedata;
    int depth = (bpp + 7) / 8;
    int dx = linelen - width * depth;
    bool          prev_pix = false, do_dither;

  buf += top * linelen + left * depth; // upper left corner
  for (int y = top; y < top+height; y++) {
    prev_pix = false;

    for (int x = left; x < left+width; x++) {
      if ( (im[3] != 0)
          && (x >= 0) && (x < xres)
          && (y >= 0) && (y < yres))  { // Alpha != 0 and in the screen
        do_dither = ((x % 2 == 1 && y % 2 == 1) ||
                      x % 2 == 0 && y % 2 == 0 || prev_pix);

        //if (keymap) keymap[(x+y*linelen / depth) / 8] |= (1 << (x % 8));
        switch (depth) {
          case 4:
            if ((do_dither && IS_BACKGROUND(im[3]) && OSDpseudo_alpha) ||
                (im[3] == 255 && im[0] == 0 && im[1] == 0 && im[2] == 0)) {
              *buf++ = 1; *buf++ = 1; *buf++ = 1; *buf++ = 255;
            } else {
              *(buf++)=im[2];
              *(buf++)=im[1];
              *(buf++)=im[0];
              *(buf++)=im[3];
            }
            //buf++;
            break;
          case 3:
            if ((do_dither && IS_BACKGROUND(im[3])) ||
                (im[3] == 255 && im[0] == 0 && im[1] == 0 && im[2] == 0)) {
              *buf++ = 1; *buf++ = 1; *buf++ = 1;
            } else {
              *(buf++)=im[2];
              *(buf++)=im[1];
              *(buf++)=im[0];
            }
            break;
          case 2: // 565 RGB
            if ((do_dither && IS_BACKGROUND(im[3])) ||
                (im[3] == 255 && im[0] == 0 && im[1] == 0 && im[2] == 0)) {
              *buf++ = 0x21; *buf++ = 0x08;
            } else {
              *(buf++)= ((im[2] >> 3)& 0x1F) | ((im[1] & 0x1C) << 3);
              *(buf++)= (im[0] & 0xF8) | (im[1] >> 5);
            }
            break;
          default:
            dsyslog("[video] Unsupported depth %d exiting",depth);
            exit(1);
        }
        prev_pix = !IS_BACKGROUND(im[3]);


      } else  {
        buf += depth; // skip this pixel
      }
      im +=4;
    }
    buf += dx;
  }
  return;
}

#endif
