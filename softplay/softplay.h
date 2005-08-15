/*
 * softplay.h: A plugin for the Video Disk Recorder
 *
 * See the README file for copyright information and how to reach the author.
 *
 * $Id: softplay.h,v 1.3 2005/08/15 07:27:42 wachm Exp $
 */
#ifndef __SOFTPLAY_H__
#define __SOFTPLAY_H__

#include <vdr/plugin.h>


class cPlayList;
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
  int start_path_len;

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
  
  const char *configDir;
 
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
  void SaveList(cPlayList *List);
  cPlayList *OpenList();

  inline cPlayList *GetCurrList() 
  { return currList; };
  inline char *MediaPath() {return start_path;};
  inline int MediaPathLen() {return start_path_len;};
};

extern cSoftPlay *Softplay;

int32_t SimpleHash( char const* str);

void PrintCutDownString(char *str,char *orig,int len);

bool IsM3UFile(const char *const Filename);

void chomp(const char *const str);
#endif
