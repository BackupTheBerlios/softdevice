/*
 * Media Player plugin for VDR
 *
 * Copyright (C) 2005 Martin Wache
 *
 * This code is distributed under the terms and conditions of the
 * GNU GENERAL PUBLIC LICENSE. See the file COPYING for details.
 *
 * $Id: PlayList.c,v 1.11 2005/08/15 09:07:30 wachm Exp $
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


void cPlayListItem::BuildIdx(sItemIdx *ShuffleIdx) {
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
                sItemIdx *ShuffleIdx) 
                : cPlayListItem(Filename,(Name ? Name : tr("Playlist 1"))) {
        first=NULL;
        last=NULL;
        options.shuffle=false;
        options.autoRepeat=false;
        
        if (!ShuffleIdx) {
	        shuffleIdx=new sItemIdx;
                shuffleIdx->currShuffleIdx=-1;
                shuffleIdx->nIdx=0;
                shuffleIdx->Idx=new sIdx[MAX_ITEMS];
                memset(shuffleIdx->Idx,0,sizeof(sIdx[MAX_ITEMS]));
                shuffleIdx->nAlbum=0;
                shuffleIdx->Album=new sIdx[MAX_ITEMS/10];
                memset(shuffleIdx->Album,0,sizeof(sIdx[MAX_ITEMS/10]));
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
                delete shuffleIdx->Idx;
                delete shuffleIdx;
                shuffleIdx=NULL;
        };     
};

bool cPlayList::RemoveItem(cPlayListItem *Item) {
        cPlayList *playList=dynamic_cast <cPlayList*>(Item);
        if (playList) {
                for (int i=0; i<shuffleIdx->nAlbum; i++) 
                        if (shuffleIdx->Album[i].Item==Item) {
                                shuffleIdx->Album[i].Album->
                                        RemoveItemFromList(Item);
                                memmove(&shuffleIdx->Album[i],
                                                &shuffleIdx->Album[i+1],
                                                sizeof(shuffleIdx->Album[i])*
                                                (shuffleIdx->nAlbum-i));
                                shuffleIdx->nAlbum--;
                                return true;
                        };

        } else {
                for (int i=0; i<shuffleIdx->nIdx; i++) 
                        if (shuffleIdx->Idx[i].Item==Item) {
                                shuffleIdx->Idx[i].Album->
                                        RemoveItemFromList(Item);
                                memmove(&shuffleIdx->Idx[i],
                                                &shuffleIdx->Idx[i+1],
                                                sizeof(shuffleIdx->Idx[i])*
                                                (shuffleIdx->nIdx-i));
                                shuffleIdx->nIdx--;
                                if (i<shuffleIdx->currShuffleIdx)
                                        shuffleIdx->currShuffleIdx--;
                                return true;
                        };
        };
        return true;
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

cPlayList *cPlayList::GetItemAlbum(cPlayListItem *Item) {
        LISTDEB("GetItemAlbum %p, first %p last %p\n",Item,first,last);
	cPlayListItem *listItem=first;
	cPlayList *List=NULL;
        cPlayList *Ret=NULL;
	// find Item in list or in sublist
	while (listItem && listItem!=Item) {
		// if listItem is a list ask list if it owns item 
                if ( (List=dynamic_cast <cPlayList*>(listItem)) 
			&& (Ret=List->GetItemAlbum(Item)) )
			// return sublist if it owns Item
			return Ret;
                listItem=listItem->GetNext();
	};
	// item is in this list
	if (listItem == Item)
		return this;
	
        return NULL;
};

void cPlayList::AddItemAtEnd(cPlayListItem *Item) {
        if (!last) {
                Item->InsertSelfIntoList(NULL,NULL);
                first = last = Item;
        } else {
                last->next = Item;
                Item->previous = last;
                Item->next = NULL;
                last = Item;
        };
        if ( dynamic_cast <cPlayList*>(Item) ) {
                shuffleIdx->Album[shuffleIdx->nAlbum].Album=this;
                shuffleIdx->Album[shuffleIdx->nAlbum].Item=Item;
                shuffleIdx->Album[shuffleIdx->nAlbum].Hash=
                        SimpleHash(Item->GetFilename());
                LISTDEB("Hash %x Filename %s\n",shuffleIdx->Album[shuffleIdx->nAlbum].Hash,Item->GetFilename());
                shuffleIdx->nAlbum++;
        } else {
                shuffleIdx->Idx[shuffleIdx->nIdx].Album=this;
                shuffleIdx->Idx[shuffleIdx->nIdx].Item=Item;
                shuffleIdx->Idx[shuffleIdx->nIdx].Hash=
                        SimpleHash(Item->GetFilename());
                LISTDEB("Hash %x Filename %s\n",shuffleIdx->Idx[shuffleIdx->nIdx].Hash,Item->GetFilename());
                shuffleIdx->nIdx++;
        }
};                    

void cPlayList::BuildIdx(sItemIdx *ShuffleIdx) {
        shuffleIdx=ShuffleIdx;
	cPlayListItem *currItem=NULL;
	if (shuffleIdx->currShuffleIdx!=-1) {
		// while playback rember current item, if this has been
		// deleted find the next not deleted item
		while ( ! (currItem=GetShuffledItemByIndex(shuffleIdx->currShuffleIdx) ) && 
				shuffleIdx->currShuffleIdx<shuffleIdx->nIdx)
			shuffleIdx->currShuffleIdx++;
	};
			
        cPlayListItem *Item=first;

        while (Item) {
                LISTDEB("Item %p Item->GetNext() %p \n",Item,Item->GetNext());
                Item->BuildIdx(shuffleIdx);
                Item=Item->GetNext();
        };
/*	
	if (currItem) {
		// FIXME if currItem has been deleted do I have to subtract one?
		currItemIdx=Item->GetIdx();
		for (int i=0; i<nItems; i++) 
			if ( currItemIdx==shuffleIdx[i] ) {
				currShuffleIdx=i;
				break;
			};
	};
        */
};

cPlayListItem *cPlayList::GetItemByFilename(const char *Filename) {
	int32_t hash=SimpleHash(Filename);
        
        for (int i=0; i<shuffleIdx->nIdx;i++) 
                if ( hash==shuffleIdx->Idx[i].Hash && shuffleIdx->Idx[i].Item
                        && strcmp(Filename,shuffleIdx->Idx[i].Item->GetFilename()) ==0)
                        return shuffleIdx->Idx[i].Item;
        return NULL;
};

int cPlayList::GetIndexByItem(const cPlayListItem *Item) {
        for (int i=0; i<shuffleIdx->nIdx;i++) 
                if ( Item==shuffleIdx->Idx[i].Item )
                        return i;
        return -1;
};

cPlayListItem *cPlayList::GetAlbumByFilename(const char *Filename) {
	int32_t hash=SimpleHash(Filename);
        
        for (int i=0; i<shuffleIdx->nAlbum;i++) 
                if ( hash==shuffleIdx->Album[i].Hash && shuffleIdx->Album[i].Item
                        && strcmp(Filename,shuffleIdx->Album[i].Item->GetFilename())==0)

                        return shuffleIdx->Album[i].Item;
        return NULL;
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
                if ( namelist[i]->d_type == 0 || namelist[i]->d_type == DT_LNK ) {
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

char * cPlayList::NextFile() {
        LISTDEB("NextFile currShuffleIdx %d\n",
                        shuffleIdx->currShuffleIdx);
        int saveCurrShuffleIdx=shuffleIdx->currShuffleIdx;

        do {
		++shuffleIdx->currShuffleIdx;
                if ( options.autoRepeat && 
                     shuffleIdx->currShuffleIdx >= shuffleIdx->nIdx ) {
                        shuffleIdx->currShuffleIdx=-1;
                        if (options.shuffle)
                                Shuffle();
                        shuffleIdx->currShuffleIdx=0;
                };
        } while ( shuffleIdx->currShuffleIdx<shuffleIdx->nIdx 
                        && !shuffleIdx->Idx[shuffleIdx->currShuffleIdx].Item );
	
        LISTDEB("NextFile currShuffleIdx %d nIdx %d\n",
                        shuffleIdx->currShuffleIdx,shuffleIdx->nIdx);

        if ( shuffleIdx->currShuffleIdx > shuffleIdx->nIdx )
                shuffleIdx->currShuffleIdx=shuffleIdx->nIdx;

	if ( shuffleIdx->Idx[shuffleIdx->currShuffleIdx].Item )
		return shuffleIdx->Idx[shuffleIdx->currShuffleIdx].Item->GetFilename();

        shuffleIdx->currShuffleIdx=saveCurrShuffleIdx;
	return NULL;
};

char * cPlayList::PrevFile() {
        LISTDEB("PrevFile currShuffleIdx %d\n",
                        shuffleIdx->currShuffleIdx);
        do {
		--shuffleIdx->currShuffleIdx;
                if (options.autoRepeat && shuffleIdx->currShuffleIdx<0)
                        shuffleIdx->currShuffleIdx=shuffleIdx->nIdx-1;

        } while ( shuffleIdx->currShuffleIdx > 0
                        &&!shuffleIdx->Idx[shuffleIdx->currShuffleIdx].Item );
	
        LISTDEB("PrevFile currShuffleIdx %d\n",shuffleIdx->currShuffleIdx);
 
        if ( shuffleIdx->currShuffleIdx < 0 )
                shuffleIdx->currShuffleIdx=0;

	if ( shuffleIdx->Idx[shuffleIdx->currShuffleIdx].Item)
		return shuffleIdx->Idx[shuffleIdx->currShuffleIdx].Item->GetFilename();
	return NULL;
};

char * cPlayList::NextAlbumFile() {
        cPlayList *currAlbum=shuffleIdx->Idx[shuffleIdx->currShuffleIdx].Album;
        LISTDEB("NextAlbumFile currShuIdx %d currAlbum %p\n",
                        shuffleIdx->currShuffleIdx,currAlbum);
        int saveCurrShuffleIdx=shuffleIdx->currShuffleIdx;

        do {
		++shuffleIdx->currShuffleIdx;
                if ( options.autoRepeat && 
                     shuffleIdx->currShuffleIdx >= shuffleIdx->nIdx ) {
                        shuffleIdx->currShuffleIdx=-1;
                        if (options.shuffle)
                                Shuffle();
                        shuffleIdx->currShuffleIdx=0;
                };
        } while ( shuffleIdx->currShuffleIdx<shuffleIdx->nIdx 
                        && ( !shuffleIdx->Idx[shuffleIdx->currShuffleIdx].Item 
                    || currAlbum==shuffleIdx->Idx[
                    shuffleIdx->currShuffleIdx].Album ) );
	
        LISTDEB("NextAlbumFile currShuffleIdx %d \n",
                        shuffleIdx->currShuffleIdx);
        
        if ( shuffleIdx->currShuffleIdx > shuffleIdx->nIdx )
                shuffleIdx->currShuffleIdx=shuffleIdx->nIdx;
        
	if (shuffleIdx->Idx[shuffleIdx->currShuffleIdx].Item)
		return shuffleIdx->Idx[shuffleIdx->currShuffleIdx].Item->GetFilename();

        shuffleIdx->currShuffleIdx=saveCurrShuffleIdx;
	return NULL;
};

char * cPlayList::PrevAlbumFile() {
        LISTDEB("PrevAlbumFile currShuffleIdx %d\n",
                        shuffleIdx->currShuffleIdx);
        cPlayList *currAlbum=shuffleIdx->Idx[shuffleIdx->currShuffleIdx].Album;
        do {
	        --shuffleIdx->currShuffleIdx;
                if (options.autoRepeat && shuffleIdx->currShuffleIdx<0)
                        shuffleIdx->currShuffleIdx=shuffleIdx->nIdx-1;
        } while ( shuffleIdx->currShuffleIdx > 0  
                  && ( !shuffleIdx->Idx[shuffleIdx->currShuffleIdx].Item 
                    || currAlbum==shuffleIdx->Idx[
                    shuffleIdx->currShuffleIdx].Album ) );
	
        LISTDEB("PrevFile currShuffleIdx %d\n",shuffleIdx->currShuffleIdx);

        if (shuffleIdx->currShuffleIdx < 0 )
                shuffleIdx->currShuffleIdx=0;
        
	if (shuffleIdx->Idx[shuffleIdx->currShuffleIdx].Item)
		return shuffleIdx->Idx[shuffleIdx->currShuffleIdx].Item->GetFilename();
	return NULL;
};

char * cPlayList::CurrFile() {
        LISTDEB("CurrFile shuffleIdx->currShuffleIdx %d\n",
                        shuffleIdx->currShuffleIdx);
	while (!shuffleIdx->Idx[shuffleIdx->currShuffleIdx].Item
                        && shuffleIdx->currShuffleIdx<shuffleIdx->nIdx )
                shuffleIdx->currShuffleIdx++;
        LISTDEB("CurrFile currShuffleIdx %d\n",shuffleIdx->currShuffleIdx);

	if (shuffleIdx->Idx[shuffleIdx->currShuffleIdx].Item)
		return shuffleIdx->Idx[shuffleIdx->currShuffleIdx].Item->GetFilename();
	return NULL;
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

int cPlayList::GetNoItems() {
        int count=0;
        cPlayListItem *Item=first;
        while ( Item ) {
                count++;
                Item=Item->GetNext();
        };
        return count;
};

int cPlayList::GetNoItemsRecursive() {
        int count=0;
        cPlayListItem *Item=first;
        cPlayList *List;
        while ( Item ) {
                // count items of lists instead of list
                if ( (List=dynamic_cast <cPlayList*>(Item)) )
                        count+=List->GetNoItemsRecursive();
                else count++;
                Item=Item->GetNext();
        };
        return count;
};
  
void cPlayList::PrepareForPlayback() {
	LISTDEB("PrepareForPlayback\n");
	shuffleIdx->currShuffleIdx=-1;

        BuildIdx();
        if (options.shuffle)
                Shuffle();
};

void cPlayList::Shuffle() {
        LISTDEB("Shuffle playlist nIdx %d currShuffleIdx %d\n",
                        shuffleIdx->nIdx,shuffleIdx->currShuffleIdx);

        sIdx index;
        for (int i=0; i<2*shuffleIdx->nIdx ; i++) {
		//FIXME - I guess the ranges are not completly correct
                int xchange1=(int)( (float)(random())*(float)(shuffleIdx->nIdx-1-shuffleIdx->currShuffleIdx)/(float)(RAND_MAX))
			+shuffleIdx->currShuffleIdx+1;
                int xchange2=(int)( (float)(random())*(float)(shuffleIdx->nIdx-1-shuffleIdx->currShuffleIdx)/(float)(RAND_MAX))
			+shuffleIdx->currShuffleIdx+1;
                if (xchange1 >=shuffleIdx->nIdx)
                        LISTDEB("Martin, depp!! %d\n",xchange1);
                if (xchange2 >=shuffleIdx->nIdx)
                        LISTDEB("Martin, depp!! %d\n",xchange2);
                
		LISTDEB("Shuffle %4d(%4d) - %4d(%4d) \n",
                   xchange1,shuffleIdx[xchange1],xchange2,shuffleIdx[xchange2]);
                index=shuffleIdx->Idx[xchange1];
                shuffleIdx->Idx[xchange1]=shuffleIdx->Idx[xchange2];
                shuffleIdx->Idx[xchange2]=index;
        };
	shuffleIdx->reshuffled=true;
/*
	for (int i=0; i<nItems ; i++) 
		LISTDEB("shuffleIdx[%d]: %d\n",i,shuffleIdx->Idx[i]);       
*/
};

void cPlayList::SetOptions( sPlayListOptions &Options) {

        if (Options.shuffle != options.shuffle 
                        && Options.shuffle)
                Shuffle();

        options=Options;
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

