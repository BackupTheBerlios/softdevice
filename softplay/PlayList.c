/*
 * Media Player plugin for VDR
 *
 * Copyright (C) 2005 Martin Wache
 *
 * This code is distributed under the terms and conditions of the
 * GNU GENERAL PUBLIC LICENSE. See the file COPYING for details.
 *
 * $Id: PlayList.c,v 1.3 2005/05/16 19:07:54 wachm Exp $
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
cPlayListItem::cPlayListItem(char *Filename, char *Name) {
        if (Name) {
                strncpy(name,Name,SHORT_STR);
                name[SHORT_STR-1]=0;
        } else name[0]=0;

        if (Filename) {
                strncpy(filename,Filename,STR_LENGTH);
                filename[STR_LENGTH-1]=0;
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
        idx=ShuffleIdx->nIdx;
};

// ----cEditList------------------------------------------------------------
cEditList::cEditList(cPlayList * List) : cOsdMenu(List->ListName) {
        playList=List;
        cPlayListItem * Item=playList->first;
        while (Item) {
                LISTDEB("EditList add %s: %p\n",Item->GetName(),Item);
                Add(new cOsdItem(Item->GetName(),
                   osUnknown),false);
                Item=Item->GetNext();
        };
        SetHelp(NULL,"(Add Files)","Delete","Stop");
        lastActivity=time(NULL);
};

cEditList::~cEditList() {
};

eOSState cEditList::ProcessKey(eKeys Key) {
        eOSState state = cOsdMenu::ProcessKey(Key);
        if (state != osUnknown ) 
                return state;

        cPlayList *List;
        cPlayListItem *Item;
        switch (Key) {
                case kOk:
                        LISTDEB("Current %d GetItem %p\n",Current(),playList->GetItem(Current()));
                        Item=playList->GetItem(Current());
                        List=dynamic_cast<cPlayList*>(Item);
                        LISTDEB("Item %p\n",Item);

                        if (List) {
                                cEditList *Menu=new cEditList(List);
                                return AddSubMenu(Menu);
                        }  else {
                                // skip to current track
                                playList->shuffleIdx->currShuffleIdx = 
                                        playList->GetIndexByItem(Item);
                                state = PLAY_CURR_FILE;
                        };
                        break;
                case kBack:
                        state= osBack;
                        break;
                 case kBlue:
                        state= osEnd;
                        break;
                 case kYellow:
                        LISTDEB("Del current %d (idx: %d): %s\n",
                                        Current(),playList->GetItem(Current())->GetIdx(),
                                        playList->GetItem(Current())->GetName() );
                        playList->RemoveItem(
                                        playList->GetItem(Current()));
                        printf("Remove finished\n");
                        Del(Current());
                        Display();
                        state=osContinue;
                        break;

                default:    
                        break;
        }
        return state;
}

// ----cReplayList------------------------------------------------------------
cReplayList::cReplayList(cPlayList * List) : cOsdMenu(List->ListName) {
        playList=List;
        SetHelp(NULL,"(Add)","Delete","Stop");
        RebuildList();
        SetCurrent(Get(playList->shuffleIdx->currShuffleIdx));
        lastActivity=time(NULL);
};

cReplayList::~cReplayList() {
};

void cReplayList::RebuildList() {
        int nItems=playList->GetNoItemsRecursive();
        LISTDEB("RebuildList nItems %d nIdx %d \n",
                        nItems,playList->shuffleIdx->nIdx);

        cPlayListItem * Item;
        for (int i = 0 ; i<nItems; i++) {
		Item=playList->GetShuffledItemByIndex(i);
                if (!Item) {
                        printf("Error getting all files for list index %d shuffleIdx %d\n",
                                        i,playList->shuffleIdx[i]);
                        continue;
                };
                
		LISTDEB("Add item %d (%p)  %s\n",
                                i,Item,Item->GetName());
                Add(new cOsdItem(Item->GetName(),
                   osUnknown),false);
        };
        lastListItemCount = playList->GetNoItemsRecursive();
};

void cReplayList::UpdateStatus() {
        int current;
        int total;
        char Status[60];
        char Name[60];
        
        if (!cControl::Control() || 
                        !cControl::Control()->GetIndex(current,total))
                return;
        
        snprintf(Status,60,"Time %02d:%02d:%02d/%02d:%02d:%02d Title %d/%d",
                        current/3600,current/60%60,current%60,
                        total/3600,total/60%60,total%60,
                        playList->shuffleIdx->currShuffleIdx+1,playList->shuffleIdx->nIdx);
        SetStatus(Status);

        if (displayedCurrIdx != playList->shuffleIdx->currShuffleIdx){
                cPlayListItem *Item=playList->GetShuffledItemByIndex(
                                playList->shuffleIdx->currShuffleIdx);
                if (Item) {
                        PrintCutDownString(Name,Item->GetName(),30);
                        snprintf(Status,60,"%s: %s",playList->ListName,
                                                Name);
                        SetTitle(Status);
                        Display();
                        displayedCurrIdx=playList->shuffleIdx->currShuffleIdx;
                } else printf("Didn't get currShuffleIdx %d!\n",playList->shuffleIdx->currShuffleIdx);
        };
};

eOSState cReplayList::ProcessKey(eKeys Key) {
        eOSState state = cOsdMenu::ProcessKey(Key);
        if (Key!=kNone) 
                lastActivity=time(NULL);
        
        if (state != osUnknown ) 
                return state;

        if ( lastListItemCount != playList->GetNoItemsRecursive() ) {
                //playList->CleanShuffleIdx();
                Clear();
                RebuildList();
                Display();
        };
        
        if (Current() != playList->shuffleIdx->currShuffleIdx && 
                        time(NULL) - lastActivity > 120 ) {
                LISTDEB("SetCurrent current title %d  time %d lastActivity %d\n",
                                playList->shuffleIdx->currShuffleIdx,time(NULL),lastActivity);
                SetCurrent(Get(playList->shuffleIdx->currShuffleIdx));
                Display();
        };

        cPlayListItem *Item;
        switch (Key) {
                case kOk:
                        // skip to current track
                        playList->shuffleIdx->currShuffleIdx = Current();
                        state = PLAY_CURR_FILE;
                        // want to have automatic track change
                        lastActivity=time(NULL)-300;
                        break;
                case kBack:
                        state= osBack;
                        break;
                case kBlue:
                        state= osEnd;
                        break;
                case kYellow:
                        Item=playList->GetShuffledItemByIndex(Current());
                        if (!Item) {
                                printf("No current Item %d!\n",Current());
                                break;
                        };
                        LISTDEB("Del current %d (idx: %d): %s\n",
                                        Current(),Item->GetIdx(),
                                        Item->GetName() );
                        playList->RemoveItem(Item);
			//playList->CleanShuffleIdx();
			lastListItemCount = playList->GetNoItemsRecursive();
                        Del(Current());
                        Display();
                        state=osContinue;
                        break;

                default:    
                        break;
        }
        if (!HasSubMenu())
                UpdateStatus();
        return state;
}


// -----cPlayList------------------------------------------------------------
cPlayList::cPlayList(char *Filename, char *Name,sItemIdx *ShuffleIdx) 
                      : cPlayListItem(Filename,Name) {
        first=NULL;
        last=NULL;
        
        strncpy(ListName,"Playlist 1",STR_LENGTH);
        ListName[STR_LENGTH-1]=0;
        if (!ShuffleIdx) {
	        shuffleIdx=new sItemIdx;
                shuffleIdx->currShuffleIdx=-1;
                shuffleIdx->nIdx=0;
                shuffleIdx->Idx=new sIdx[MAX_ITEMS];
                memset(shuffleIdx->Idx,0,sizeof(shuffleIdx->Idx));
                shuffleIdx->nAlbum=0;
                shuffleIdx->Album=new sIdx[MAX_ITEMS/10];
                memset(shuffleIdx->Album,0,sizeof(shuffleIdx->Album));
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
                printf("Hash %x Filename %s\n",shuffleIdx->Album[shuffleIdx->nAlbum].Hash,Item->GetFilename());
                shuffleIdx->nAlbum++;
        } else {
                shuffleIdx->Idx[shuffleIdx->nIdx].Album=this;
                shuffleIdx->Idx[shuffleIdx->nIdx].Item=Item;
                shuffleIdx->Idx[shuffleIdx->nIdx].Hash=
                        SimpleHash(Item->GetFilename());
                printf("Hash %x Filename %s\n",shuffleIdx->Idx[shuffleIdx->nIdx].Hash,Item->GetFilename());
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

cPlayListItem *cPlayList::GetItemByIndex(int Index) {
        
        for (int i=0; i<shuffleIdx->nIdx;i++) 
                if ( shuffleIdx->Idx[i].Item 
                        && Index==shuffleIdx->Idx[i].Item->GetIdx() )
                        return shuffleIdx->Idx[i].Item;
        return NULL;
};

cPlayListItem *cPlayList::GetItemByName(const char *name) {
	int32_t hash=SimpleHash(name);
        
        for (int i=0; i<shuffleIdx->nIdx;i++) 
                if ( hash==shuffleIdx->Idx[i].Hash && shuffleIdx->Idx[i].Item
                        && strcmp(name,shuffleIdx->Idx[i].Item->GetFilename()) ==0)
                        return shuffleIdx->Idx[i].Item;
        return NULL;
};

int cPlayList::GetIndexByItem(const cPlayListItem *Item) {
        for (int i=0; i<shuffleIdx->nIdx;i++) 
                if ( Item==shuffleIdx->Idx[i].Item )
                        return i;
        return -1;
};

cPlayListItem *cPlayList::GetAlbumByName(const char *name) {
	int32_t hash=SimpleHash(name);
        
        for (int i=0; i<shuffleIdx->nAlbum;i++) 
                if ( hash==shuffleIdx->Album[i].Hash && shuffleIdx->Album[i].Item
                        && strcmp(name,shuffleIdx->Album[i].Item->GetFilename())==0)

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
        do {
		++shuffleIdx->currShuffleIdx;
        } while ( shuffleIdx->currShuffleIdx<shuffleIdx->nIdx 
                        && !shuffleIdx->Idx[shuffleIdx->currShuffleIdx].Item );
	
        LISTDEB("NextFile currShuffleIdx %d\n",shuffleIdx->currShuffleIdx);

        if ( shuffleIdx->currShuffleIdx > shuffleIdx->nIdx )
                shuffleIdx->currShuffleIdx=shuffleIdx->nIdx;

	if ( shuffleIdx->Idx[shuffleIdx->currShuffleIdx].Item )
		return shuffleIdx->Idx[shuffleIdx->currShuffleIdx].Item->GetFilename();
	return NULL;
};

char * cPlayList::PrevFile() {
        LISTDEB("PrevFile currShuffleIdx %d\n",
                        shuffleIdx->currShuffleIdx);
        do {
		--shuffleIdx->currShuffleIdx;
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
        do {
		++shuffleIdx->currShuffleIdx;
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
	return NULL;
};

char * cPlayList::PrevAlbumFile() {
        LISTDEB("PrevAlbumFile currShuffleIdx %d\n",
                        shuffleIdx->currShuffleIdx);
        cPlayList *currAlbum=shuffleIdx->Idx[shuffleIdx->currShuffleIdx].Album;
        do {
	        --shuffleIdx->currShuffleIdx;
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
        //Shuffle();
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
                        printf("Martin, depp!! %d\n",xchange1);
                if (xchange2 >=shuffleIdx->nIdx)
                        printf("Martin, depp!! %d\n",xchange2);
                
		LISTDEB("Shuffle %4d(%4d) - %4d(%4d) \n",
                   xchange1,shuffleIdx[xchange1],xchange2,shuffleIdx[xchange2]);
                index=shuffleIdx->Idx[xchange1];
                shuffleIdx->Idx[xchange1]=shuffleIdx->Idx[xchange2];
                shuffleIdx->Idx[xchange2]=index;
        };
/*
	for (int i=0; i<nItems ; i++) 
		LISTDEB("shuffleIdx[%d]: %d\n",i,shuffleIdx->Idx[i]);       
*/
};
/*
void cPlayList::CleanShuffleIdx() {
        // remove deleted files from shuffleIdx
        int fillIndex=0;
        int newNItems=GetNoItemsRecursive();
        
        for (int i=0; i<nItems; i++) 
                if ( GetItemByIndex(shuffleIdx[i]) )
                        shuffleIdx[fillIndex++]=shuffleIdx[i];
        
        // for future adds
        for (int i=newNItems; i<MAX_ITEMS; i++)
                shuffleIdx[i]=maxIdx+i-newNItems;
        nItems=newNItems;
};*/
