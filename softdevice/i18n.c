/*
 * i18n.c: Internationalization
 *
 * See the README file for copyright information and how to reach the author.
 *
 * $Id: i18n.c,v 1.8 2005/09/10 16:14:36 lucke Exp $
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
    "N�ytt�laite",  //  9
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
  { "A software emulated MPEG2 device", //  1
    "",             //  2 TODO
    "",             //  3 TODO
    "",             //  4 TODO
    "",             //  5 TODO   
    "",             //  6 TODO
    "",             //  7 TODO
    "",             //  8 TODO
    "Ohjelmistopohjainen n�ytt�laite",  //  9
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
    "Xv-kuvasuhde k�ynnistett�ess�",   //  9
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
    "Kuvan rajaustapa", //  9
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
    "Lomituksen poisto",    //  9
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
    "Pikseliformaatti", //  9
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
    "Kuvan peilaus",//  9
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
    "A/V-viive",    //  9
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
  { "Screen Aspect",      //  1
    "Bildschirmformat",   //  2
    "",             //  3 TODO
    "",             //  4 TODO
    "",             //  5 TODO
    "",             //  6 TODO
    "",             //  7 TODO
    "",             //  8 TODO
    "N�yt�n kuvasuhde",   //  9
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
    "N�yt�n ulostulo", //  9
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
    "Kuvaruutun�yt�n l�pin�kyvyys", //  9
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
  { "on",           //  1
    "ein",          //  2
    "",             //  3 TODO
    "",             //  4 TODO
    "",             //  5 TODO
    "",             //  6 TODO
    "",             //  7 TODO
    "",             //  8 TODO
    "p��ll�",       //  9
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
    "pois",         //  9
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
  { "AC3 Mode",     //  1
    "AC3 Modus",    //  2
    "",             //  3 TODO
    "",             //  4 TODO
    "",             //  5 TODO
    "",             //  6 TODO
    "",             //  7 TODO
    "",             //  8 TODO
    "AC3-��net",    //  9
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
  { "Buffer Mode",  //  1
    "",             //  2 TODO
    "",             //  3 TODO
    "",             //  4 TODO
    "",             //  5 TODO
    "",             //  6 TODO
    "",             //  7 TODO
    "",             //  8 TODO
    "Puskurointitapa", //  9
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
  { "CropModeToggleKey", //  1
    "",             //  2 TODO
    "",             //  3 TODO
    "",             //  4 TODO
    "",             //  5 TODO
    "",             //  6 TODO
    "",             //  7 TODO
    "",             //  8 TODO
    "N�pp�in kuvan rajaukselle", //  9
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
  { "Crop lines from top", //  1
    "",             //  2 TODO
    "",             //  3 TODO
    "",             //  4 TODO
    "",             //  5 TODO
    "",             //  6 TODO
    "",             //  7 TODO
    "",             //  8 TODO
    "Rajaa kuvaa ylh��lt�", //  9
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
  { "Crop lines from bottom", //  1
    "",             //  2 TODO
    "",             //  3 TODO
    "",             //  4 TODO
    "",             //  5 TODO
    "",             //  6 TODO
    "",             //  7 TODO
    "",             //  8 TODO
    "Rajaa kuvaa alhaalta", //  9
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
  { "Crop columns from left", //  1
    "",             //  2 TODO
    "",             //  3 TODO
    "",             //  4 TODO
    "",             //  5 TODO
    "",             //  6 TODO
    "",             //  7 TODO
    "",             //  8 TODO
    "Rajaa kuvaa vasemmalta", //  9
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
  { "Crop columns from right", //  1
    "",             //  2 TODO
    "",             //  3 TODO
    "",             //  4 TODO
    "",             //  5 TODO
    "",             //  6 TODO
    "",             //  7 TODO
    "",             //  8 TODO
    "Rajaa kuvaa oikealta", //  9
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
  { "Postprocessing Method", //  1
    "",             //  2 TODO
    "",             //  3 TODO
    "",             //  4 TODO
    "",             //  5 TODO
    "",             //  6 TODO
    "",             //  7 TODO
    "",             //  8 TODO
    "Kuvan j�lkik�sittely", //  9
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
  { "Postprocessing Quality", //  1
    "",             //  2 TODO
    "",             //  3 TODO
    "",             //  4 TODO
    "",             //  5 TODO
    "",             //  6 TODO
    "",             //  7 TODO
    "",             //  8 TODO
    "Kuvan j�lkik�sittelyn laatu", //  9
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
  { "Use StretchBlit", //  1
    "",             //  2 TODO
    "",             //  3 TODO
    "",             //  4 TODO
    "",             //  5 TODO
    "",             //  6 TODO
    "",             //  7 TODO
    "",             //  8 TODO
    "K�yt� StretchBlit-metodia", //  9
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
  { "none",         //  1
    "keine",        //  2
    "",             //  3 TODO
    "",             //  4 TODO
    "",             //  5 TODO
    "",             //  6 TODO
    "",             //  7 TODO
    "",             //  8 TODO
    "ei",           //  9
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
  { "fast",         //  1
    "",             //  2 TODO
    "",             //  3 TODO
    "",             //  4 TODO
    "",             //  5 TODO
    "",             //  6 TODO
    "",             //  7 TODO
    "",             //  8 TODO
    "nopea",        //  9
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
  { "default",      //  1
    "",             //  2 TODO
    "",             //  3 TODO
    "",             //  4 TODO
    "",             //  5 TODO
    "",             //  6 TODO
    "",             //  7 TODO
    "",             //  8 TODO
    "oletus",       //  9
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
  { "playing",      //  1
    "",             //  2 TODO
    "",             //  3 TODO
    "",             //  4 TODO
    "",             //  5 TODO
    "",             //  6 TODO
    "",             //  7 TODO
    "",             //  8 TODO
    "toiminnassa",  //  9
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
  { "suspended",    //  1
    "",             //  2 TODO
    "",             //  3 TODO
    "",             //  4 TODO
    "",             //  5 TODO
    "",             //  6 TODO
    "",             //  7 TODO
    "",             //  8 TODO
    "pys�ytetty",   //  9
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
  { "pseudo",       //  1
    "",             //  2 TODO
    "",             //  3 TODO
    "",             //  4 TODO
    "",             //  5 TODO
    "",             //  6 TODO
    "",             //  7 TODO
    "",             //  8 TODO
    "n�enn�inen",   //  9
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
  { "software",     //  1
    "",             //  2 TODO
    "",             //  3 TODO
    "",             //  4 TODO
    "",             //  5 TODO
    "",             //  6 TODO
    "",             //  7 TODO
    "",             //  8 TODO
    "ohjelmallinen",//  9
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
  { "save",         //  1
    "",             //  2 TODO
    "",             //  3 TODO
    "",             //  4 TODO
    "",             //  5 TODO
    "",             //  6 TODO
    "",             //  7 TODO
    "",             //  8 TODO
    "tallennus",    //  9
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
  { "good seeking", //  1
    "",             //  2 TODO
    "",             //  3 TODO
    "",             //  4 TODO
    "",             //  5 TODO
    "",             //  6 TODO
    "",             //  7 TODO
    "",             //  8 TODO
    "nopea haku",   //  9
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
  { "HDTV",         //  1
    "",             //  2 TODO
    "",             //  3 TODO
    "",             //  4 TODO
    "",             //  5 TODO
    "",             //  6 TODO
    "",             //  7 TODO
    "",             //  8 TODO
    "HDTV",         //  9
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
  { "16:9 wide",    //  1
    "",             //  2 TODO
    "",             //  3 TODO
    "",             //  4 TODO
    "",             //  5 TODO
    "",             //  6 TODO
    "",             //  7 TODO
    "",             //  8 TODO
    "16:9 laaja",   //  9
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
  { "4:3 normal",   //  1
    "",             //  2 TODO
    "",             //  3 TODO
    "",             //  4 TODO
    "",             //  5 TODO
    "",             //  6 TODO
    "",             //  7 TODO
    "",             //  8 TODO
    "4:3 normaali", //  9
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
