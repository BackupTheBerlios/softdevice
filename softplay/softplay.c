/*
 * softplay.c: A plugin for the Video Disk Recorder
 *
 * See the README file for copyright information and how to reach the author.
 *
 * $Id: softplay.c,v 1.11 2006/04/02 20:19:12 wachm Exp $
 */


#include <dirent.h>

#include "softplay.h"
#include "SoftPlayer.h"
#include "PlayList.h"
#include "PlayListMenu.h"
#include "i18n.h"
#include "FileIndex.h"


static const char *VERSION        = "0.0.2";
static const char *DESCRIPTION    = "SoftPlay play media files with the softdevice";
static const char *MAINMENUENTRY  = "SoftPlay";

static const char *DIR_NAME ="softplay";

//#define PLUGDEB(out...)     {printf("PLUGDEB: ");printf(out);}

#ifndef PLUGDEB
#define PLUGDEB(out...)
#endif

// --- cMenuDirectory -------------------------------------------

cMenuDirectory::cMenuDirectory(const char * path, cPlayList *EditList, 
		const char *ToPos) 
        : cOsdMenu(tr("Files"),4,2,8) 
{
  Entries=NULL;
  nEntries=0;
  keySelNo=0;
  SetHelp(NULL,tr("Play"),tr("Toggle List"),tr("Play List"));
  editList=EditList;
  if (ToPos) {
	  printf("Opening dir '%s' until '%s'\n",path,ToPos);
	  char myDir[120];
	  char *myToPos;
	  strlcpy(myDir,path,
	      ((unsigned int)(ToPos-path+1))>(unsigned int)sizeof(myDir)?
	      sizeof(myDir): (unsigned int)(ToPos-path+1));
	  printf("MyDir '%s'\n",myDir);
	  PrepareDirectory(myDir);
	  myToPos=index(ToPos+1,'/');
	  AddSubMenu(new cMenuDirectory(path,EditList,myToPos));
  } else  {
	  printf("Open only directory '%s'.\n",path);
	  PrepareDirectory(path);
	  SoftplaySetup.SetLastDir(path);
  };
	  
};

cMenuDirectory::~cMenuDirectory()
{
  delete Entries;
  nEntries=0;
};

cMenuDirectory * cMenuDirectory::OpenLastBrowsedDir() {
	const char *lastDir=SoftplaySetup.GetLastDir();
	const char *startFrom=lastDir+strlen(Softplay->MediaPath());
	printf("OpenLastBrowsedDir: '%s' startFrom '%s'\n",lastDir,startFrom);
	return new cMenuDirectory(lastDir,Softplay->GetCurrList(),startFrom);
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

bool cMenuDirectory::PrepareDirectory(const char *path) 
{
  struct dirent **namelist;
  int n;
  char Name[60];
  char Title[60];


  if (Entries) {
  	delete Entries;
	nEntries=0;
  };

  strlcpy(start_path,path,sizeof(start_path));

  //FIXME find a clever way to cut down the directory name
  PrintCutDownString(Name,start_path,30);
  snprintf(Title,60,tr("Files: %s"),Name);
  SetTitle(Title);

  n = scandir(path, &namelist, 0, alphasort);
  if (n<0) {
	  printf("scandir error\n");
	  return false;
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
	  snprintf(Entries[nEntries].name,STR_LENGTH,"%s/%s",
	            start_path,namelist[i]->d_name);
	  Entries[nEntries].name[STR_LENGTH-1]=0;
          strlcpy(Entries[nEntries].title,namelist[i]->d_name,STR_LENGTH);
          Entries[nEntries].title[STR_LENGTH-1]=0;

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
	  
	  // add to menu useing original names
          PrintItemName(Name, Entries[nEntries],nEntries);

	  Add(new cOsdItem(strdup(Name),osUnknown),false);
	  PLUGDEB("Name %s type %d \n",Entries[nEntries].name
                          ,Entries[nEntries].type);

          nEntries++;
          free(namelist[i]);	  
  }
  free(namelist);
  return true;
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
                        PlayList->AddDir(Entries[No].name,true);
                else PlayList->RemoveItem(Item);
        } else if (Entries[No].type == DT_REG && 
                        IsM3UFile(Entries[No].name) ) {
                Item=PlayList->GetAlbumByFilename(Entries[No].name);
                PLUGDEB("Item %p\n",Item);
                if (!Item)
                        PlayList->AddM3U(Entries[No].name);
                else PlayList->RemoveItem(Item);   
                refreshAll=true;
        } else if (Entries[No].type == DT_REG){
                Item=PlayList->GetItemByFilename(Entries[No].name);
                if (!Item)
                        PlayList->AddFile(Entries[No].name);
                else PlayList->RemoveItem(Item);
        };
        if (play) {
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
            return  ( SoftplaySetup.OpenLastDir() ?
		    AddSubMenu(cMenuDirectory::OpenLastBrowsedDir()) :
		    AddSubMenu(new cMenuDirectory(Softplay->MediaPath(),
                                    Softplay->GetCurrList())) );
            break;

    case CURR_PLAYLIST: return AddSubMenu(new cAlbumList(*currList));
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
	strlcpy(configDir,ConfigDirectory(DIR_NAME),sizeof(configDir));
        currList=OpenList("current");
	if (!FileIndex)
                FileIndex=new cIndexIdx;

        FileIndex->ReadIndex("/video/plugins/softplay/index");
        
        ScanForPlaylists();
	return true;
};
	
cSoftPlay::~cSoftPlay() {
        if (currList)
                // FIXME remove
              SaveList(currList,"current");

        // Clean up after yourself!
        if (currList && currListIsTmp)
                delete currList;

        if (FileIndex){
		// FIXME remove
		FileIndex->SaveIndex("/video/plugins/softplay/index");
                delete FileIndex;
	};
};

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
			strlcpy(start_path,argv[i],sizeof(start_path));
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
  SoftplaySetup.SetPlugin(this);
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
  return new cSoftplaySetupMenu(this,&SoftplaySetup);
}

bool cSoftPlay::SetupParse(const char *Name, const char *Value)
{
  // Parse your own setup parameters and store their values.
  return SoftplaySetup.Parse(Name,Value);
}

void cSoftPlay::SetTmpCurrList(cPlayList *List) {
        if (currList && currListIsTmp)
                delete currList;
        currList=List;
};

void cSoftPlay::SaveList(cPlayList *List, const char* Name) {
	char filename[60];
	if (Name)
                sprintf(filename,"%s/%s.playlist",configDir,Name);
	else
                sprintf(filename,"%s/%s.playlist",configDir,List->GetName());

	printf("Save list as %s\n",filename);
	FILE *out=fopen(filename,"w");
	if (!out)
		return;
	List->Save(out);
	fclose(out);
};

cPlayList *cSoftPlay::OpenList(const char *Name) {
        char filename[120];
        char line[500];
       
        const char *extension=rindex(Name,'.');
        if ( !extension || strcmp(".playlist",extension ) ) 
                extension=".playlist";
        else extension=NULL;
                
                        
	if (extension)
                snprintf(filename,sizeof(filename),
			"%s/%s%s",configDir,Name,extension);
        else
                snprintf(filename,sizeof(filename),
                                "%s/%s",configDir,Name);
  
	printf("Read list from %s\n",filename);
	FILE *out=fopen(filename,"r");
        if (!out) {
                printf("could not open %s for reading!\n",filename);
                return NULL;
        };
        fgets(line,500,out);
        
        cPlayList *List=new cPlayList();
	List->Load(out,line);
	fclose(out);
        return List;
};

void cSoftPlay::ScanForPlaylists() {
        struct dirent **namelist;
        int n;
        //char Name[60];
        //char Title[60];


        n = scandir(configDir, &namelist, 0, alphasort);
        if (n<0) {
                printf("scandir error. Could not scan for playlists\n");
                return;
        };

        for (int i=0; i<n; i++) {
                if ( !strcmp("..",namelist[i]->d_name) ||
                     !strcmp(".",namelist[i]->d_name) ) {
                        PLUGDEB("ignore %s\n",namelist[i]->d_name);
                        continue;
                };

                char *extension=rindex(namelist[i]->d_name,'.');
                if ( !extension || strcmp(".playlist",extension) ) {
                        PLUGDEB("%s is not a playlist\n",namelist[i]->d_name);
                        continue;
                };

                PLUGDEB("found playlist %s!\n",namelist[i]->d_name);
                free(namelist[i]);	  
        }
        free(namelist);
};



VDRPLUGINCREATOR(cSoftPlay); // Don't touch this!

//------------------------------------------------------------------------
const int Divisor=0xfda9743d;
//const int32_t Divisor=0xfda97431;

int SimpleHash( char const* str) {
        // just used to fast identify strings. 
        // I guess this can be made much better.
        // FIXME buggy?
        if (!str)
                return 0;

        //printf("String: %s",str);
        int result=0;
        while ( *(str) ) {
                result=((result<<8)+*str) % Divisor;
                str++;
        };
        //printf(" Hash: 0x%x\n",result);
        return result;
};

void PrintCutDownString(char *str, const char *orig, int len) {
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

bool IsStream( const char * const Filename) {
        const char * const pos=Filename;
        printf("isstream %s: %d\n",pos,strncmp(pos,"http://",7));
        if ( !strncmp(pos,"http://",7) ) {
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

void stripTrailingSpaces(char *str) {
        char *pos=str+strlen(str);
        while ( pos>str && *pos == ' ')
                *pos--=0;
};

void stripTrailingNonLetters(char *str) {
        char *pos=str+strlen(str);
        //printf("start at '%c'(0x%x) ",*pos,*pos);
        while ( pos>str &&
                 !(*pos >= '!' && *pos <= '~' ) ) {
                //printf(" del '%c'(0x%x) ",*pos,*pos);
                *pos--=0;
        };
        //printf("\n");
};

cIndexIdx *FileIndex=NULL;


const char *ReadQuotedString(char *out, const char *Str) {
        int len;
	if ( !Str || !*Str)
		return NULL;

        // empty string case
        if ( !strncmp(Str,"\"\"",2) ) {
                out[0]=0;
                len=2;
        } else if ( sscanf(Str,"\"%" TO_STRING(STR_LENGTH) "[^\"]\"%n",
                                out,&len) == 0 ) {
                fprintf(stderr,"Could not parse quoted string at position '%s'. Ignoring.\n",Str);
                out[0]=0;
                return NULL;
        };
        return &Str[len];
};


