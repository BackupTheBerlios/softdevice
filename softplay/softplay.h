/*
 * softplay.h: A plugin for the Video Disk Recorder
 *
 * See the README file for copyright information and how to reach the author.
 *
 * $Id: softplay.h,v 1.1 2005/05/07 20:14:01 wachm Exp $
 */
#ifndef __SOFTPLAY_H__
#define __SOFTPLAY_H__

#include <vdr/plugin.h>

#include "PlayList.h"


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
  
public:
  cSoftPlay(void);
  virtual ~cSoftPlay();
  virtual const char *Version(void);
  virtual const char *Description(void);
  virtual const char *CommandLineHelp(void);
  virtual bool ProcessArgs(int argc, char *argv[]);
  virtual bool Start(void);
  virtual void Housekeeping(void);
  virtual const char *MainMenuEntry(void); 
  virtual cOsdObject *MainMenuAction(void);
  virtual cMenuSetupPage *SetupMenu(void);
  virtual bool SetupParse(const char *Name, const char *Value);

  void SetTmpCurrList(cPlayList *List);
  inline cPlayList *GetCurrList() 
  { return currList; };
  inline char *MediaPath() {return start_path;};
};

extern cSoftPlay *Softplay;

#endif
