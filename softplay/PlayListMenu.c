/*
 * Media Player plugin for VDR
 *
 * Copyright (C) 2005 Martin Wache
 *
 * This code is distributed under the terms and conditions of the
 * GNU GENERAL PUBLIC LICENSE. See the file COPYING for details.
 *
 * $Id: PlayListMenu.c,v 1.4 2006/04/02 20:19:12 wachm Exp $
 */
#include "vdr/player.h"

#include "PlayListMenu.h"
#include "FileIndex.h"

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
                                playList->SetCurrIdx(
                                                playList->GetIndexByItem(Item));
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
cReplayList::eMode cReplayList::Mode=cReplayList::eMNormal;

cReplayList::cReplayList(cPlayList * List) 
                : cOsdMenu(List->GetName(),20,1,16,5) {
        displayedCurrIdx=-1;
        playList=List;
        RebuildList();
	UpdateHelp();
        lastModeActivity=time(NULL)-600;
        lastActivity=time(NULL)-600;
        hold=false;
};

cReplayList::~cReplayList() {
};

void cReplayList::UpdateHelp() {
	switch (Mode) {
		case eMEdit:
			SetHelp(tr("Mode"),tr("(Add)"),
					tr("Delete"),tr("Move"));
			break;
		case eMNormal:
			SetHelp(tr("Mode"),tr("Skip Forward"),
					tr("Skip Back"),tr("Stop"));
			break;
		case eMOptions:
			SetHelp(tr("Mode"),tr("Options"),NULL,tr("Save"));
			break;
                default:
                        break;
	};
        lastMode=Mode;
};

void cReplayList::PrintItemStr(char *ItemStr, int count,
                        cPlayListItem *Item, bool hold ) {
        cIndex *Idx=FileIndex ? 
                FileIndex->GetIndex(Item->GetFilename()) : NULL ;
        char holdR = hold ? '>' : ' ';
        char holdL = hold ? '<' : ' ';
               
        if ( Idx && *Idx->GetTitle() ) { 
                snprintf(ItemStr,100," %c%s\t\t%s\t%3d:%02d%c ",
                                holdR,
                                Idx->GetTitle(),
                                Idx->GetAuthor(),
                                Idx->GetDuration()/60,
                                Idx->GetDuration()%60,
                                holdL);
        } else
                snprintf(ItemStr,count," %c%s%c ",
                                holdR,Item->GetName(),holdL);
};

void cReplayList::RebuildList() {
        int nItems=playList->GetNIdx();
        MENUDEB("RebuildList nItems %d \n",nItems);

        cPlayListItem * Item;
        for (int i = 0; i < nItems; i++) {
		Item=playList->GetItemByIndex(i);
                if (!Item) {
                        printf("Error getting all files for list index %d\n",
                                        i);
                        continue;
                };
                
		MENUDEB("Add item %d (%p)  %s\n",
                                i,Item,Item->GetName());

                char ItemStr[100];
                PrintItemStr(ItemStr,100,Item);                
                Add(new cOsdItem(ItemStr,osUnknown),false);
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
        cPlayListItem *Item;
        
        if (!cControl::Control() || 
                        !cControl::Control()->GetIndex(current,total))
                return;
        
        snprintf(Status,60,"Time %02d:%02d:%02d/%02d:%02d:%02d Title %d/%d",
                        current/3600,current/60%60,current%60,
                        total/3600,total/60%60,total%60,
                        playList->GetCurrIdx()+1,
			playList->GetNIdx());
        SetStatus(Status);

        if ( displayedCurrIdx != playList->GetCurrIdx() 
                        && ( Item=playList->GetCurrItem() ) ) {
                cIndex *Idx=FileIndex ? 
                        FileIndex->GetIndex(Item->GetFilename()) : NULL ;
                if ( Idx && *Idx->GetTitle() ) 
                        PrintCutDownString(Name,Idx->GetTitle(),30);
                else PrintCutDownString(Name,Item->GetName(),30);
                
                snprintf(Status,60,"%s: %s",playList->GetName(),
                                Name);
                SetTitle(Status);
                Display();
                displayedCurrIdx=playList->GetCurrIdx();
        };
};

eOSState cReplayList::ProcessKey(eKeys Key) {
        int lastCurrent=Current();
        eOSState state = cOsdMenu::ProcessKey(Key);


        // fallback to default mode after some time of inactivity
        if ( Key!=kNone) 
                lastModeActivity=time(NULL);
       
        if ( lastMode !=eMNormal && 
                        time(NULL) - lastModeActivity > 40 ) {
                if (hold) {
                        // break move
                        hold=false;
                        SetItemStr(Get(Current()),
                                        playList->GetItemByIndex(Current()),
                                        hold);
                        Move(Current(),origPos);
                        playList->GetShuffleIdx()->Move(Current(),
                                        origPos);
                        SetCurrent(Get(origPos));
                        Display();
                };
               Mode=eMNormal;
        };
 
        // don't move cursor when it has been moved by the user
	// a short while ago or in move item mode
        if ( Key==kUp || Key==kDown || Key==kRight || Key==kLeft ) 
                lastActivity=time(NULL);

        if (Current() != playList->GetCurrIdx() && 
                        time(NULL) - lastActivity > 40
                        && !hold ) {
                MENUDEB("SetCurrent current title %d  time %d lastActivity %d\n",
                                playList->GetCurrIdx(),int(time(NULL)),int(lastActivity));
                SetCurrent(Get(playList->GetCurrIdx()));
                Display();
        };
       
        // handle move item mode
        if ( lastMode == eMEdit && hold ) {
                // moves
                if ( (Key==kUp || Key==kDown|| Key==kRight || Key==kLeft) ) {
                        Move(lastCurrent,Current());
                        playList->GetShuffleIdx()->Move(lastCurrent,Current()); 
                        Display();
                };
               
                // end move
                switch(Key) {
                        case kBack:
                        case kRed:
                        case kGreen:
                        case kYellow:
                                // break Move
                                hold=false;
                                SetItemStr(Get(Current()),
                                                playList->GetItemByIndex(Current()),
                                                hold);
                                Move(Current(),origPos);
                                playList->GetShuffleIdx()->Move(Current(),
                                                origPos);
                                SetCurrent(Get(origPos));
                                Display();

                                Key=kNone;
                                state= osContinue;
                                break;
                        case kOk:
                        case kBlue:
                                // finish move
                                hold=false;
                                SetItemStr(Get(Current()),
                                                playList->GetItemByIndex(Current()),
                                                hold);

                                DisplayCurrent(true);
                                state= osContinue;
                                break;
                        default:
                                break;
                } 
        
        };

        // handle new items or removed items
        if ( lastListItemCount != playList->GetNIdx()
	     || playList->IsDirty() ) {
                Clear();
                RebuildList();
                Display();
        };
        

	if (state != osUnknown ) 
		return state;


	switch (Key) {
		case kOk:
			// skip to current track
			playList->SetCurrIdx( Current() );
			state = PLAY_CURR_FILE;
			// want to have automatic track change
			lastActivity=time(NULL)-300;
			break;
		case kBack:
			state= osBack;
			break;
		default:    
			break;
	};

        if ( Key >= kRed && Key <= kBlue)
                state=ProcessColourKeys(Key);
        
        if (!HasSubMenu())
                UpdateStatus();
        
        if ( Mode != lastMode ) 
		UpdateHelp();

        return state;
}

eOSState cReplayList::ProcessColourKeys(eKeys Key) {
        eOSState state = osUnknown;
        
	if (Key==kRed) { 
		Mode=(eMode) ( ( Mode + 1 ) % eMLast );
		return osContinue;
	};
	
	switch (lastMode) {
		case eMNormal:
			switch(Key) {
				case kGreen:
					// Skip backward, pass to cControl
					state= osUnknown;
					break;
				case kYellow:
					// Skip forward, pass to cControl
					state= osUnknown;
					break;
				case kBlue:
                                        // end replay
					state= osEnd;
					break;
                                default:
                                        break;
			};
			break;
		case eMEdit:
			switch(Key) {
				case kGreen:
					// add files TODO
					state= osContinue;
					break;
				case kYellow:
					// delete Item
					MENUDEB("Del current %d: %s\n",
                                                 Current(),
                                                 playList->GetItemByIndex(Current())->GetName() );
					playList->RemoveItem(
					  playList->GetItemByIndex(Current()));
					MENUDEB("Remove finished\n");
					Del(Current());
					Display();
					state=osContinue;
					break;
				case kBlue:
                                        // move files TODO
                                        hold=true;
                                        origPos=Current();
                                        SetItemStr(Get(Current()),
                                               playList->GetItemByIndex(Current()),
                                               hold);
                                        DisplayCurrent(true);
					state= osContinue;
					break;
                                default:
                                        break;
			};
			break;
		case eMOptions:
			switch(Key) {
				case kGreen:
					// call options menu
                                        state = AddSubMenu(
                                                new cPlOptionsMenu(playList));

					break;
				case kYellow:
					// nothing
					state= osContinue;
					break;
				case kBlue:
                                        // save list TODO
					state= osContinue;
					Softplay->SaveList(playList);
					break;
                                default:
                                        break;
			};
			break;
                default:
                        break;
	};
        return state;
};



// ---cPlOptionsMenu-------------------------------------------------------

cPlOptionsMenu::cPlOptionsMenu(cPlayList *PlayList)
        : cOsdMenu(tr("Options"),33) {
        playList=PlayList;
        playList->GetOptions(playListOptions);

        printf("playlistname '%s' \n",playListOptions.name);
        Add(new cMenuEditStrItem(tr("Name"),playListOptions.name,40,tr(FileNameChars)));
        
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
                                                                                


