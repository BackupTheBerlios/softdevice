/*
 * Media Player plugin for VDR
 *
 * Copyright (C) 2005 Martin Wache
 *
 * This code is distributed under the terms and conditions of the
 * GNU GENERAL PUBLIC LICENSE. See the file COPYING for details.
 *
 * $Id: PlayList.c,v 1.12 2005/08/15 13:13:14 wachm Exp $
 */
#include "softplay.h"
#include "PlayList.h"
#include <string.h>
#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>

#include "vdr/player.h"

#define LISTDEB(out...) {printf("LISTDEB: ");printf(out);}

#ifndef LISTDEB
#define LISTDEB(out...)
#endif

// ---cItemIdx-------------------------------------------------------------

cItemIdx::cItemIdx(cPlayList *Owner ) {
        owner = Owner;
	currShuffleIdx=-1;
	nIdx=0;
	Idx=new sIdx[MAX_ITEMS];
	memset(Idx,0,sizeof(sIdx[MAX_ITEMS]));
	nAlbum=0;
	Album=new sIdx[MAX_ITEMS/10];
	memset(Album,0,sizeof(sIdx[MAX_ITEMS/10]));
};

cItemIdx::~cItemIdx() {
        delete Idx;
        delete Album;
};

cPlayList *cItemIdx::RemoveItem(cPlayListItem *Item) {
        cPlayList *album;
        if ( dynamic_cast <cPlayList*>(Item) ) {
                // remove a album 
                for (int i=0; i<nAlbum; i++) 
                        if (Album[i].Item==Item) {
                                album=Album[i].Album;
                                memmove(&Album[i],&Album[i+1],
                                        sizeof(Album[i])*(nAlbum-i));
                                nAlbum--;
                                return album;
                        };

        } else {
                // remove a track
                for (int i=0; i<nIdx; i++) 
                        if (Idx[i].Item==Item) {
                                album=Idx[i].Album;
                                memmove(&Idx[i],&Idx[i+1],
                                        sizeof(Idx[i])*(nIdx-i));
                                nIdx--;
                                if (i<currShuffleIdx)
                                        currShuffleIdx--;
                                return album;
                        };
        };
        return NULL;
};     

void cItemIdx::AddItemAtEnd(cPlayList *album, cPlayListItem *Item) {
        if ( dynamic_cast <cPlayList*>(Item) ) {
                Album[nAlbum].Album=album;
                Album[nAlbum].Item=Item;
                Album[nAlbum].Hash=SimpleHash(Item->GetFilename());
                LISTDEB("Hash %x Filename %s\n",Album[nAlbum].Hash,Item->GetFilename());
                nAlbum++;
        } else {
                Idx[nIdx].Album=album;
                Idx[nIdx].Item=Item;
                Idx[nIdx].Hash=SimpleHash(Item->GetFilename());
                LISTDEB("Hash %x Filename %s\n",
                                Idx[nIdx].Hash,Item->GetFilename());
                nIdx++;
        }
}; 

cPlayListItem *cItemIdx::GetTrackByFilename(const char *Filename) {
	int32_t hash=SimpleHash(Filename);
        
        for ( int i=0; i < nIdx; i++ ) 
                if ( hash==Idx[i].Hash && Idx[i].Item
                        && strcmp(Filename,Idx[i].Item->GetFilename()) ==0)
                        return Idx[i].Item;
        return NULL;
};

cPlayListItem *cItemIdx::GetAlbumByFilename(const char *Filename) {
	int32_t hash=SimpleHash(Filename);
        
        for ( int i=0; i < nAlbum; i++ ) 
                if ( hash==Album[i].Hash && Album[i].Item
                        && strcmp(Filename,Album[i].Item->GetFilename())==0)
                        return Album[i].Item;
        return NULL;
};

int cItemIdx::GetIndexByItem(const cPlayListItem *Item) {
        for ( int i=0; i < nIdx; i++ ) 
                if ( Item == Idx[i].Item )
                        return i;
        return -1;
};

const char * cItemIdx::NextFile() {
        LISTDEB("NextFile currShuffleIdx %d\n",currShuffleIdx);
        int saveCurrShuffleIdx=currShuffleIdx;

        do {
		++currShuffleIdx;
                if ( owner->options.autoRepeat && currShuffleIdx >= nIdx ) {
                        currShuffleIdx=-1;
                        if (owner->options.shuffle)
                                Shuffle();
                        currShuffleIdx=0;
                };
        } while ( currShuffleIdx < nIdx 
                        && !Idx[currShuffleIdx].Item );
	
        LISTDEB("NextFile currShuffleIdx %d nIdx %d\n",currShuffleIdx,nIdx);

        if ( currShuffleIdx > nIdx )
                currShuffleIdx=nIdx;

	if ( Idx[currShuffleIdx].Item )
		return Idx[currShuffleIdx].Item->GetFilename();

        currShuffleIdx=saveCurrShuffleIdx;
	return NULL;
};

const char * cItemIdx::PrevFile() {
        LISTDEB("PrevFile currShuffleIdx %d\n",currShuffleIdx);
        do {
		--currShuffleIdx;
                if (owner->options.autoRepeat && currShuffleIdx<0)
                        currShuffleIdx=nIdx-1;

        } while ( currShuffleIdx > 0
                        && !Idx[currShuffleIdx].Item );
	
        LISTDEB("PrevFile currShuffleIdx %d\n",currShuffleIdx);
 
        if ( currShuffleIdx < 0 )
                currShuffleIdx=0;

	if ( Idx[ currShuffleIdx ].Item)
		return Idx[ currShuffleIdx ].Item->GetFilename();
	return NULL;
};

const char * cItemIdx::NextAlbumFile() {
        cPlayList *currAlbum=Idx[currShuffleIdx].Album;
        LISTDEB("NextAlbumFile currShuIdx %d currAlbum %p\n",
                        currShuffleIdx,currAlbum);
        int saveCurrShuffleIdx=currShuffleIdx;

        do {
		++currShuffleIdx;
                if ( owner->options.autoRepeat && 
                     currShuffleIdx >= nIdx ) {
                        if (owner->options.shuffle)
                                Shuffle();
                        currShuffleIdx=0;
                };
        } while ( currShuffleIdx < nIdx 
                 && ( !Idx[currShuffleIdx].Item 
                    || currAlbum==Idx[currShuffleIdx].Album ) );
	
        LISTDEB("NextAlbumFile currShuffleIdx %d \n",currShuffleIdx);
        
        if ( currShuffleIdx > nIdx )
                currShuffleIdx=nIdx;
        
	if ( Idx[ currShuffleIdx ].Item )
		return Idx[ currShuffleIdx ].Item->GetFilename();

        currShuffleIdx=saveCurrShuffleIdx;
	return NULL;
};

const char * cItemIdx::PrevAlbumFile() {
        LISTDEB("PrevAlbumFile currShuffleIdx %d\n",currShuffleIdx);
        cPlayList *currAlbum=Idx[currShuffleIdx].Album;
        do {
	        --currShuffleIdx;
                if ( owner->options.autoRepeat && currShuffleIdx < 0 )
                        currShuffleIdx = nIdx - 1;
        } while ( currShuffleIdx > 0  
                  && ( !Idx[currShuffleIdx].Item 
                    || currAlbum == Idx[currShuffleIdx].Album ) );
	
        LISTDEB("PrevFile currShuffleIdx %d\n",currShuffleIdx);

        if (currShuffleIdx < 0 )
                currShuffleIdx=0;
        
	if (Idx[currShuffleIdx].Item)
		return Idx[currShuffleIdx].Item->GetFilename();
	return NULL;
};

const char * cItemIdx::CurrFile() {
        LISTDEB("CurrFile currShuffleIdx %d\n",currShuffleIdx);
        
	while ( !Idx[currShuffleIdx].Item && currShuffleIdx < nIdx )
                currShuffleIdx++;
        
        LISTDEB("CurrFile currShuffleIdx %d\n",currShuffleIdx);

	if (Idx[currShuffleIdx].Item)
		return Idx[currShuffleIdx].Item->GetFilename();
	return NULL;
};

void cItemIdx::Shuffle() {
        LISTDEB("Shuffle playlist nIdx %d currShuffleIdx %d\n",
                        nIdx,currShuffleIdx);

        sIdx index;
        for (int i=0; i<2*nIdx ; i++) {
		//FIXME - I guess the ranges are not completly correct
                int xchange1=(int)( (float)(random())*(float)(nIdx-1-currShuffleIdx)/(float)(RAND_MAX))
			+currShuffleIdx+1;
                int xchange2=(int)( (float)(random())*(float)(nIdx-1-currShuffleIdx)/(float)(RAND_MAX))
			+currShuffleIdx+1;
                if (xchange1 >=nIdx)
                        LISTDEB("Martin, depp!! %d\n",xchange1);
                if (xchange2 >=nIdx)
                        LISTDEB("Martin, depp!! %d\n",xchange2);
                
		LISTDEB("Shuffle %4d(%4d) - %4d(%4d) \n",
                   xchange1,Idx[xchange1],xchange2,Idx[xchange2]);
                index=Idx[xchange1];
                Idx[xchange1]=Idx[xchange2];
                Idx[xchange2]=index;
        };
	reshuffled=true;
/*
	for (int i=0; i<nItems ; i++) 
		LISTDEB("shuffleIdx[%d]: %d\n",i,shuffleIdx->Idx[i]);       
*/
};


// ---cPlayListItem--------------------------------------------------------
cPlayListItem::cPlayListItem(const char *Filename, const char *Name) {
        if (Name) {
                SetName(Name);
        } else name[0]=0;

        if (Filename) {
                SetFilename(Filename);
        } else filename[0]=0;
        next=previous=NULL;
};

cPlayListItem::~cPlayListItem() {
};

void cPlayListItem::InsertSelfIntoList(cPlayListItem *Next,
                cPlayListItem *Previous) {
        next=Next;
        if (next) 
                next->previous=this;
        previous=Previous;
        if (previous)
                previous->next=this;
};


void cPlayListItem::BuildIdx(cItemIdx *ShuffleIdx) {
};

const char *cPlayListItem::ParseTypeFilenameName(const char *pos, 
                char *Type, char *Filename, char *Name) {
        int len;
        // read entry type
        if ( sscanf(pos,"%20[^:]:%n",Type,&len) == 0 ) {
                LISTDEB("Ignoring invalid line in list file: \"%s\"!\n",pos);
                return NULL;
        };
        pos = &pos[len];

        //skip whitespaces
        while (pos[0] && pos[0]==' ')
                pos++;
        // read file name (should be always present)
        if ( !strncmp(pos,"\"\",",3) ) {
                filename[0]=0;
                len=3;
        } else if ( sscanf(pos,"\"%" TO_STRING(STR_LENGTH) 
                                "[^\"]\",%n",Filename,&len) == 0 ) {
                LISTDEB("Could not parse filename at pos \"%s\". Ignoring.\n",pos);
                return NULL;
        };
        pos = &pos[len];

        // skip whitespaces
        while (pos[0] && pos[0]==' ')
                pos++;

        // read name (should be always present)
        if ( !strncmp(pos,"\"\",",3) ) {
                name[0]=0;
                len=3;
        } else if ( sscanf(pos,"\"%" TO_STRING(SHORT_STR) 
                                "[^\"]\",%n",Name,&len) == 0 ) {
                LISTDEB("Could not parse name at pos \"%s\". Ignoring.\n",pos);
                return NULL;
        };
        pos = &pos[len];

        return pos;
};
 
//-----cPlayListRegular-----------------------------------------------------

void cPlayListRegular::Save(FILE *out) {
	fprintf(out,"File: ");
        fprintf(out,"\"%s\",",GetFilename());
        fprintf(out,"\"%s\",",GetName());
        fprintf(out,"\n");
};

// -----cPlayList------------------------------------------------------------
cPlayList::cPlayList(const char *Filename, const char *Name,
                cItemIdx *ShuffleIdx) 
                : cPlayListItem(Filename,(Name ? Name : tr("Playlist 1"))) {
        first=NULL;
        last=NULL;
        options.shuffle=false;
        options.autoRepeat=false;
        
        if (!ShuffleIdx) {
	        shuffleIdx=new cItemIdx(this);
                shuffleIdxOwner=true;
        } else {
                shuffleIdxOwner=false;
                shuffleIdx=ShuffleIdx;
        };
};

cPlayList::~cPlayList() {
        while (first)
                RemoveItem(first);
        if (shuffleIdxOwner) {
                delete shuffleIdx;
                shuffleIdx=NULL;
        };     
};

bool cPlayList::RemoveItemFromList(cPlayListItem *Item) {
        LISTDEB("RemoveItemFromList %p, first %p last %p\n",Item,first,last);
	
        // is it the first or the last item?
        if (first && Item == first)
                first = first->next;
        if (last && Item == last) 
                last = last->previous;
	// remove from list
        if (Item->next) 
                Item->next->previous=Item->previous;
        if (Item->previous) 
                Item->previous->next=Item->next;
        Item->previous=NULL;
        Item->next=NULL;
        delete Item;
	return true;
};

void cPlayList::AddItemAtEndToList(cPlayListItem *Item) {
        if (!last) {
                Item->InsertSelfIntoList(NULL,NULL);
                first = last = Item;
        } else {
                last->next = Item;
                Item->previous = last;
                Item->next = NULL;
                last = Item;
        };
};

void cPlayList::AddItemAtEnd(cPlayListItem *Item) {
        AddItemAtEndToList(Item);
        if (shuffleIdx)
                shuffleIdx->AddItemAtEnd(this,Item);        
};                    

bool cPlayList::RemoveItem(cPlayListItem *Item) { 
        cPlayList *album=NULL;
        if (shuffleIdx )
                album=shuffleIdx->RemoveItem(Item);
        if (album)
                return album->RemoveItemFromList(Item);
        return false; 
};

void cPlayList::BuildIdx(cItemIdx *ShuffleIdx) {
};

bool cPlayList::AddFile(char * filename, char *title) {
        LISTDEB("AddFile %s\n",filename);
        cPlayListItem *Item= new cPlayListRegular(filename,title);
        LISTDEB("Item created %p\n",Item);
        AddItemAtEnd(Item);
        LISTDEB("Item added first %p last %p\n",first,last);
        
        return true;
};

bool cPlayList::AddDir(char * dirname, char * title, bool recursive) {
        LISTDEB("AddDir %s\n",dirname);
        cPlayList *Item= new cPlayList(dirname,title,shuffleIdx);
        Item->ScanDir(dirname,recursive);
        AddItemAtEnd(Item);

        return true;
};

bool cPlayList::AddM3U(char * Filename, char * title) {
        LISTDEB("AddM3U %s\n",Filename);
        cPlayList *Item= new cPlayList(Filename,title,shuffleIdx);
        Item->LoadM3U(Filename,title);
        AddItemAtEnd(Item);

        return true;
};

cPlayListItem *cPlayList::GetItem(int No) {
        int count=0;
        cPlayListItem *Item=first;
        while ( Item && count!=No ) {
                count++;
                Item=Item->GetNext();
        };
        return Item;
};
  
void cPlayList::PrepareForPlayback() {
	LISTDEB("PrepareForPlayback\n");
	shuffleIdx->currShuffleIdx=-1;

        BuildIdx();
        if (options.shuffle)
                shuffleIdx->Shuffle();
};

void cPlayList::SetOptions( sPlayListOptions &Options) {

        if (Options.shuffle != options.shuffle 
                        && Options.shuffle)
                shuffleIdx->Shuffle();

        options=Options;
};

bool cPlayList::ScanDir(char * dirname, bool recursive) {
        struct dirent **namelist;
        int n;
        char Name[STR_LENGTH];

        LISTDEB("ScanDir %s recursive %d\n",dirname,recursive);
        
        n = scandir(dirname, &namelist, 0, alphasort);
        if (n<0) {
                printf("scandir error\n");
                return false;
        };

        for (int i=0; i<n; i++) {
                if ( !strcmp("..",namelist[i]->d_name) ||
                     !strcmp(".",namelist[i]->d_name) ) {
                        LISTDEB("ignore %s\n",namelist[i]->d_name);
                        continue;
                };

                bool ret=true;
                snprintf(Name,STR_LENGTH,"%s/%s",
                                dirname,namelist[i]->d_name);
                Name[STR_LENGTH-1]=0;

                // check type (non ext2/3 and symlinks)
                if ( namelist[i]->d_type == 0 || 
                                namelist[i]->d_type == DT_LNK ) {
                        struct stat stbuf;
                        if ( !stat(Name,&stbuf) ) {
                                if ( S_ISDIR(stbuf.st_mode) ) 
                                        namelist[i]->d_type = DT_DIR;
                                else if ( S_ISREG(stbuf.st_mode) )
                                        namelist[i]->d_type = DT_REG;
                        };
                };

                if (namelist[i]->d_type == DT_DIR && recursive )  
                        ret=AddDir(Name,namelist[i]->d_name,recursive);
                else  ret=AddFile(Name,namelist[i]->d_name);

                free(namelist[i]);	  
        }
        free(namelist);
        return true;
};


void cPlayList::Save(FILE *out) {
        cPlayListItem *Item;
        
        fprintf(out,"Start_List: ");
        fprintf(out,"\"%s\",",filename);
        fprintf(out,"\"%s\",",name);
        fprintf(out,"\n");
        
        Item=first;
        while (Item) {
                Item->Save(out);
                Item=Item->next;
        };

        fprintf(out,"End_List: ");
        fprintf(out,"\"%s\",",filename);
        fprintf(out,"\"%s\",",name);
        fprintf(out,"\n");
};
        
      
int cPlayList::Load(FILE *in, const char *StartLine) {
        char line[500];
        const char *pos;
        char Type[STR_LENGTH];
        char Filename[STR_LENGTH];
        char Name[SHORT_STR];
       
        if (StartLine) {
                if ( !(pos=ParseTypeFilenameName(StartLine,Type,Filename,Name)) ) {
                        LISTDEB("Could not parse Startline. Returning. \n");
                        return -1;
                };

                if ( strcmp(Type,"Start_List")!=0 ) {
                        LISTDEB("Startline not a List startline! Returning.\n");
                        return -1;
                };
                
                SetName(Name);
                SetFilename(Filename);
        };              
                
        while( !feof(in) && fgets(line,500,in)) {
                chomp(line);

                LISTDEB("read line %s \n",line); 

                if ( !(pos=ParseTypeFilenameName(line,Type,Filename,Name)) )
                        continue;
                       
                LISTDEB("filename \"%s\" name \"%s\" \n",Filename,Name); 
                if ( !strcmp(Type,"Start_List") ) {
                        LISTDEB("Found start of list\n");
                        cPlayList *Item = new cPlayList(Filename,Name,shuffleIdx);
                        if ( Item->Load(in,line)<0 )
                                return -1;

                        AddItemAtEnd(Item);
                } else if ( !strcmp(Type,"End_List") ) {
                        LISTDEB("Found end of list \n");
                        if ( strcmp(Filename,GetFilename()) ||
                                        strcmp(Name,GetName()) ) {
                                LISTDEB("End of list doesn't match! Returning!\n");
                                return -1; 
                        };
                        return 0;
                } else if ( !strcmp(Type,"File") ) {
                        LISTDEB("Found file \n");
                        cPlayListItem *Item = new cPlayListRegular(Filename,Name);
                        Item->ParseSaveLine(line);
                        AddItemAtEnd(Item);
                } else {
                        LISTDEB("Unknown type \"%s\"\n",Type);
                };
        };
        return 0;
};

int cPlayList::LoadM3U(const char *Filename, const char *Name) {
        FILE *list;
        char line[500];
        char dir[STR_LENGTH];
        LISTDEB("LoadM3U: %s\n",Filename);
        
        list = fopen( Filename, "r");
        
        if (!list) {
                LISTDEB("Could not open .m3u file!\n");
                return -1;
        };
        
        SetName(Name);
        SetFilename(Filename);
        char *ncopy=rindex(Filename,'/');
        int count=(int) (ncopy-Filename);
        if ( ncopy && count > STR_LENGTH) {
                fprintf(stderr,"Could not extract directory!\n");
                return -1;
        };
        strncpy(dir,Filename,count);
        dir[count]=0;
        LISTDEB("dir: %s\n",dir);

        while ( !feof(list)  && fgets(line,500,list) ) {
                chomp(line);
                printf("read line \"%s\"\n",line);
                if (line[0]=='#')
                        continue;

                // poor man's windows to linux filename transformation
                char *pos=line;
                while ( (pos=index(pos,'\\')) )
                        *pos='/';
    
                ncopy=line;
                // don't copy leading "./"
                if (ncopy[0]=='.' && ncopy[1]=='/')
                        ncopy+=2;
                
                char ItemName[SHORT_STR];
                char ItemFile[STR_LENGTH];
                snprintf(ItemFile,STR_LENGTH,"%s/%s",dir,ncopy);

                // strip off all directories to extract the name
                ncopy = rindex(line,'/');
                if (!ncopy)
                        ncopy=line;
                else ncopy++;
                snprintf(ItemName,SHORT_STR,"%s",ncopy);
                printf("Name %s \n",ncopy);
                
                cPlayListItem *Item = new cPlayListRegular(ItemFile,ItemName);
                AddItemAtEnd(Item);
        };

        return 0;
};

