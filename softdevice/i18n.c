/*
 * i18n.c: Internationalization
 *
 * See the README file for copyright information and how to reach the author.
 *
 * $Id: i18n.c,v 1.2 2005/03/04 20:04:20 lucke Exp $
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
    "",             // 17 TODO
    "",             // 18 TODO
    "",             // 19 TODO
    "",             // 20 TODO
  },
  { "Xv startup aspect",   //  1
    "Xv Startgröße",       //  2
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
    "",             // 17 TODO
    "",             // 18 TODO
    "",             // 19 TODO
    "",             // 20 TODO
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
    "",             // 17 TODO
    "",             // 18 TODO
    "",             // 19 TODO
    "",             // 20 TODO
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
    "",             // 17 TODO
    "",             // 18 TODO
    "",             // 19 TODO
    "",             // 20 TODO
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
    "",             // 17 TODO
    "",             // 18 TODO
    "",             // 19 TODO
    "",             // 20 TODO
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
    "",             // 17 TODO
    "",             // 18 TODO
    "",             // 19 TODO
    "",             // 20 TODO
  },
  { "A/V Delay",        //  1
    "A/V Verzögerung",  //  2
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
    "",             // 17 TODO
    "",             // 18 TODO
    "",             // 19 TODO
    "",             // 20 TODO
  },
  { "Pixel Aspect",       //  1
    "Seitenverhältnis",   //  2
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
    "",             // 17 TODO
    "",             // 18 TODO
    "",             // 19 TODO
    "",             // 20 TODO
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
    "",             // 17 TODO
    "",             // 18 TODO
    "",             // 19 TODO
    "",             // 20 TODO
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
    "",             // 17 TODO
    "",             // 18 TODO
    "",             // 19 TODO
    "",             // 20 TODO
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
    "",             // 17 TODO
    "",             // 18 TODO
    "",             // 19 TODO
    "",             // 20 TODO
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
    "",             // 17 TODO
    "",             // 18 TODO
    "",             // 19 TODO
    "",             // 20 TODO
  },
  { NULL }
  };
