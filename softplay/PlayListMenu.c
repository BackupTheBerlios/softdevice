/*
 * Media Player plugin for VDR
 *
 * Copyright (C) 2005 Martin Wache
 *
 * This code is distributed under the terms and conditions of the
 * GNU GENERAL PUBLIC LICENSE. See the file COPYING for details.
 *
 * $Id: PlayListMenu.c,v 1.2 2005/08/15 13:13:14 wachm Exp $
 */
#include "vdr/player.h"

#include "PlayListMenu.h"

#define MENUDEB(out...) {printf("MENUDEB: ");printf(out);}

#ifndef MENUDEB
#define MENUDEB(out...)
#endif


// ----cAlbumList------------------------------------------------------------
cAlbumList::cAlbumList(cPlayList * List) : cOsdMenu(List->GetName()) {
        playList=List;
        cPlayListItem * Item=playList->GetFirst();
        while (Item) {
                MENUDEB("EditList add %s: %p\n",Item->GetName(),Item);
                Add(new cOsdItem(Item->GetName(),
                   osUnknown),false);
                Item=Item->GetNext();
        };
        SetHelp(tr("Options"),tr("(Add Files)"),tr("Delete"),tr("Stop"));
        lastActivity=time(NULL);
};

cAlbumList::~cAlbumList() {
};

eOSState cAlbumList::ProcessKey(eKeys Key) {
        eOSState state = cOsdMenu::ProcessKey(Key);
        if (state != osUnknown ) 
                return state;

        cPlayList *List;
        cPlayListItem *Item;
        switch (Key) {
                case kOk:
                        MENUDEB("Current %d GetItem %p\n",Current(),playList->GetItem(Current()));
                        Item=playList->GetItem(Current());
                        List=dynamic_cast<cPlayList*>(Item);
                        MENUDEB("Item %p\n",Item);

                        if (List) {
                                cAlbumList *Menu=new cAlbumList(List);
                                return AddSubMenu(Menu);
                        }  else {
                                // skip to current track
                                playList->SetCurrShuffleIdx(
                                        playList->GetIndexByItem(Item) );
                                state = PLAY_CURR_FILE;
                        };
                        break;
                case kBack:
                        state= osBack;
                        break;
                case kBlue:
                        state= osEnd;
                        break;
                case kRed:
                        return AddSubMenu(new cPlOptionsMenu(playList));
                        break;
                case kYellow:
                        MENUDEB("Del current %d: %s\n",
                                        Current(),
                                        playList->GetItem(Current())->GetName() );
                        playList->RemoveItem(
                                        playList->GetItem(Current()));
                        MENUDEB("Remove finished\n");
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
cReplayList::cReplayList(cPlayList * List) : cOsdMenu(List->GetName()) {
        playList=List;
        SetHelp(tr("Options"),tr("(Add)"),tr("Delete"),tr("Stop"));
        RebuildList();
};

cReplayList::~cReplayList() {
};

void cReplayList::RebuildList() {
        int nItems=playList->GetNIdx();
        MENUDEB("RebuildList nItems %d \n",nItems);

        cPlayListItem * Item;
        for (int i = 0 ; i<nItems; i++) {
		Item=playList->GetShuffledItemByIndex(i);
                if (!Item) {
                        printf("Error getting all files for list index %d\n",
                                        i);
                        continue;
                };
                
		MENUDEB("Add item %d (%p)  %s\n",
                                i,Item,Item->GetName());
                Add(new cOsdItem(Item->GetName(),
                   osUnknown),false);
        };
        lastListItemCount = playList->GetNIdx();
	playList->SetClean();
        SetCurrent(Get(playList->GetCurrIdx()));
        lastActivity=time(NULL)-600;
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
                        playList->GetCurrIdx()+1,
			playList->GetNIdx());
        SetStatus(Status);

        if (displayedCurrIdx != playList->GetCurrIdx()){
                cPlayListItem *Item=playList->GetShuffledItemByIndex(
                                playList->GetCurrIdx());
                if (Item) {
                        PrintCutDownString(Name,Item->GetName(),30);
                        snprintf(Status,60,"%s: %s",playList->GetName(),
                                                Name);
                        SetTitle(Status);
                        Display();
                        displayedCurrIdx=playList->GetCurrIdx();
                } else printf("Didn't get currShuffleIdx %d!\n",playList->GetCurrIdx());
        };
};

eOSState cReplayList::ProcessKey(eKeys Key) {
        eOSState state = cOsdMenu::ProcessKey(Key);

	// don't move cursor when it has been moved by the user
	// a short while ago
        if ( Key==kUp || Key==kDown || Key==kRight || Key==kLeft ) 
                lastActivity=time(NULL);
        
        if ( lastListItemCount != playList->GetNIdx()
	     || playList->IsDirty() ) {
                Clear();
                RebuildList();
                Display();
        };
        
        if (Current() != playList->GetCurrIdx() && 
                        time(NULL) - lastActivity > 12 ) {
                MENUDEB("SetCurrent current title %d  time %d lastActivity %d\n",
                                playList->GetCurrIdx(),time(NULL),lastActivity);
                SetCurrent(Get(playList->GetCurrIdx()));
                Display();
        };

	if (state != osUnknown ) 
		return state;

        cPlayListItem *Item;
        switch (Key) {
                case kOk:
                        // skip to current track
                        playList->SetCurrShuffleIdx( Current() );
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
                case kGreen:
			// not yet implemented
                        state= osContinue;
                        break;
                case kRed:
                        return AddSubMenu(new cPlOptionsMenu(playList));
                        break;
                case kYellow:
                        Item=playList->GetShuffledItemByIndex(Current());
                        if (!Item) {
                                printf("No current Item %d!\n",Current());
                                break;
                        };
                        MENUDEB("Del current %d: %s\n",
                                        Current(),
                                        Item->GetName() );
                        playList->RemoveItem(Item);
			lastListItemCount = playList->GetNIdx();
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


// ---cPlOptionsMenu-------------------------------------------------------

cPlOptionsMenu::cPlOptionsMenu(cPlayList *PlayList)
        : cOsdMenu(tr("Options"),33) {
        playList=PlayList;
        playList->GetOptions(playListOptions);
        Add(new cMenuEditBoolItem(tr("Shuffle Mode"),
                               &playListOptions.shuffle, tr("no"), tr("yes")));
        Add(new cMenuEditBoolItem(tr("Auto Repeat"),
                               &playListOptions.autoRepeat, tr("no"), tr("yes")));
};                                   

cPlOptionsMenu::cPlOptionsMenu(sPlayListOptions *Options)
        : cOsdMenu(tr("Options"),33) {
        playList=NULL;
        playListOptions=*Options;
        options=Options;
        Add(new cMenuEditBoolItem(tr("Shuffle Mode"),
                               &playListOptions.shuffle, tr("no"), tr("yes")));
        Add(new cMenuEditBoolItem(tr("Auto Repeat"),
                               &playListOptions.autoRepeat, tr("no"), tr("yes")));
};          

cPlOptionsMenu::~cPlOptionsMenu() {
};

eOSState cPlOptionsMenu::ProcessKey(eKeys Key)
{
  eOSState state = cOsdMenu::ProcessKey(Key);
                                                                                
  if (state == osUnknown) {
     switch (Key) {
       case kOk: if (playList)
                         playList->SetOptions(playListOptions);
                 else *options=playListOptions;
                 state = osBack;
                 break;
       default: break;
       }
     }
  return state;
}
                                                                                


