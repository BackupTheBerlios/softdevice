/*
 * Media Player plugin for VDR
 *
 * Copyright (C) 2005 Martin Wache
 *
 * This code is distributed under the terms and conditions of the
 * GNU GENERAL PUBLIC LICENSE. See the file COPYING for details.
 *
 * $Id: PlayList.h,v 1.2 2005/05/09 21:40:05 wachm Exp $
 */

#ifndef __PLAYLIST_H__
#define __PLAYLIST_H__

#include "vdr/osdbase.h"

//#define STR_LENGTH  120
#define STR_LENGTH  200
#define SHORT_STR   60
#define MAX_ITEMS   2000

class cPlayList;

class cPlayListItem {
        friend class cPlayList;
        protected:
                char name[SHORT_STR];
                char filename[STR_LENGTH];
                int idx;
                cPlayListItem *next;
                cPlayListItem *previous;
        public:
                cPlayListItem(char *Filename=NULL,  char *Name=NULL);
                virtual ~cPlayListItem();

                void InsertSelfIntoList(cPlayListItem *Next,
                                cPlayListItem *previous);
                void RemoveSelfFromList();

                inline char * GetName()
                { return name; };
 
                inline char * GetFilename()
                { return filename; };

                virtual int BuildIdx(int startIdx=0);
                        
                inline int GetIdx()
                { return idx; };

                virtual cPlayListItem *GetItemByIndex(int Index)
                {if (Index==idx) return this; else return NULL;};

                virtual int GetNoItems()
                {return 1; };

                inline cPlayListItem *GetNext()
                {return next;};

                inline cPlayListItem *GetPrev()
                {return previous;};
};

class cPlayListRegular: public cPlayListItem {
        private:
                char title[STR_LENGTH];
                char album[STR_LENGTH];
                char author[STR_LENGTH];
        public:
                cPlayListRegular(char *Filename,  char *Name) :
                        cPlayListItem(Filename,Name) {};
                virtual ~cPlayListRegular() {};
}; 

class cEditList: public cOsdMenu {
        time_t lastActivity;
        int displayedCurrIdx;
        cPlayList *playList;
        public:
        cEditList(cPlayList * List);
        ~cEditList();
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

class cPlayList : public cPlayListItem {
	friend class cEditList;
	friend class cReplayList;
	
	char ListName[STR_LENGTH];
private:
        cPlayListItem *first;
        cPlayListItem *last;
        int currShuffleIdx;
        int currItemIdx;
        int minIdx;
        int maxIdx;
        int nItems;
	int *shuffleIdx;
        
  public:
        cPlayList(char *Filename=NULL, char *Name=NULL);
        virtual ~cPlayList();

        virtual int BuildIdx(int startIdx=0);

        void PrepareForPlayback();
        
        virtual cPlayListItem *GetItemByIndex(int Index);
        inline cPlayListItem *GetShuffledItemByIndex(int Index)
        { return GetItemByIndex(shuffleIdx[Index]); };

        void RemoveItemFromList(cPlayListItem *Item);
        void AddItemAtEnd(cPlayListItem *Item);
        
        bool AddFile(char * Filename,char *Title = NULL);

        cPlayListItem *GetItem(int No);
	int GetNoItems();
	int GetNoItemsRecursive();

        bool ScanDir(char * dirname, bool recursive = true);
        bool AddDir(char * dirname,char *Title = NULL, bool recursive = true);

	inline cOsdMenu *ReplayList() {return new cReplayList(this);};
	void Shuffle();
	void CleanShuffleIdx();
        char *NextFile();
};
        
        
#endif
