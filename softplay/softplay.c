/*
 * softplay.c: A plugin for the Video Disk Recorder
 *
 * See the README file for copyright information and how to reach the author.
 *
 * $Id: softplay.c,v 1.2 2005/05/07 09:07:57 wachm Exp $
 */

#include <vdr/plugin.h>

#include "SoftPlayer.h"

#include <dirent.h>

#define NAME_LENGTH 120

static const char *VERSION        = "0.0.1";
static const char *DESCRIPTION    = "SoftPlay play media files with the softdevice";
static const char *MAINMENUENTRY  = "SoftPlay";


// --- cMenuDirectory -------------------------------------------

class cMenuDirectory : public cOsdMenu {
private:
  char start_path[NAME_LENGTH];
  struct DirEntry {
      char name[NAME_LENGTH];
      int type;
  } * Entries;
  int nEntries;
      
public:
  void PrepareDirectory(char * path);
  cMenuDirectory(void);
  virtual ~cMenuDirectory();
  virtual eOSState ProcessKey(eKeys Key);
};

cMenuDirectory::cMenuDirectory(void) : cOsdMenu("Files") 
{
  Entries=NULL;
  nEntries=0;
};

cMenuDirectory::~cMenuDirectory()
{
  delete Entries;
  nEntries=0;
};

void cMenuDirectory::PrepareDirectory(char *path) 
{
  struct dirent **namelist;
  int n;
  char Name[60];

  if (Entries) {
  	delete Entries;
	nEntries=0;
  };

  strncpy(start_path,path,NAME_LENGTH-1);
  start_path[NAME_LENGTH]=0;

  n = scandir(path, &namelist, 0, alphasort);
  if (n<0) {
	  printf("scandir error\n");
	  return;
  };
  Entries=new DirEntry[n];
  nEntries=n;
  
  for (int i=0; i<n; i++) {
  	  // fill Entries array and resolve symlinks
	  snprintf(Entries[i].name,NAME_LENGTH,"%s/%s",
	            start_path,namelist[i]->d_name);
	  Entries[i].name[NAME_LENGTH-1]=0;
	  Entries[i].type=namelist[i]->d_type;
	  
	  // add to menu using original names (symlinks!!)
	  if (Entries[i].type == DT_DIR)  
	    snprintf(Name,60," %4d [%s]",i+1,namelist[i]->d_name);
	  else  snprintf(Name,60," %4d %s",i+1,namelist[i]->d_name);
	  
	  Add(new cOsdItem(strdup(Name),osUnknown),false);
	  printf("Name %s type %d \n",Entries[i].name,Entries[i].type);

          free(namelist[i]);	  
  }
  free(namelist);
};

eOSState cMenuDirectory::ProcessKey(eKeys Key) {
  eOSState state = cOsdMenu::ProcessKey(Key);
  int No=0;

  if (state == osUnknown) {
     switch (Key) {
       case kOk: 
         sscanf(Get(Current())->Text(),"%d ",&No);
	 No--;
	 if (No>nEntries)
	    break;
	 if ( Entries[No].type == DT_REG ) { 
		 cControl::Launch(new cSoftControl(Entries[No].name));
		 return osEnd;
	} else if ( Entries[No].type == DT_DIR ) { 
		cMenuDirectory *Menu=new cMenuDirectory;
		Menu->PrepareDirectory(Entries[No].name);
		return AddSubMenu(Menu);
	};
	break;
       default:
         break;
     };
  }
  return state;
};

// --- cSoftPlay --------------------------------------------------------

class cSoftPlay : public cPlugin {
private:
  // Add any member variables or functions you may need here.
  char start_path[60];
public:
  cSoftPlay(void);
  virtual ~cSoftPlay();
  virtual const char *Version(void) { return VERSION; }
  virtual const char *Description(void) { return DESCRIPTION; }
  virtual const char *CommandLineHelp(void);
  virtual bool ProcessArgs(int argc, char *argv[]);
  virtual bool Start(void);
  virtual void Housekeeping(void);
  virtual const char *MainMenuEntry(void) { return MAINMENUENTRY; }
  virtual cOsdObject *MainMenuAction(void);
  virtual cMenuSetupPage *SetupMenu(void);
  virtual bool SetupParse(const char *Name, const char *Value);
};

cSoftPlay::cSoftPlay(void)
{
  // Initialize any member variables here.
  // DON'T DO ANYTHING ELSE THAT MAY HAVE SIDE EFFECTS, REQUIRE GLOBAL
  // VDR OBJECTS TO EXIST OR PRODUCE ANY OUTPUT!
}

cSoftPlay::~cSoftPlay()
{
  // Clean up after yourself!
}

const char *cSoftPlay::CommandLineHelp(void)
{
  // Return a string that describes all known command line options.
  return 
   " --media-path path to media files\n";
}

bool cSoftPlay::ProcessArgs(int argc, char *argv[])
{
  // Implement command line argument processing here if applicable.
  int i=0;
  while ( argc > 0 ) {
  	if (!strcmp (argv[i], "--media-path")) {
		argc--;
		i++;
		if (argc>0) {
			strncpy(start_path,argv[i],60);
			start_path[59]=0;
			argc--;
			i++;
		};
	} else argc--;
	i++;
  };
  return true;
}

bool cSoftPlay::Start(void)
{
  // Start any background activities the plugin shall perform.
  return true;
}

void cSoftPlay::Housekeeping(void)
{
  // Perform any cleanup or other regular tasks.
}

cOsdObject *cSoftPlay::MainMenuAction(void)
{
  cMenuDirectory *Menu=new cMenuDirectory;
  Menu->PrepareDirectory(start_path);
  //cControl::Launch(new cSoftControl);
  return Menu;
}

cMenuSetupPage *cSoftPlay::SetupMenu(void)
{
  // Return a setup menu in case the plugin supports one.
  return NULL;
}

bool cSoftPlay::SetupParse(const char *Name, const char *Value)
{
  // Parse your own setup parameters and store their values.
  return false;
}

VDRPLUGINCREATOR(cSoftPlay); // Don't touch this!
