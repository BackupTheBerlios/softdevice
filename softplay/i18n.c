/*
 * i18n.c: Internationalization
 *
 * See the README file for copyright information and how to reach the author.
 *
 * $Id: i18n.c,v 1.1 2005/08/04 15:20:40 wachm Exp $
 */

#include "i18n.h"

const tI18nPhrase Phrases[] = {
  { "SoftPlay",     //  1
    "SoftPlay",     //  2 German 
    "",             //  3 TODO
    "",             //  4 TODO
    "",             //  5 TODO
    "",             //  6 TODO
    "",             //  7 TODO
    "",             //  8 TODO
    "Mediasoitin",  //  9 finnish
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
  { "SoftPlay play media files with the softdevice", //  1
    "SoftPlay spielt Multimediadateien mit dem Softdevice", //  2 TODO
    "",             //  3 TODO
    "",             //  4 TODO
    "",             //  5 TODO
    "",             //  6 TODO
    "",             //  7 TODO
    "",             //  8 TODO
    "Mediasoitin ohjelmistopohjaiselle näyttölaitteelle", //  9
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
  { "Files",        //  1
    "Datei",        //  2 TODO
    "",             //  3 TODO
    "",             //  4 TODO
    "",             //  5 TODO
    "",             //  6 TODO
    "",             //  7 TODO
    "",             //  8 TODO
    "Tiedostot",    //  9
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
  { "Play",         //  1
    "",             //  2 TODO
    "",             //  3 TODO
    "",             //  4 TODO
    "",             //  5 TODO
    "",             //  6 TODO
    "",             //  7 TODO
    "",             //  8 TODO
    "Toista",       //  9
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
  { "Play Files",   //  1
    "Spiel Dateien",//  2 TODO
    "",             //  3 TODO
    "",             //  4 TODO
    "",             //  5 TODO
    "",             //  6 TODO
    "",             //  7 TODO
    "",             //  8 TODO
    "Toista tiedostot", //  9
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
  { "Toggle List",  //  1
    "Liste",        //  2 TODO
    "",             //  3 TODO
    "",             //  4 TODO
    "",             //  5 TODO
    "",             //  6 TODO
    "",             //  7 TODO
    "",             //  8 TODO
    "Vaihda lista", //  9
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
  { "Play List",    //  1
    "Spiel Liste",  //  2 TODO
    "",             //  3 TODO
    "",             //  4 TODO
    "",             //  5 TODO
    "",             //  6 TODO
    "",             //  7 TODO
    "",             //  8 TODO
    "Toista lista", //  9
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
  { "Files: %s",    //  1
    "Dateien: %s",  //  2 TODO
    "",             //  3 TODO
    "",             //  4 TODO
    "",             //  5 TODO
    "",             //  6 TODO
    "",             //  7 TODO
    "",             //  8 TODO
    "Tiedostot: %s",//  9
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
  { "current playlist", //  1
    "aktuelle Liste",   //  2 TODO
    "",             //  3 TODO
    "",             //  4 TODO
    "",             //  5 TODO
    "",             //  6 TODO
    "",             //  7 TODO
    "",             //  8 TODO
    "nykyinen soittolista", //  9
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
  { "Options",      //  1
    "Optionen",    //  2 TODO
    "",             //  3 TODO
    "",             //  4 TODO
    "",             //  5 TODO
    "",             //  6 TODO
    "",             //  7 TODO
    "",             //  8 TODO
    "Asetukset",    //  9
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
  { "Shuffle Mode", //  1
    "Zufallsmodus", //  2 TODO
    "",             //  3 TODO
    "",             //  4 TODO
    "",             //  5 TODO
    "",             //  6 TODO
    "",             //  7 TODO
    "",             //  8 TODO
    "Satunnaissoitto", //  9
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
  { "Auto Repeat",  //  1
    "automatisches Wiederh.", //  2 TODO
    "",             //  3 TODO
    "",             //  4 TODO
    "",             //  5 TODO
    "",             //  6 TODO
    "",             //  7 TODO
    "",             //  8 TODO
    "Uudelleentoisto", //  9
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
  { "(Add Files)",  //  1
    "(Hinzufügen)", //  2 TODO
    "",             //  3 TODO
    "",             //  4 TODO
    "",             //  5 TODO
    "",             //  6 TODO
    "",             //  7 TODO
    "",             //  8 TODO
    "(Lisää)",      //  9
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
  { "(Add)",        //  1
    "(Hinzufügen)", //  2 TODO
    "",             //  3 TODO
    "",             //  4 TODO
    "",             //  5 TODO
    "",             //  6 TODO
    "",             //  7 TODO
    "",             //  8 TODO
    "(Lisää)",      //  9
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
  { "Delete",       //  1
    "Löschen",      //  2 TODO
    "",             //  3 TODO
    "",             //  4 TODO
    "",             //  5 TODO
    "",             //  6 TODO
    "",             //  7 TODO
    "",             //  8 TODO
    "Poista",       //  9
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
  { "Stop",         //  1
    "Stopp",        //  2 TODO
    "",             //  3 TODO
    "",             //  4 TODO
    "",             //  5 TODO
    "",             //  6 TODO
    "",             //  7 TODO
    "",             //  8 TODO
    "Pysäytä",      //  9
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
  { "Playlist 1",   //  1
    "Liste 1",      //  2 TODO
    "",             //  3 TODO
    "",             //  4 TODO
    "",             //  5 TODO
    "",             //  6 TODO
    "",             //  7 TODO
    "",             //  8 TODO
    "Soittolista 1",//  9
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
