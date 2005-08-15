/*
 * softplay.c: A plugin for the Video Disk Recorder
 *
 * See the README file for copyright information and how to reach the author.
 *
 * $Id: softplay.c,v 1.8 2005/08/15 07:27:42 wachm Exp $
 */


#include "softplay.h"
#include "SoftPlayer.h"
#include "PlayList.h"
#include "i18n.h"

#include <dirent.h>

#define NAME_LENGTH 200

static const char *VERSION        = "0.0.2";
static const char *DESCRIPTION    = "SoftPlay play media files with the softdevice";
static const char *MAINMENUENTRY  = "SoftPlay";

static const char *DIR_NAME ="softplay";

//#define PLUGDEB(out...)     {printf("PLUGDEB: ");printf(out...);}

#ifndef PLUGDEB
#define PLUGDEB(out...)
#endif

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
  cPlayList *editList;
  void PrintItemName(char *Name, const struct DirEntry &Entry,int i);
  eOSState SelectEntry(int No, bool play);
  
public:
  void PrepareDirectory(char * path);
  cMenuDirectory(char * path, cPlayList *EditList=NULL);
  virtual ~cMenuDirectory();
  virtual eOSState ProcessKey(eKeys Key);
};

cMenuDirectory::cMenuDirectory(char * path, cPlayList *EditList) 
        : cOsdMenu(tr("Files"),4,2,8) 
{
  Entries=NULL;
  nEntries=0;
  keySelNo=0;
  SetHelp(NULL,tr("Play"),tr("Toggle List"),tr("Play List"));
  editList=EditList;
  PrepareDirectory(path);
};

cMenuDirectory::~cMenuDirectory()
{
  delete Entries;
  nEntries=0;
};

void cMenuDirectory::PrintItemName(char *Name, const struct DirEntry &Entry,int i) {
        char inPlayList=' ';
        if (Entry.type == DT_DIR ) {  
                if (editList && editList->GetAlbumByFilename(Entry.name))
                        inPlayList='*';

                snprintf(Name,60,"%4d\t%c\t[%s]",
                                i+1,inPlayList,Entry.title);
        } else {
                if ( IsM3UFile(Entry.name) ) {
                        if (editList && editList->GetAlbumByFilename(Entry.name))
                                inPlayList='*';
                } else {  
                        if (editList && editList->GetItemByFilename(Entry.name))
                                inPlayList='*';
                }
                snprintf(Name,60,"%4d\t%c\t%s",
                                i+1,inPlayList,Entry.title);
        };
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

  strncpy(start_path,path,NAME_LENGTH);
  start_path[NAME_LENGTH-1]=0;

  //FIXME find a clever way to cut down the directory name
  PrintCutDownString(Name,&start_path[Softplay->MediaPathLen()],30);
  snprintf(Title,60,tr("Files: %s"),Name);
  SetTitle(Title);

  n = scandir(path, &namelist, 0, alphasort);
  if (n<0) {
	  printf("scandir error\n");
	  return;
  };
  Entries=new DirEntry[n];
  nEntries=0;
  
  for (int i=0; i<n; i++) {
	  if ( !strcmp("..",namelist[i]->d_name) ||
			  !strcmp(".",namelist[i]->d_name) ) {
		  PLUGDEB("ignore %s\n",namelist[i]->d_name);
		  continue;
	  };

	  // fill Entries array and resolve symlinks
	  snprintf(Entries[nEntries].name,NAME_LENGTH,"%s/%s",
	            start_path,namelist[i]->d_name);
	  Entries[nEntries].name[NAME_LENGTH-1]=0;
          strncpy(Entries[nEntries].title,namelist[i]->d_name,NAME_LENGTH);
          Entries[nEntries].title[NAME_LENGTH-1]=0;

	  Entries[nEntries].type=namelist[i]->d_type;
          // check type (non ext2/3 and symlinks)
          if ( Entries[nEntries].type == 0 || 
                          Entries[nEntries].type == DT_LNK ) {
                  struct stat stbuf;
                  if ( !stat(Entries[nEntries].name,&stbuf) ) {
                          if ( S_ISDIR(stbuf.st_mode) ) 
                                  Entries[nEntries].type = DT_DIR;
                          else if ( S_ISREG(stbuf.st_mode) )
                                  Entries[nEntries].type = DT_REG;
                  };
          };
	  
	  // add to menu using original names
          PrintItemName(Name, Entries[nEntries],nEntries);

	  Add(new cOsdItem(strdup(Name),osUnknown),false);
	  PLUGDEB("Name %s type %d \n",Entries[nEntries].name
                          ,Entries[nEntries].type);

          nEntries++;
          free(namelist[i]);	  
  }
  free(namelist);
};

eOSState cMenuDirectory::SelectEntry(int No, bool play) {
        bool refreshAll=false;
        
        if (No>nEntries)
                return osContinue;

        if ( Entries[No].type == DT_REG && play
                        && !IsM3UFile(Entries[No].name) ) { 
                cControl::Launch(new cSoftControl(Entries[No].name));
                return osEnd;
        };

        cPlayListItem *Item;
        cPlayList *PlayList=Softplay->GetCurrList();
        if (!PlayList || play ) {
                editList = PlayList = new cPlayList;
                Softplay->SetTmpCurrList(PlayList);
        };
        if (Entries[No].type == DT_DIR) {
                Item=PlayList->GetAlbumByFilename(Entries[No].name);
                PLUGDEB("Item %p\n",Item);
                if (!Item)
                        PlayList->AddDir(Entries[No].name,
                                        Entries[No].title,true);
                else PlayList->RemoveItem(Item);
        } else if (Entries[No].type == DT_REG && 
                        IsM3UFile(Entries[No].name) ) {
                Item=PlayList->GetAlbumByFilename(Entries[No].name);
                PLUGDEB("Item %p\n",Item);
                if (!Item)
                        PlayList->AddM3U(Entries[No].name,
                                        Entries[No].title);
                else PlayList->RemoveItem(Item);   
                refreshAll=true;
        } else if (Entries[No].type == DT_REG){
                Item=PlayList->GetItemByFilename(Entries[No].name);
                if (!Item)
                        PlayList->AddFile(Entries[No].name,
                                        Entries[No].title);
                else PlayList->RemoveItem(Item);
        };
        if (play) {
                // FIXME remove
               Softplay->SaveList(PlayList);
               cControl::Launch(new cSoftControl(PlayList));
               return osEnd;
        };

        if (refreshAll) {
                char Name[60];
                int current=Current();
                Clear();
                //PrepareDirectory(start_path);
                for (int i=0; i<nEntries; i++) {
                        PrintItemName(Name, Entries[i],i);
                        Add(new cOsdItem(strdup(Name),osUnknown),false);
                };
                SetCurrent(Get(current));        
                Display();
                return osContinue;
        };
                       
        char str[60];
        PrintItemName(str,Entries[No],No);
        Get(Current())->SetText(str,true);
        DisplayCurrent(true);

        return osContinue;
};

eOSState cMenuDirectory::ProcessKey(eKeys Key) {
	eOSState state = cOsdMenu::ProcessKey(Key);
        cPlayListItem *Item;
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
                                PLUGDEB("key %d keySelNo %d\n",(Key-k0),keySelNo);
                                break;
                        }
		case kOk: 
			No=Current();
			if (No>nEntries)
				break;
			if ( Entries[No].type == DT_REG ) {
                                // play current entry
				return SelectEntry(Current(),true);
			} else if ( Entries[No].type == DT_DIR ) { 
				return AddSubMenu(
                                        new cMenuDirectory(Entries[No].name,
                                            Softplay->GetCurrList()));
			};      
			break;
		case kGreen:
                        // play current entry
			state=SelectEntry(Current(),true);
                        break;
                case kYellow: 
                        // add / remove current entry 
                        state=SelectEntry(Current(),false);
                        break;
                case kBlue:
                        {
                                cPlayList *PlayList=Softplay->GetCurrList();
				if (PlayList)
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
        : cOsdMenu(tr("SoftPlay"))  {
        currList=CurrList;
        lists=Lists;
        SetHelp(NULL,NULL,NULL,tr("Play List"));
};
  
cMainMenu::~cMainMenu() {
};

void cMainMenu::PrepareMenu() {
        Add(new cOsdItem(tr("Play Files"),PLAY_FILES),false);
        if ( *currList )
          Add(new cOsdItem(tr("current playlist"),CURR_PLAYLIST),false);
}

eOSState cMainMenu::ProcessKey(eKeys Key) {
  eOSState state = cOsdMenu::ProcessKey(Key);
  
  switch (state) {
    case PLAY_FILES:  
            return AddSubMenu(new cMenuDirectory(Softplay->MediaPath(),
                                    Softplay->GetCurrList()));
            break;

    case CURR_PLAYLIST: return AddSubMenu(new cEditList(*currList));
            break;

    default: switch (Key) {
                     case kBlue:
                             if (*currList) {
                              cControl::Launch(new cSoftControl(*currList));
                              return osEnd;
                             };
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

bool cSoftPlay::Initialize(void) {
	configDir=ConfigDirectory(DIR_NAME);
        currList=OpenList();
	return true;
};
	
cSoftPlay::~cSoftPlay() {
        // Clean up after yourself!
        if (currList && currListIsTmp)
                delete currList;
}

const char *cSoftPlay::Version(void) { 
        return VERSION; 
};      

const char *cSoftPlay::Description(void) { 
        return tr(DESCRIPTION); 
};
  
const char *cSoftPlay::MainMenuEntry(void) { 
        return tr(MAINMENUENTRY); 
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
                        start_path_len=strlen(start_path)+1;
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
  RegisterI18n(Phrases);
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
                return new cMenuDirectory(start_path,Softplay->GetCurrList());
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

void cSoftPlay::SaveList(cPlayList *List) {
	char filename[60];
	sprintf(filename,"%s/%s",configDir,List->GetName());

	printf("Save list as %s\n",filename);
	FILE *out=fopen(filename,"w");
	if (!out)
		return;
	List->Save(out);
	fclose(out);
};

cPlayList *cSoftPlay::OpenList() {
        char filename[60];
        char line[500];
	sprintf(filename,"%s/%s",configDir,"Liste 1");

	printf("Read list from %s\n",filename);
	FILE *out=fopen(filename,"r");
        if (!out)
                return NULL;
        fgets(line,500,out);
        
        cPlayList *List=new cPlayList();
	List->Load(out,line);
	fclose(out);
        return List;
};

VDRPLUGINCREATOR(cSoftPlay); // Don't touch this!

//------------------------------------------------------------------------
const int32_t Divisor=0xfda9743d;
//const int32_t Divisor=0xfda97431;

int32_t SimpleHash( char const* str) {
        // just used to fast identify strings. 
        // I guess this can be made much better.
        // FIXME buggy?
        //printf("String: %s",str);
        int result=0;
        do {
                result=((result<<8)+*str) % Divisor;
        } while ( *(++str) );
        //printf(" Hash: 0x%x\n",result);
        return result;
};

void PrintCutDownString(char *str,char *orig,int len) {
#define STARTCPY 4 
// length of the copy at the beginning
        int Pos=STARTCPY;
        int origlen=strlen(orig);
        if (origlen<len) {
                strcpy(str,orig);
                PLUGDEB("just copy str: %s\n",str);
                return;
        };
        if (len<15) {
                PLUGDEB("CutDownString len %d is too small!\n",len);
                return;
        };
        strncpy(str,orig,Pos);
        str[Pos++]='.';
        str[Pos++]='.';
        str[Pos++]='.';
        strncpy(&str[Pos],&orig[origlen-len+1+STARTCPY+3],len-STARTCPY-3);
        str[len-1]=0;
        PLUGDEB("before end copy %s\n",str);
};
 
bool IsM3UFile( const char * const Filename) {
        char * pos;
        pos = rindex(Filename,'.');
        if ( !pos )
                return false;
        if ( !strcmp(pos,".m3u") ) {
                return true;
        };

        return false;
};

void chomp(const char *const str) {
        char *pos;
        pos = index(str,'\n');
        if (pos)
              *pos=0;
        pos = index(str,'\r');
        if (pos)
                *pos=0;
};

