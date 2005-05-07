/*
 * Media Player plugin for VDR
 *
 * Copyright (C) 2005 Martin Wache
 *
 * This code is distributed under the terms and conditions of the
 * GNU GENERAL PUBLIC LICENSE. See the file COPYING for details.
 *
 * $Id: PlayList.h,v 1.1 2005/05/07 20:05:42 wachm Exp $
 */

#ifndef __PLAYLIST_H__
#define __PLAYLIST_H__

#include "vdr/osdbase.h"

//#define STR_LENGTH  120
#define STR_LENGTH  200
#define MAX_ITEMS   500


class cPlayList {
	class cEditList: public cOsdMenu {
		time_t lastActivity;
		cPlayList *playList;
	public:
		cEditList(cPlayList * List);
		~cEditList();
		eOSState ProcessKey(eKeys Key);
		void UpdateStatus();
	};
	friend class cEditList;
	
	char ListName[STR_LENGTH];
public:
	struct ListItem {
                char Filename[STR_LENGTH];
                int Played;
                char Title[STR_LENGTH];
                char Album[STR_LENGTH];
                char Author[STR_LENGTH];
        };
private:
	ListItem Items[MAX_ITEMS];
        int nItems;
        int currItem;
	int shuffleIdx[MAX_ITEMS];
        
  public:
        cPlayList();
        ~cPlayList();

        bool AddFile(char * Filename,char *Title = NULL);

        bool AddDir(char * dirname, bool recursive = true);

	inline cOsdMenu *EditList() {return new cEditList(this);};
	void Shuffle();
        char *NextFile();

	ListItem *GetItem(int index);
};
        
        
#endif
