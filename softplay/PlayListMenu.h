/*
 * Copyright (C) 2005 Martin Wache
 *
 * This code is distributed under the terms and conditions of the
 * GNU GENERAL PUBLIC LICENSE. See the file COPYING for details.
 *
 * $Id: PlayListMenu.h,v 1.1 2005/08/15 09:07:30 wachm Exp $
 */

#ifndef __PLAYLISTMENU_H__
#define __PLAYLISTMENU_H__

#include "vdr/osdbase.h"
#include "PlayList.h"

class cAlbumList: public cOsdMenu {
        time_t lastActivity;
        int displayedCurrIdx;
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
        public:
        cReplayList(cPlayList * List);
        ~cReplayList();
	void RebuildList();
        eOSState ProcessKey(eKeys Key);
        void UpdateStatus();
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

