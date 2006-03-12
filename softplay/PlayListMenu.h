/*
 * Copyright (C) 2005 Martin Wache
 *
 * This code is distributed under the terms and conditions of the
 * GNU GENERAL PUBLIC LICENSE. See the file COPYING for details.
 *
 * $Id: PlayListMenu.h,v 1.2 2006/03/12 20:28:52 wachm Exp $
 */

#ifndef __PLAYLISTMENU_H__
#define __PLAYLISTMENU_H__

#include "vdr/osdbase.h"
#include "PlayList.h"

class cAlbumList: public cOsdMenu {
        time_t lastActivity;
        cPlayList *playList;
        public:
        cAlbumList(cPlayList * List);
        ~cAlbumList();
        eOSState ProcessKey(eKeys Key);
};

class cReplayList: public cOsdMenu {
        time_t lastActivity;
        int displayedCurrIdx;
        cPlayList *playList;
	int lastListItemCount;
	
	enum eMode {
		eMNormal,
		eMEdit,
		eMOptions,
                eMLast,
        }; 
	static eMode Mode;
	eMode lastMode;
        time_t lastModeActivity;

        bool hold;
        int origPos;

        public:
        cReplayList(cPlayList * List);
        ~cReplayList();
	
        void RebuildList();
        
        void PrintItemStr(char *ItemStr, int count,
                        cPlayListItem *Item, bool hold=false);
        
        inline void SetItemStr(cOsdItem *OsdItem, cPlayListItem *Item,
                        bool hold=false) {
                char Str[100];
                PrintItemStr(Str,100,Item,hold);
                OsdItem->SetText(Str);
        };
        
        eOSState ProcessKey(eKeys Key);
        eOSState ProcessColourKeys(eKeys Key);
        
        void UpdateStatus();
        void UpdateHelp();
};

class cPlOptionsMenu : public cOsdMenu {
        protected:
                sPlayListOptions playListOptions;
                sPlayListOptions *options;
                cPlayList *playList;
        public:
                cPlOptionsMenu(cPlayList *PlayList);
                cPlOptionsMenu(sPlayListOptions *Options);
                ~cPlOptionsMenu();
                eOSState ProcessKey(eKeys Key);
};
        


#endif

