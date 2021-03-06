/*
 * i18n.c: Internationalization
 *
 * See the README file for copyright information and how to reach the author.
 *
 * $Id: i18n.c,v 1.25 2008/04/14 02:28:09 lucke Exp $
 */

#include "i18n.h"

const tI18nPhrase Phrases[] = {
  { "Softdevice",   //  1
    "Softdevice",   //  2
    "",             //  3 TODO
    "Softdevice",   //  4
    "",             //  5 TODO
    "",             //  6 TODO
    "Softdevice",   //  7
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
    "Softdevice",   // 17
    "",             // 18 TODO
    "",             // 19 TODO
    "",             // 20 TODO
#endif
  },
  { "A software emulated MPEG2 device",   //  1
    "Software-Ausgabeger�t",              //  2
    "",             //  3 TODO
    "Software emulazione scheda MPEG2", //  4
    "",             //  5 TODO
    "",             //  6 TODO
    "Affichage vid�o sur carte graphique", //  7
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
    "����������-����������� MPEG-2 ����������",   // 17
    "",             // 18 TODO
    "",             // 19 TODO
    "",             // 20 TODO
#endif
  },
  { "Xv startup aspect",   //  1
    "Xv Startgr��e",       //  2
    "",             //  3 TODO
    "Aspetto avvio Xv",    //  4
    "",             //  5 TODO
    "",             //  6 TODO
    "Aspect Xv au d�marrage", //  7
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
    "������ ��� �������  Xv",  // 17
    "",             // 18 TODO
    "",             // 19 TODO
    "",             // 20 TODO
#endif
  },
  { "CropMode",         //  1
    "Bildausschnitt",   //  2
    "",             //  3 TODO
    "Modalit� compatta",//  4
    "",             //  5 TODO
    "",             //  6 TODO
    "D�coupage de l'image au ratio", //  7
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
    "����� ������� �����������", // 17
    "",             // 18 TODO
    "",             // 19 TODO
    "",             // 20 TODO
#endif
  },
  { "Deinterlace Method",   //  1
    "Deinterlace-Methode",   //  2
    "",             //  3 TODO
    "Metodo deinterlacciamento",  //  4
    "",             //  5 TODO
    "",             //  6 TODO
    "D�sentrelacement", //  7
    "",             //  8 TODO
    "K�yt� lomituksen poistoa", //  9
    "",             // 10 TODO
    "",             // 11 TODO
    "",             // 12 TODO
    "",             // 13 TODO
    "",             // 14 TODO
    "",             // 15 TODO
    "",             // 16 TODO
#if VDRVERSNUM >= 10316
    "����� ���������������", // 17
    "",             // 18 TODO
    "",             // 19 TODO
    "",             // 20 TODO
#endif
  },
  { "Pixel Format",   //  1
    "Pixelformat",   //  2
    "",             //  3 TODO
    "Formato pixel",  //  4
    "",             //  5 TODO
    "",             //  6 TODO
    "Format de pixel", //  7
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
    "������ �������",  // 17
    "",             // 18 TODO
    "",             // 19 TODO
    "",             // 20 TODO
#endif
  },
  { "Picture mirroring",    //  1
    "Spiegelbild",          //  2
    "",             //  3 TODO
    "Immagine specchiata",  //  4
    "",             //  5 TODO
    "",             //  6 TODO
    "Mirroir horizontal", //  7
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
    "���������� �����������",  // 17
    "",             // 18 TODO
    "",             // 19 TODO
    "",             // 20 TODO
#endif
  },
  { "A/V Delay",        //  1
    "A/V Verz�gerung",  //  2
    "",             //  3 TODO
    "Ritardo A/V",      //  4
    "",             //  5 TODO
    "",             //  6 TODO
    "D�lai A/V",    //  7
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
    "�������� A/V",     // 17
    "",             // 18 TODO
    "",             // 19 TODO
    "",             // 20 TODO
#endif
  },
  { "Screen Aspect",      //  1
    "Bildschirmformat",   //  2
    "",             //  3 TODO
    "Aspetto schermo",    //  4
    "",             //  5 TODO
    "",             //  6 TODO
    "Proportion de l'�cran", //  7
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
    "������ ������", // 17
    "",             // 18 TODO
    "",             // 19 TODO
    "",             // 20 TODO
#endif
  },
  { "Playback",     //  1
    "Wiedergabe",   //  2
    "",             //  3 TODO
    "Riproduzione", //  4
    "",             //  5 TODO
    "",             //  6 TODO
    "Affichage",    //  7
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
    "���������������", // 17
    "",             // 18 TODO
    "",             // 19 TODO
    "",             // 20 TODO
#endif
  },
  { "OSD alpha blending",   //  1
    "OSD Einblendung",      //  2
    "",             //  3 TODO
    "Trasparenza alpha OSD", //  4
    "",             //  5 TODO
    "",             //  6 TODO
    "Fusion du menu avec la vid�o", //  7
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
    "������������ OSD", // 17
    "",             // 18 TODO
    "",             // 19 TODO
    "",             // 20 TODO
#endif
  },
  { "on",           //  1
    "ein",          //  2
    "",             //  3 TODO
    "attivo",       //  4
    "",             //  5 TODO
    "",             //  6 TODO
    "oui",          //  7
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
    "���.",         // 17
    "",             // 18 TODO
    "",             // 19 TODO
    "",             // 20 TODO
#endif
  },
  { "off",          //  1
    "aus",          //  2
    "",             //  3 TODO
    "disattivo",    //  4
    "",             //  5 TODO
    "",             //  6 TODO
    "non",          //  7
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
    "����.",        // 17
    "",             // 18 TODO
    "",             // 19 TODO
    "",             // 20 TODO
#endif
  },
  { "AC3 Mode",     //  1
    "AC3 Modus",    //  2
    "",             //  3 TODO
    "Modalit� AC3", //  4
    "",             //  5 TODO
    "",             //  6 TODO
    "Sortie AC3",   //  7
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
    "����� AC3",    // 17
    "",             // 18 TODO
    "",             // 19 TODO
    "",             // 20 TODO
#endif
  },
  { "Buffer Mode",  //  1
    "Puffermodus",  //  2
    "",             //  3 TODO
    "Modalit� buffer", //  4
    "",             //  5 TODO
    "",             //  6 TODO
    "M�moire tampon", //  7
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
    "�������� �����",  // 17
    "",             // 18 TODO
    "",             // 19 TODO
    "",             // 20 TODO
#endif
  },
  { "CropModeToggleKey",                  //  1
    "Bildausschnitts-Taste",               //  2
    "",             //  3 TODO
    "Tasto cambiamento aspetto",          //  4
    "",             //  5 TODO
    "",             //  6 TODO
    "Bouton de changement d'aspect", //  7
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
    "������� ������������ ������� �������", // 17
    "",             // 18 TODO
    "",             // 19 TODO
    "",             // 20 TODO
#endif
  },
  { "Crop lines from top",                //  1
    "Zeilen von oben abschneiden",        //  2
    "",             //  3 TODO
    "Righe da togliere dall'alto",        //  4
    "",             //  5 TODO
    "",             //  6 TODO
    "Lignes � enlever en haut", //  7
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
    "O������� ����� ������", // 17
    "",             // 18 TODO
    "",             // 19 TODO
    "",             // 20 TODO
#endif
  },
  { "Crop lines from bottom",             //  1
    "Zeilen von unten abschneiden",       //  2
    "",             //  3 TODO
    "Righe da togliere dal basso",        //  4
    "",             //  5 TODO
    "",             //  6 TODO
    "Lignes � enlever en bas", //  7
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
    "O������� ����� �����", // 17
    "",             // 18 TODO
    "",             // 19 TODO
    "",             // 20 TODO
#endif
  },
  { "Crop columns from left",             //  1
    "Spalten von links abschneiden",      //  2
    "",             //  3 TODO
    "Colonne da togliere da sinistra",    //  4
    "",             //  5 TODO
    "",             //  6 TODO
    "Colonnes � enlever � gauche", //  7
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
    "O������� �����o� �����", // 17
    "",             // 18 TODO
    "",             // 19 TODO
    "",             // 20 TODO
#endif
  },
  { "Crop columns from right",            //  1
    "Spalten von rechts abschneiden",     //  2
    "",             //  3 TODO
    "Colonne da togliere da destra",      //  4
    "",             //  5 TODO
    "",             //  6 TODO
    "Colonnes � enlever � droite", //  7
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
    "O������� �����o� ������",  // 17
    "",             // 18 TODO
    "",             // 19 TODO
    "",             // 20 TODO
#endif
  },
  { "Postprocessing Method",              //  1
    "Nachbearbeitungs-Methode",        //  2
    "",             //  3 TODO
    "Metodo post elaborazione",             //  4
    "",             //  5 TODO
    "",             //  6 TODO
    "Post-traitement", //  7
    "",             //  8 TODO
    "K�yt� kuvan j�lkik�sittely�", //  9
    "",             // 10 TODO
    "",             // 11 TODO
    "",             // 12 TODO
    "",             // 13 TODO
    "",             // 14 TODO
    "",             // 15 TODO
    "",             // 16 TODO
#if VDRVERSNUM >= 10316
    "����� �������������", // 17
    "",             // 18 TODO
    "",             // 19 TODO
    "",             // 20 TODO
#endif
  },
  { "Postprocessing Quality",             //  1
    "Nachbearbeitungs-Qualit�t",          //  2
    "",             //  3 TODO
    "Qualit� post elaborazione",             //  4
    "",             //  5 TODO
    "",             //  6 TODO
    "Qualit� du post-traitement", //  7
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
    "�������� �������������", // 17
    "",             // 18 TODO
    "",             // 19 TODO
    "",             // 20 TODO
#endif
  },
  { "Use StretchBlit",                    //  1
    "StretchBlit verwenden",              //  2
    "",             //  3 TODO
    "Utilizza StretchBlit",               //  4
    "",             //  5 TODO
    "",             //  6 TODO
    "Utiliser StretchBlit", //  7
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
    "������������ StretchBlit",  // 17
    "",             // 18 TODO
    "",             // 19 TODO
    "",             // 20 TODO
#endif
  },
  { "none",         //  1
    "keine",        //  2
    "",             //  3 TODO
    "nessuno",      //  4
    "",             //  5 TODO
    "",             //  6 TODO
    "aucun",        //  7
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
    "������",       // 17
    "",             // 18 TODO
    "",             // 19 TODO
    "",             // 20 TODO
#endif
  },
  { "fast",         //  1
    "schnell",      //  2
    "",             //  3 TODO
    "veloce",       //  4
    "",             //  5 TODO
    "",             //  6 TODO
    "rapide",       //  7
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
    "������",       // 17
    "",             // 18 TODO
    "",             // 19 TODO
    "",             // 20 TODO
#endif
  },
  { "default",      //  1
    "standard",     //  2
    "",             //  3 TODO
    "predefinito",  //  4
    "",             //  5 TODO
    "",             //  6 TODO
    "d�faut",       //  7
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
    "�� ���������", // 17
    "",             // 18 TODO
    "",             // 19 TODO
    "",             // 20 TODO
#endif
  },
  { "playing",      //  1
    "aktiviert",    //  2
    "",             //  3 TODO
    "esecuzione",   //  4
    "",             //  5 TODO
    "",             //  6 TODO
    "activ�",       //  7
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
    "�������������", // 17
    "",             // 18 TODO
    "",             // 19 TODO
    "",             // 20 TODO
#endif
  },
  { "suspended",    //  1
    "deaktiviert",                        //  2
    "",             //  3 TODO
    "sospeso",      //  4
    "",             //  5 TODO
    "",             //  6 TODO
    "suspendu",     //  7
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
    "���������������",  // 17
    "",             // 18 TODO
    "",             // 19 TODO
    "",             // 20 TODO
#endif
  },
  { "pseudo",       //  1
    "pseudo",       //  2
    "",             //  3 TODO
    "pseudo",       //  4
    "",             //  5 TODO
    "",             //  6 TODO
    "pseudo",       //  7
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
    "������",       // 17
    "",             // 18 TODO
    "",             // 19 TODO
    "",             // 20 TODO
#endif
  },
  { "software",     //  1
    "software",     //  2 TODO
    "",             //  3 TODO
    "software",     //  4
    "",             //  5 TODO
    "",             //  6 TODO
    "logiciel",     //  7
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
    "����������",      // 17
    "",             // 18 TODO
    "",             // 19 TODO
    "",             // 20 TODO
#endif
  },
  { "safe",         //  1
    "sicher",       //  2
    "",             //  3 TODO
    "sicuro",       //  4
    "",             //  5 TODO
    "",             //  6 TODO
    "limit�e",      //  7
    "",             //  8 TODO
    "varma",        //  9
    "",             // 10 TODO
    "",             // 11 TODO
    "",             // 12 TODO
    "",             // 13 TODO
    "",             // 14 TODO
    "",             // 15 TODO
    "",             // 16 TODO
#if VDRVERSNUM >= 10316
    "���������",    // 17
    "",             // 18 TODO
    "",             // 19 TODO
    "",             // 20 TODO
#endif
  },
  { "good seeking",         //  1
    "suchlauf optimiert",   //  2
    "",             //  3 TODO
    "precisione",           //  4
    "",             //  5 TODO
    "",             //  6 TODO
    "pr�cision",    //  7
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
    "����� �����������",  // 17
    "",             // 18 TODO
    "",             // 19 TODO
    "",             // 20 TODO
#endif
  },
  { "HDTV",         //  1
    "HDTV",         //  2
    "",             //  3 TODO
    "HDTV",         //  4
    "",             //  5 TODO
    "",             //  6 TODO
    "HDTV",         //  7
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
    "HDTV (²�)",   // 17
    "",             // 18 TODO
    "",             // 19 TODO
    "",             // 20 TODO
#endif
  },
  { "16:9 wide",                          //  1
    "16:9 Breitbild",                     //  2
    "",             //  3 TODO
    "16:9 wide",    //  4
    "",             //  5 TODO
    "",             //  6 TODO
    "16:9 large",   //  7
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
    "16:9 ��������������", // 17
    "",             // 18 TODO
    "",             // 19 TODO
    "",             // 20 TODO
#endif
  },
  { "4:3 normal",                         //  1
    "4:3 Normalbild",                     //  2
    "",             //  3 TODO
    "4:3 normale",  //  4
    "",             //  5 TODO
    "",             //  6 TODO
    "4:3 normal",   //  7
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
    "4:3 ����������",  // 17
    "",             // 18 TODO
    "",             // 19 TODO
    "",             // 20 TODO
#endif
  },
  { "Hide main menu entry", //  1
    "Hauptmen�eintrag verstecken", //  2
    "",             //  3 TODO
    "Nascondi voce nel menu principale", //  4
    "",             //  5 TODO
    "",             //  6 TODO
    "",             //  7 TODO
    "",             //  8 TODO
    "Piilota valinta p��valikosta", //  9
    "",             // 10 TODO
    "",             // 11 TODO
    "",             // 12 TODO
    "",             // 13 TODO
    "",             // 14 TODO
    "",             // 15 TODO
    "",             // 16 TODO
#if VDRVERSNUM >= 10316
    "�������� ����� � ������� ����",  // 17
    "",             // 18 TODO
    "",             // 19 TODO
    "",             // 20 TODO
#endif
  },
  { "Cropping",      //  1
    "Bildausschnitt",//  2
    "",             //  3 TODO
    "Compattazione",//  4
    "",             //  5 TODO
    "",             //  6 TODO
    "",             //  7 TODO
    "",             //  8 TODO
    "Kuvan rajaus", //  9
    "",             // 10 TODO
    "",             // 11 TODO
    "",             // 12 TODO
    "",             // 13 TODO
    "",             // 14 TODO
    "",             // 15 TODO
    "",             // 16 TODO
#if VDRVERSNUM >= 10316
    "�������",     // 17
    "",             // 18 TODO
    "",             // 19 TODO
    "",             // 20 TODO
#endif
  },
  { "Post processing",//  1
    "Bildnachbearbeitung",  //  2
    "",             //  3 TODO
    "Post elaborazione",//  4
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
    "�������������", // 17
    "",             // 18 TODO
    "",             // 19 TODO
    "",             // 20 TODO
#endif
  },
  { "Sync Mode",    //  1
    "Sync Modus",   //  2
    "",             //  3 TODO
    "Modalit� Sync",//  4
    "",             //  5 TODO
    "",             //  6 TODO
    "",             //  7 TODO
    "",             //  8 TODO
    "A/V-synkronointitapa", //  9
    "",             // 10 TODO
    "",             // 11 TODO
    "",             // 12 TODO
    "",             // 13 TODO
    "",             // 14 TODO
    "",             // 15 TODO
    "",             // 16 TODO
#if VDRVERSNUM >= 10316
    "����� �������������", // 17
    "",             // 18 TODO
    "",             // 19 TODO
    "",             // 20 TODO
#endif
  },
  { "Video out",    //  1
    "Videoausgabe", //  2
    "",             //  3 TODO
    "Uscita video", //  4
    "",             //  5 TODO
    "",             //  6 TODO
    "",             //  7 TODO
    "",             //  8 TODO
    "Kuvasignaalin ulostulo", //  9
    "",             // 10 TODO
    "",             // 11 TODO
    "",             // 12 TODO
    "",             // 13 TODO
    "",             // 14 TODO
    "",             // 15 TODO
    "",             // 16 TODO
#if VDRVERSNUM >= 10316
    "����� �����",  // 17
    "",             // 18 TODO
    "",             // 19 TODO
    "",             // 20 TODO
#endif
  },
  { "Brightness",   //  1
    "Helligkeit",   //  2
    "",             //  3 TODO
    "Luminosit�",   //  4
    "",             //  5 TODO
    "",             //  6 TODO
    "",             //  7 TODO
    "",             //  8 TODO
    "Kirkkaus",     //  9
    "",             // 10 TODO
    "",             // 11 TODO
    "",             // 12 TODO
    "",             // 13 TODO
    "",             // 14 TODO
    "",             // 15 TODO
    "",             // 16 TODO
#if VDRVERSNUM >= 10316
    "�������",      // 17
    "",             // 18 TODO
    "",             // 19 TODO
    "",             // 20 TODO
#endif
  },
  { "Contrast",     //  1
    "Kontrast",     //  2
    "",             //  3 TODO
    "Contrasto",    //  4
    "",             //  5 TODO
    "",             //  6 TODO
    "",             //  7 TODO
    "",             //  8 TODO
    "Kontrasti",    //  9
    "",             // 10 TODO
    "",             // 11 TODO
    "",             // 12 TODO
    "",             // 13 TODO
    "",             // 14 TODO
    "",             // 15 TODO
    "",             // 16 TODO
#if VDRVERSNUM >= 10316
    "�������������",     // 17
    "",             // 18 TODO
    "",             // 19 TODO
    "",             // 20 TODO
#endif
  },
  { "Hue",          //  1
    "Farbart",      //  2
    "",             //  3 TODO
    "Tonalit�",     //  4
    "",             //  5 TODO
    "",             //  6 TODO
    "",             //  7 TODO
    "",             //  8 TODO
    "V�ris�vy",     //  9
    "",             // 10 TODO
    "",             // 11 TODO
    "",             // 12 TODO
    "",             // 13 TODO
    "",             // 14 TODO
    "",             // 15 TODO
    "",             // 16 TODO
#if VDRVERSNUM >= 10316
    "�������� �����",    // 17
    "",             // 18 TODO
    "",             // 19 TODO
    "",             // 20 TODO
#endif
  },
  { "Saturation",   //  1
    "Farbkontrast", //  2
    "",             //  3 TODO
    "Saturazione",  //  4
    "",             //  5 TODO
    "",             //  6 TODO
    "",             //  7 TODO
    "",             //  8 TODO
    "V�rikyll�isyys",//  9
    "",             // 10 TODO
    "",             // 11 TODO
    "",             // 12 TODO
    "",             // 13 TODO
    "",             // 14 TODO
    "",             // 15 TODO
    "",             // 16 TODO
#if VDRVERSNUM >= 10316
    "������������ �����", // 17
    "",             // 18 TODO
    "",             // 19 TODO
    "",             // 20 TODO
#endif
  },
  { "Zoom factor",   //  1
    "Zoomfaktor",    //  2
    "",             //  3 TODO
    "Fattore ingrandimento",//  4
    "",             //  5 TODO
    "",             //  6 TODO
    "",             //  7 TODO
    "",             //  8 TODO
    "Zoomauskerroin",//  9
    "",             // 10 TODO
    "",             // 11 TODO
    "",             // 12 TODO
    "",             // 13 TODO
    "",             // 14 TODO
    "",             // 15 TODO
    "",             // 16 TODO
#if VDRVERSNUM >= 10316
    "����������",   // 17
    "",             // 18 TODO
    "",             // 19 TODO
    "",             // 20 TODO
#endif
  },
  { "Zoom area shift (left/right)",   //  1
    "Zoomausschnitt (links/rechts)",  //  2
    "",             //  3 TODO
    "Cambia area zoom (sinistra/destra)",//  4
    "",             //  5 TODO
    "",             //  6 TODO
    "",             //  7 TODO
    "",             //  8 TODO
    "Zoomauksen siirto (vasen/oikea)",//  9
    "",             // 10 TODO
    "",             // 11 TODO
    "",             // 12 TODO
    "",             // 13 TODO
    "",             // 14 TODO
    "",             // 15 TODO
    "",             // 16 TODO
#if VDRVERSNUM >= 10316
    "����� ������� ���������� (�����/������)",  // 17
    "",             // 18 TODO
    "",             // 19 TODO
    "",             // 20 TODO
#endif
  },
  { "Zoom area shift (up/down)",   //  1
    "Zoomausschnitt (oben/unten)", //  2
    "",             //  3 TODO
    "Cambia area zoom (su/gi�)",   //  4
    "",             //  5 TODO
    "",             //  6 TODO
    "",             //  7 TODO
    "",             //  8 TODO
    "Zoomauksen siirto (yl�s/alas)",//  9
    "",             // 10 TODO
    "",             // 11 TODO
    "",             // 12 TODO
    "",             // 13 TODO
    "",             // 14 TODO
    "",             // 15 TODO
    "",             // 16 TODO
#if VDRVERSNUM >= 10316
    "����� ������� ���������� (�����/����)",  // 17
    "",             // 18 TODO
    "",             // 19 TODO
    "",             // 20 TODO
#endif
  },
  { "Expand top/bottom lines",   //  1
    "Zeilen vergr��ern (ob./un.)", //  2
    "",             //  3 TODO
    "Righe espansione alto/basso",//  4
    "",             //  5 TODO
    "",             //  6 TODO
    "",             //  7 TODO
    "",             //  8 TODO
    "Levit� laitimmaiset rivit",//  9
    "",             // 10 TODO
    "",             // 11 TODO
    "",             // 12 TODO
    "",             // 13 TODO
    "",             // 14 TODO
    "",             // 15 TODO
    "",             // 16 TODO
#if VDRVERSNUM >= 10316
    "���������� (���./����.) ������", // 17
    "",             // 18 TODO
    "",             // 19 TODO
    "",             // 20 TODO
#endif
  },
  { "Expand left/right columns",   //  1
    "Spalten vergr��en (li./re.)", //  2
    "",             //  3 TODO
    "Colonne espansione sinistra/destra",//  4
    "",             //  5 TODO
    "",             //  6 TODO
    "",             //  7 TODO
    "",             //  8 TODO
    "Levit� reunimmaiset sarakkeet",//  9
    "",             // 10 TODO
    "",             // 11 TODO
    "",             // 12 TODO
    "",             // 13 TODO
    "",             // 14 TODO
    "",             // 15 TODO
    "",             // 16 TODO
#if VDRVERSNUM >= 10316
    "���������� (���./����.) �������",  // 17
    "",             // 18 TODO
    "",             // 19 TODO
    "",             // 20 TODO
#endif
  },
  { "Logging",         //  1
    "Protokollierung", //  2
    "",             //  3 TODO
    "Log",             //  4
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
    "����������������",   // 17
    "",             // 18 TODO
    "",             // 19 TODO
    "",             // 20 TODO
#endif
  },
  { "Info Messages",         //  1
    "Info Nachrichten",      //  2
    "",             //  3 TODO
    "Messaggi informazione", //  4
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
    "����. ���������", // 17
    "",             // 18 TODO
    "",             // 19 TODO
    "",             // 20 TODO
#endif
  },
  { "Debug Messages",         //  1
    "Debug Nachrichten",      //  2
    "",             //  3 TODO
    "Messaggi debug",         //  4
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
    "��������� �������",  // 17
    "",             // 18 TODO
    "",             // 19 TODO
    "",             // 20 TODO
#endif
  },
  { "Trace Messages",        //  1
    "Test Nachrichten",      //  2
    "",             //  3 TODO
    "Messaggi prova",        //  4
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
    "������������ ���������",  // 17
    "",             // 18 TODO
    "",             // 19 TODO
    "",             // 20 TODO
#endif
  },
  { "Logfile",         //  1
    "Protokolldatei",  //  2
    "",             //  3 TODO
    "File di log",     //  4
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
    "���� �������", // 17
    "",             // 18 TODO
    "",             // 19 TODO
    "",             // 20 TODO
#endif
  },
  { "Append PID",   //  1
    "PID erg�nzen", //  2
    "",             //  3 TODO
    "Accoda PID",   //  4
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
    "�������� PID", // 17
    "",             // 18 TODO
    "",             // 19 TODO
    "",             // 20 TODO
#endif
  },
  { "Autodetect Movie Aspect",   //  1
    "Formaterkennung (16:9/4:3)", //  2
    "",             //  3 TODO
    "Rileva aspetto film (16:9/4:3)",             //  4
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
    "��������������� ������� (16:9/4:3)",  // 17
    "",             // 18 TODO
    "",             // 19 TODO
    "",             // 20 TODO
#endif
  },
  { "yes",
    "ja",
    "da",
    "si",
    "ja",
    "sim",
    "oui",
    "ja",
    "kyll�",
    "tak",
    "s�",
    "���",
    "ja",
    "da",
    "igen",
    "s�",
    "��",
    "da",
    "jah",
    "ja",
    "ano",
  },
  { "no",
    "nein",
    "ne",
    "no",
    "nee",
    "n�o",
    "non",
    "nei",
    "ei",
    "nie",
    "no",
    "���",
    "nej",
    "nu",
    "nem",
    "no",
    "���",
    "ne",
    "ei",
    "nej",
    "ne",
  },
  { NULL }
  };
