/*
 * softplay.h: A plugin for the Video Disk Recorder
 *
 * See the README file for copyright information and how to reach the author.
 *
 * $Id: softplay.h,v 1.5 2006/07/25 20:05:20 wachm Exp $
 */
#ifndef __SOFTPLAY_H__
#define __SOFTPLAY_H__

#include <vdr/plugin.h>

#include "Setup.h"

class cPlayList;
class cIndexIdx;

extern cIndexIdx *FileIndex;

//#define STR_LENGTH  120
#define STR_LENGTH  200
#define SHORT_STR   60
#define TO_STRING(x) STRINGIFY(x)
#define STRINGIFY(x) #x

#define PLAY_FILES      osUser1
#define CURR_PLAYLIST   osUser2
#define PLAY_CURR_FILE  osUser3

class cSoftPlay : public cPlugin {
private:
  // Add any member variables or functions you may need here.
  char start_path[60];

public:
  struct sPlayLists {
  	char Name[STR_LENGTH];
  	cPlayList *playList;
	sPlayLists * next;
  };
private:
  sPlayLists *Lists;

public:
  cPlayList *currList;
  bool currListIsTmp;
  char currListName[STR_LENGTH];
  
  char configDir[300];
 
public:
  cSoftPlay(void);
  virtual ~cSoftPlay();
  virtual const char *Version(void);
  virtual const char *Description(void);
  virtual const char *CommandLineHelp(void);
  virtual bool ProcessArgs(int argc, char *argv[]);
  virtual bool Initialize(void);
  virtual bool Start(void);
  virtual void Housekeeping(void);
  virtual const char *MainMenuEntry(void); 
  virtual cOsdObject *MainMenuAction(void);
  virtual cMenuSetupPage *SetupMenu(void);
  virtual bool SetupParse(const char *Name, const char *Value);

  void SetTmpCurrList(cPlayList *List);
  void SaveList(cPlayList *List, const char *Name=NULL);
  cPlayList *OpenList(const char *Name);
  void ScanForPlaylists();
  
  inline cPlayList *GetCurrList() 
  { return currList; };
  inline char *MediaPath() {return start_path;};
};
// --- cMenuDirectory -------------------------------------------

class cMenuDirectory : public cOsdMenu {
private:
  char start_path[STR_LENGTH];
  struct DirEntry {
      char name[STR_LENGTH];
      char title[STR_LENGTH];
      int type;
  } * Entries;
  int nEntries;
  int keySelNo;
  cPlayList *editList;
  void PrintItemName(char *Name, const struct DirEntry &Entry,int i);
  eOSState SelectEntry(int No, bool play);
  
public:
  bool PrepareDirectory(const char * path);
  cMenuDirectory(const char * path, cPlayList *EditList=NULL, const char *ToPos=NULL);
  virtual ~cMenuDirectory();
  virtual eOSState ProcessKey(eKeys Key);
  static cMenuDirectory * OpenLastBrowsedDir();
};

extern cSoftPlay *Softplay;

int SimpleHash( char const* str);

void PrintCutDownString(char *str, const char *orig, int len);

bool IsM3UFile(const char *const Filename);
bool IsStream(const char *const Filename);

void chomp(const char *const str);

void stripTrailingSpaces(char *str);
void stripTrailingNonLetters(char *str);


static inline void strlcpy(char *dest, const char *src, size_t n) {
        while ( *src && --n )
                *(dest++)=*(src++);
        *(dest)=0;
};

static inline void skipSpaces(const char *&pos) {
        if (!pos)
                return;
	while ( *pos==' ' )
		pos++;
};

static inline void nextField(const char *&pos) {        
	//skip whitespaces and the comma
	skipSpaces(pos);

	if ( *pos!=',' ) {
		pos=NULL;
		return;
	} else pos++;

	skipSpaces(pos);
};

const char *ReadQuotedString(char *out, 
		const char *Str);
// out should be at least STR_LENGTH large


#endif
