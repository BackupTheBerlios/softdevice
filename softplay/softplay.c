/*
 * softplay.c: A plugin for the Video Disk Recorder
 *
 * See the README file for copyright information and how to reach the author.
 *
 * $Id: softplay.c,v 1.4 2005/05/09 21:40:05 wachm Exp $
 */


#include "softplay.h"
#include "SoftPlayer.h"
#include "PlayList.h"

#include <dirent.h>

#define NAME_LENGTH 200

static const char *VERSION        = "0.0.1";
static const char *DESCRIPTION    = "SoftPlay play media files with the softdevice";
static const char *MAINMENUENTRY  = "SoftPlay";


// --- cMenuDirectory -------------------------------------------

class cMenuDirectory : public cOsdMenu {
private:
  char start_path[NAME_LENGTH];
  struct DirEntry {
      char name[NAME_LENGTH];
      char title[NAME_LENGTH];
      int type;
  } * Entries;
  int nEntries;
  int keySelNo;
      
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
  keySelNo=0;
  SetHelp(NULL,"Play","Add To List","Play List");
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
  char Title[60];


  if (Entries) {
  	delete Entries;
	nEntries=0;
  };

  strncpy(start_path,path,NAME_LENGTH-1);
  start_path[NAME_LENGTH]=0;

  //FIXME find a clever way to cut down the directory name
  snprintf(Title,60,"Files: %s",start_path);
  SetTitle(Title);

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
          strncpy(Entries[i].title,namelist[i]->d_name,NAME_LENGTH);
          Entries[i].title[NAME_LENGTH-1]=0;

	  Entries[i].type=namelist[i]->d_type;
          // check type (non ext2/3 and symlinks)
          if ( Entries[i].type == 0 || Entries[i].type == DT_LNK ) {
                  struct stat stbuf;
                  if ( !stat(Entries[i].name,&stbuf) ) {
                          if ( S_ISDIR(stbuf.st_mode) ) 
                                  Entries[i].type = DT_DIR;
                          else if ( S_ISREG(stbuf.st_mode) )
                                  Entries[i].type = DT_REG;
                  };
          };
	  
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

	if (state != osUnknown) 
		return state;

	switch (Key) {
                case k0 ... k9:
                        {
                                keySelNo*=10;keySelNo+=(Key - k0);
                                int pos=1000;
                                while (keySelNo>nEntries) {
                                        keySelNo%=pos;pos/=10;
                                };
                                SetCurrent(Get(keySelNo-1));
                                Display();
                                printf("key %d keySelNo %d\n",(Key-k0),keySelNo);
                                break;
                        }
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
		case kGreen:
			sscanf(Get(Current())->Text(),"%d ",&No);
			No--;
			if (No>nEntries)
				break;
			if ( Entries[No].type == DT_REG ) { 
				cControl::Launch(new cSoftControl(Entries[No].name));
				return osEnd;
			} else if ( Entries[No].type == DT_DIR ) {
				printf("create playlist %s\n",Entries[No].name);
				cPlayList *Playlist=new cPlayList;
				Playlist->AddDir(Entries[No].name,
					Entries[No].title,true);
                                Softplay->SetTmpCurrList(Playlist);
				cControl::Launch(
				  	new cSoftControl(Playlist));
				return osEnd;
			};
			break;
                case kYellow: 
                        sscanf(Get(Current())->Text(),"%d ",&No);
                        No--;
                        if (No>nEntries)
                                break;
                        {
                                cPlayList *PlayList=Softplay->GetCurrList();
                                if (!PlayList) {
                                        PlayList = new cPlayList;
                                        Softplay->SetTmpCurrList(PlayList);
                                };
                                if (Entries[No].type == DT_DIR)
                                        PlayList->AddDir(Entries[No].name,
						Entries[No].title,true);
                                else if (Entries[No].type == DT_REG)
                                        PlayList->AddFile(Entries[No].name,
                                                        Entries[No].title);
                                break;
                        };
                case kBlue:
                        {
                                cPlayList *PlayList=Softplay->GetCurrList();
                                cControl::Launch(
				  	new cSoftControl(PlayList));
				return osEnd;
                                break;
                        };
		default:
			break;
	};
	return state;
};

// ----cMainMenu --------------------------------------------------------

class cMainMenu: public cOsdMenu {
        private:
                cSoftPlay::sPlayLists **lists;
                cPlayList **currList;

        public:
                cMainMenu(cPlayList **CurrList,
                                cSoftPlay::sPlayLists **Lists);
                virtual ~cMainMenu();
                void PrepareMenu();
                virtual eOSState ProcessKey(eKeys Key);
};

cMainMenu::cMainMenu(cPlayList **CurrList,cSoftPlay::sPlayLists **Lists)
        : cOsdMenu("SoftPlay")  {
        currList=CurrList;
        lists=Lists;
};
  
cMainMenu::~cMainMenu() {
};

void cMainMenu::PrepareMenu() {
        Add(new cOsdItem("Play Files",osUser1),false);
        if ( *currList )
          Add(new cOsdItem("current playlist",osUser2),false);
}

eOSState cMainMenu::ProcessKey(eKeys Key) {
  cOsdMenu *Menu;
  eOSState state = cOsdMenu::ProcessKey(Key);
  
  switch (state) {
    case osUser1:  
            Menu=new cMenuDirectory;
            ((cMenuDirectory*)Menu)->PrepareDirectory(Softplay->MediaPath());
            return AddSubMenu(Menu);
            break;

    case osUser2: return AddSubMenu((*currList)->ReplayList());
            break;
    case osUser3: cControl::Launch(
				  new cSoftControl(*currList));
                  return osEnd;
                  break;

    default: switch (Key) {
               default:      break;
               }
    }
  return state;
}


// --- cSoftPlay --------------------------------------------------------

cSoftPlay *Softplay;

cSoftPlay::cSoftPlay(void){
        // Initialize any member variables here.
        // DON'T DO ANYTHING ELSE THAT MAY HAVE SIDE EFFECTS, REQUIRE GLOBAL
        // VDR OBJECTS TO EXIST OR PRODUCE ANY OUTPUT!
        Softplay=this;
        Lists=NULL;
        currList=NULL;
}

cSoftPlay::~cSoftPlay() {
        // Clean up after yourself!
        if (currList && currListIsTmp)
                delete currList;
}

const char *cSoftPlay::Version(void) { 
        return VERSION; 
};      

const char *cSoftPlay::Description(void) { 
        return DESCRIPTION; 
};
  
const char *cSoftPlay::MainMenuEntry(void) { 
        return MAINMENUENTRY; 
};

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
        // not playing a list and no playlists available
        if ( !currList && !Lists ) {
                cMenuDirectory *Menu=new cMenuDirectory;
                Menu->PrepareDirectory(start_path);
                return Menu;
        };
        cMainMenu *Menu=new cMainMenu(&currList,&Lists);
        Menu->PrepareMenu();
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

void cSoftPlay::SetTmpCurrList(cPlayList *List) {
        if (currList && currListIsTmp)
                delete currList;
        currList=List;
};

VDRPLUGINCREATOR(cSoftPlay); // Don't touch this!
