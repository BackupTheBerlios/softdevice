/*
 * i18n.c: Internationalization
 *
 * See the README file for copyright information and how to reach the author.
 *
 * $Id: i18n.c,v 1.3 2005/03/19 22:01:21 lucke Exp $
 */

#include "i18n.h"

const tI18nPhrase Phrases[] = {
  { "Softdevice",   //  1
    "Softdevice",   //  2
    "",             //  3 TODO
    "",             //  4 TODO
    "",             //  5 TODO
    "",             //  6 TODO
    "",             //  7 TODO
    "",             //  8 TODO
    "",             //  9 TODO
    "",             // 10 TODO
    "",             // 11 TODO
    "",             // 12 TODO
    "",             // 13 TODO
    "",             // 14 TODO
    "",             // 15 TODO
    "",             // 16 TODO
#if VDRVERSNUM >= 10316
    "",             // 17 TODO
    "",             // 18 TODO
    "",             // 19 TODO
    "",             // 20 TODO
#endif
  },
  { "Xv startup aspect",   //  1
    "Xv Startgr��e",       //  2
    "",             //  3 TODO
    "",             //  4 TODO
    "",             //  5 TODO
    "",             //  6 TODO
    "",             //  7 TODO
    "",             //  8 TODO
    "",             //  9 TODO
    "",             // 10 TODO
    "",             // 11 TODO
    "",             // 12 TODO
    "",             // 13 TODO
    "",             // 14 TODO
    "",             // 15 TODO
    "",             // 16 TODO
#if VDRVERSNUM >= 10316
    "",             // 17 TODO
    "",             // 18 TODO
    "",             // 19 TODO
    "",             // 20 TODO
#endif
  },
  { "CropMode",         //  1
    "Bildausschnitt",   //  2
    "",             //  3 TODO
    "",             //  4 TODO
    "",             //  5 TODO
    "",             //  6 TODO
    "",             //  7 TODO
    "",             //  8 TODO
    "",             //  9 TODO
    "",             // 10 TODO
    "",             // 11 TODO
    "",             // 12 TODO
    "",             // 13 TODO
    "",             // 14 TODO
    "",             // 15 TODO
    "",             // 16 TODO
#if VDRVERSNUM >= 10316
    "",             // 17 TODO
    "",             // 18 TODO
    "",             // 19 TODO
    "",             // 20 TODO
#endif
  },
  { "Deinterlace Method",   //  1
    "Deinterlace Method",   //  2
    "",             //  3 TODO
    "",             //  4 TODO
    "",             //  5 TODO
    "",             //  6 TODO
    "",             //  7 TODO
    "",             //  8 TODO
    "",             //  9 TODO
    "",             // 10 TODO
    "",             // 11 TODO
    "",             // 12 TODO
    "",             // 13 TODO
    "",             // 14 TODO
    "",             // 15 TODO
    "",             // 16 TODO
#if VDRVERSNUM >= 10316
    "",             // 17 TODO
    "",             // 18 TODO
    "",             // 19 TODO
    "",             // 20 TODO
#endif
  },
  { "Pixel Format",   //  1
    "Pixel Format",   //  2
    "",             //  3 TODO
    "",             //  4 TODO
    "",             //  5 TODO
    "",             //  6 TODO
    "",             //  7 TODO
    "",             //  8 TODO
    "",             //  9 TODO
    "",             // 10 TODO
    "",             // 11 TODO
    "",             // 12 TODO
    "",             // 13 TODO
    "",             // 14 TODO
    "",             // 15 TODO
    "",             // 16 TODO
#if VDRVERSNUM >= 10316
    "",             // 17 TODO
    "",             // 18 TODO
    "",             // 19 TODO
    "",             // 20 TODO
#endif
  },
  { "Picture mirroring",    //  1
    "Spiegelbild",          //  2
    "",             //  3 TODO
    "",             //  4 TODO
    "",             //  5 TODO
    "",             //  6 TODO
    "",             //  7 TODO
    "",             //  8 TODO
    "",             //  9 TODO
    "",             // 10 TODO
    "",             // 11 TODO
    "",             // 12 TODO
    "",             // 13 TODO
    "",             // 14 TODO
    "",             // 15 TODO
    "",             // 16 TODO
#if VDRVERSNUM >= 10316
    "",             // 17 TODO
    "",             // 18 TODO
    "",             // 19 TODO
    "",             // 20 TODO
#endif
  },
  { "A/V Delay",        //  1
    "A/V Verz�gerung",  //  2
    "",             //  3 TODO
    "",             //  4 TODO
    "",             //  5 TODO
    "",             //  6 TODO
    "",             //  7 TODO
    "",             //  8 TODO
    "",             //  9 TODO
    "",             // 10 TODO
    "",             // 11 TODO
    "",             // 12 TODO
    "",             // 13 TODO
    "",             // 14 TODO
    "",             // 15 TODO
    "",             // 16 TODO
#if VDRVERSNUM >= 10316
    "",             // 17 TODO
    "",             // 18 TODO
    "",             // 19 TODO
    "",             // 20 TODO
#endif
  },
  { "Pixel Aspect",       //  1
    "Seitenverh�ltnis",   //  2
    "",             //  3 TODO
    "",             //  4 TODO
    "",             //  5 TODO
    "",             //  6 TODO
    "",             //  7 TODO
    "",             //  8 TODO
    "",             //  9 TODO
    "",             // 10 TODO
    "",             // 11 TODO
    "",             // 12 TODO
    "",             // 13 TODO
    "",             // 14 TODO
    "",             // 15 TODO
    "",             // 16 TODO
#if VDRVERSNUM >= 10316
    "",             // 17 TODO
    "",             // 18 TODO
    "",             // 19 TODO
    "",             // 20 TODO
#endif
  },
  { "Playback",     //  1
    "Wiedergabe",   //  2
    "",             //  3 TODO
    "",             //  4 TODO
    "",             //  5 TODO
    "",             //  6 TODO
    "",             //  7 TODO
    "",             //  8 TODO
    "",             //  9 TODO
    "",             // 10 TODO
    "",             // 11 TODO
    "",             // 12 TODO
    "",             // 13 TODO
    "",             // 14 TODO
    "",             // 15 TODO
    "",             // 16 TODO
#if VDRVERSNUM >= 10316
    "",             // 17 TODO
    "",             // 18 TODO
    "",             // 19 TODO
    "",             // 20 TODO
#endif
  },
  { "OSD alpha blending",   //  1
    "OSD Einblendung",      //  2
    "",             //  3 TODO
    "",             //  4 TODO
    "",             //  5 TODO
    "",             //  6 TODO
    "",             //  7 TODO
    "",             //  8 TODO
    "",             //  9 TODO
    "",             // 10 TODO
    "",             // 11 TODO
    "",             // 12 TODO
    "",             // 13 TODO
    "",             // 14 TODO
    "",             // 15 TODO
    "",             // 16 TODO
#if VDRVERSNUM >= 10316
    "",             // 17 TODO
    "",             // 18 TODO
    "",             // 19 TODO
    "",             // 20 TODO
#endif
  },
  { "on",       //  1
    "ein",      //  2
    "",             //  3 TODO
    "",             //  4 TODO
    "",             //  5 TODO
    "",             //  6 TODO
    "",             //  7 TODO
    "",             //  8 TODO
    "",             //  9 TODO
    "",             // 10 TODO
    "",             // 11 TODO
    "",             // 12 TODO
    "",             // 13 TODO
    "",             // 14 TODO
    "",             // 15 TODO
    "",             // 16 TODO
#if VDRVERSNUM >= 10316
    "",             // 17 TODO
    "",             // 18 TODO
    "",             // 19 TODO
    "",             // 20 TODO
#endif
  },
  { "off",          //  1
    "aus",          //  2
    "",             //  3 TODO
    "",             //  4 TODO
    "",             //  5 TODO
    "",             //  6 TODO
    "",             //  7 TODO
    "",             //  8 TODO
    "",             //  9 TODO
    "",             // 10 TODO
    "",             // 11 TODO
    "",             // 12 TODO
    "",             // 13 TODO
    "",             // 14 TODO
    "",             // 15 TODO
    "",             // 16 TODO
#if VDRVERSNUM >= 10316
    "",             // 17 TODO
    "",             // 18 TODO
    "",             // 19 TODO
    "",             // 20 TODO
#endif
  },
  { NULL }
  };
