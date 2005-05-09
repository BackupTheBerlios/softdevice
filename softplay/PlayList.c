/*
 * Media Player plugin for VDR
 *
 * Copyright (C) 2005 Martin Wache
 *
 * This code is distributed under the terms and conditions of the
 * GNU GENERAL PUBLIC LICENSE. See the file COPYING for details.
 *
 * $Id: PlayList.c,v 1.2 2005/05/09 21:40:05 wachm Exp $
 */
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
        RemoveSelfFromList();
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

void cPlayListItem::RemoveSelfFromList() {
        LISTDEB("RemoveSelfFromList next %p previous %p \n",next,previous);
        if (next) 
                next->previous=previous;
        if (previous) 
                previous->next=next;
        next=NULL;
        previous=NULL;
};

int cPlayListItem::BuildIdx(int startIdx) {
        idx=startIdx;
        return startIdx+1;
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
        SetHelp("Replay","Add Files","Delete","Stop");
        lastActivity=time(NULL);
};

cEditList::~cEditList() {
};

eOSState cEditList::ProcessKey(eKeys Key) {
        eOSState state = cOsdMenu::ProcessKey(Key);
        if (state != osUnknown ) 
                return state;

        switch (state) {
                cPlayList *Item;
                default: switch (Key) {
                                 case kOk:
                                         LISTDEB("Current %d GetItem %p\n",Current(),playList->GetItem(Current()));
                                         Item=dynamic_cast<cPlayList*>
                                                 (playList->GetItem(Current()));
                                         LISTDEB("Item %p\n",Item);
                                         
                                         if (Item) {
                                                 cEditList *Menu=new cEditList(Item);
                                                 return AddSubMenu(Menu);
                                         };
                                         state = osContinue;
                                         break;
                                 case kRed:
                                         state= osBack;
                                         break;
                                 case kBack:
                                         state= osBack;
                                         break;
				 case kYellow:
				 	 LISTDEB("Del current\n");
					 Del(Current());
					 playList->RemoveItemFromList(
					 	playList->GetItem(Current()));
					 Display();
					 state=osContinue;
					 break;

                                 default:    
                                         break;
                         }
        }
        return state;
}

// ----cReplayList------------------------------------------------------------
cReplayList::cReplayList(cPlayList * List) : cOsdMenu(List->ListName) {
        playList=List;
        SetHelp("Edit","Save","???","Stop");
        SetCurrent(Get(playList->currShuffleIdx));
        RebuildList();
        lastActivity=time(NULL);
};

cReplayList::~cReplayList() {
};

void cReplayList::RebuildList() {
        LISTDEB("RebuildList\n");

        cPlayListItem * Item;
        for (int i = 0 ; i<playList->GetNoItemsRecursive(); i++) {
		Item=playList->GetShuffledItemByIndex(i);
                if (!Item) {
                        printf("Error getting all files for list index %d shuffleIdx %d\n",
                                        i,playList->shuffleIdx[i]);
                        continue;
                };
                
		LISTDEB("Add item %d shuffleIdx %d: %s\n",
                                i,playList->shuffleIdx[i],Item->GetName());
                Add(new cOsdItem(Item->GetName(),
                   osUnknown),false);
        };
        lastListItemCount = playList->GetNoItemsRecursive();
};

void cReplayList::UpdateStatus() {
        int current;
        int total;
        char Status[60];
        
        if (!cControl::Control() || 
                        !cControl::Control()->GetIndex(current,total))
                return;
        
        snprintf(Status,60,"Time %02d:%02d:%02d/%02d:%02d:%02d Title %d/%d",
                        current/3600,current/60%60,current%60,
                        total/3600,total/60%60,total%60,
                        playList->currShuffleIdx+1,playList->nItems);
        SetStatus(Status);

        if (displayedCurrIdx != playList->currShuffleIdx){
                cPlayListItem *Item=playList->GetShuffledItemByIndex(
                                playList->currShuffleIdx);
                if (Item) {
                        snprintf(Status,60,"%s: %30s",playList->ListName,
                                                Item->GetName());
                        SetTitle(Status);
                        Display();
                        displayedCurrIdx=playList->currShuffleIdx;
                } else printf("Didn't get currShuffleIdx %d!\n",playList->currShuffleIdx);
        };
};

eOSState cReplayList::ProcessKey(eKeys Key) {
        eOSState state = cOsdMenu::ProcessKey(Key);
        if (Key!=kNone) 
                lastActivity=time(NULL);
        
        if (state != osUnknown ) 
                return state;

        if ( lastListItemCount != playList->GetNoItemsRecursive() ) {
                playList->CleanShuffleIdx();
                Clear();
                RebuildList();
        };
        
        if (Current() != playList->currShuffleIdx && 
                        time(NULL) - lastActivity > 120 ) {
                LISTDEB("SetCurrent current title %d  time %d lastActivity %d\n",
                                playList->currShuffleIdx,time(NULL),lastActivity);
                SetCurrent(Get(playList->currShuffleIdx));
                Display();
        };

        switch (state) {
                default: switch (Key) {
                                 case kOk:
                                         // skip to current track
                                         playList->currShuffleIdx = Current()-1;
                                         state = osUser3;
                                         // want to have automatic track change
                                         lastActivity=time(NULL)-300;
                                         break;
                                 case kRed:
                                         state= AddSubMenu(new cEditList(playList));
                                         break;
                                 case kBack:
                                         state= osBack;
                                         break;

                                 default:    
                                         break;
                         }
        }
        if (!HasSubMenu())
                UpdateStatus();
        return state;
}


// -----cPlayList------------------------------------------------------------
cPlayList::cPlayList(char *Filename, char *Name) 
                      : cPlayListItem(Filename,Name) {
        currItemIdx=-1;
        currShuffleIdx=-1;
        minIdx=0;
        maxIdx=0;
        first=NULL;
        last=NULL;
        
        strncpy(ListName,"Playlist 1",STR_LENGTH);
        ListName[STR_LENGTH-1]=0;
	shuffleIdx=NULL;
};

cPlayList::~cPlayList() {
};

void cPlayList::RemoveItemFromList(cPlayListItem *Item) {
        LISTDEB("RemoveItemFromList %p, first %p last %p\n",Item,first,last);
        if (first && Item == first)
                first = first->next;
        if (last && Item == last)
                last = last->previous;
        Item->RemoveSelfFromList();
        delete Item;
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
};                    

int cPlayList::BuildIdx(int startIdx) {
	cPlayListItem *currItem=NULL;
	if (currItemIdx!=-1) {
		// while playback rember current item, if this has been
		// deleted find the next not deleted item
		while ( ! (currItem=GetItemByIndex(shuffleIdx[currShuffleIdx]) ) && 
				currShuffleIdx<nItems)
			currShuffleIdx++;
	};
			
        cPlayListItem *Item=first;
        minIdx=startIdx;
        LISTDEB("BuildIdx startIdx %d\n",startIdx);

        while (Item) {
                LISTDEB("Item %p Item->GetNext() %p \n",Item,Item->GetNext());
                startIdx=Item->BuildIdx(startIdx);
                Item=Item->GetNext();
        };
        nItems=startIdx-minIdx;
        LISTDEB("BuildIdx maxIdx %d nItems %d\n",startIdx,nItems);
	
	if (currItem) {
		// FIXME if currItem has been deleted do I have to subtract one?
		currItemIdx=Item->GetIdx();
		for (int i=0; i<nItems; i++) 
			if ( currItemIdx==shuffleIdx[i] ) {
				currShuffleIdx=i;
				break;
			};
	};
        return maxIdx=startIdx;
};

cPlayListItem *cPlayList::GetItemByIndex(int Index) {
        if ( minIdx > Index)
                return NULL;
        if ( Index > maxIdx)
                return NULL;
        
        cPlayListItem *Item=first;
        cPlayListItem *found;
        while (Item) {
                if ( (found=Item->GetItemByIndex(Index)) )
                        return found;
                Item=Item->GetNext();
        };
        return NULL;
};

bool cPlayList::AddFile(char * filename, char *title) {
        LISTDEB("AddFile %s\n",filename);
        cPlayListItem *Item= new cPlayListRegular(filename,title);
        LISTDEB("Item created %p\n",Item);
        AddItemAtEnd(Item);
        maxIdx=Item->BuildIdx(maxIdx);
        nItems=maxIdx-minIdx;
        LISTDEB("Item added first %p last %p\n",first,last);
        
        return true;
};

bool cPlayList::AddDir(char * dirname, char * title, bool recursive) {
        LISTDEB("AddDir %s\n",filename);
        cPlayList *Item= new cPlayList(filename,title);
        Item->ScanDir(dirname,recursive);
        AddItemAtEnd(Item);
        maxIdx=Item->BuildIdx(maxIdx); 
        nItems=maxIdx-minIdx;

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
        LISTDEB("NextFile currItemIdx: %d nItems %d currShuffleIdx %d\n",
                        currItemIdx,nItems,currShuffleIdx);
    	cPlayListItem *Item=NULL;
        do {
		currItemIdx=shuffleIdx[++currShuffleIdx];
        } while ( !(Item=GetItemByIndex(currItemIdx))  && currShuffleIdx<nItems );
	
        LISTDEB("NextFile currItemIdx: %d currShuffleIdx %d\n",currItemIdx,currShuffleIdx);

	if (Item)
		return Item->GetFilename();
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
        currItemIdx=-1;
	currShuffleIdx=-1;

        BuildIdx();
	if (!shuffleIdx)
		shuffleIdx= new int[MAX_ITEMS];
        for (int i=0; i<MAX_ITEMS ; i++) {
                shuffleIdx[i]=i;
        };
        Shuffle();
};

void cPlayList::Shuffle() {
        LISTDEB("Shuffle playlist nItems %d currShuffleIdx %d\n",nItems,currShuffleIdx);

        int index=0;
        for (int i=0; i<2*nItems ; i++) {
		//FIXME - I guess the ranges are not completly correct
                long int xchange1=( random()*(nItems-currShuffleIdx)/RAND_MAX)
			+currShuffleIdx;
                long int xchange2=( random()*(nItems-currShuffleIdx)/RAND_MAX)
			+currShuffleIdx;
		LISTDEB("Shuffle %d - %d \n",xchange1,xchange2);
                index=shuffleIdx[xchange1];
                shuffleIdx[xchange1]=shuffleIdx[xchange2];
                shuffleIdx[xchange2]=index;
        };

	for (int i=0; i<nItems ; i++) 
		LISTDEB("shuffleIdx[%d]: %d\n",i,shuffleIdx[i]);       
};

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
};
