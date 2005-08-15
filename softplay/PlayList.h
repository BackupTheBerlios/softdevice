/*
 * Media Player plugin for VDR
 *
 * Copyright (C) 2005 Martin Wache
 *
 * This code is distributed under the terms and conditions of the
 * GNU GENERAL PUBLIC LICENSE. See the file COPYING for details.
 *
 * $Id: PlayList.h,v 1.7 2005/08/15 07:27:42 wachm Exp $
 */

#ifndef __PLAYLIST_H__
#define __PLAYLIST_H__

#include "vdr/osdbase.h"
#include "softplay.h"

#define MAX_ITEMS   2000

class cPlayListItem;
class cPlayList;

struct sIdx {
        cPlayList *Album;
        cPlayListItem *Item;
        int32_t Hash;
};
                
struct sItemIdx {
        int currShuffleIdx;
        int nIdx;
        sIdx *Idx;
        int nAlbum;
        sIdx *Album;
	bool reshuffled;
};      

class cPlayListItem {
        friend class cPlayList;
        private:
                char name[SHORT_STR];
                char filename[STR_LENGTH];
        protected:
                int idx;
                cPlayListItem *next;
                cPlayListItem *previous;
        public:
                cPlayListItem(const char *Filename=NULL,  const char *Name=NULL);
                virtual ~cPlayListItem();

                void InsertSelfIntoList(cPlayListItem *Next,
                                cPlayListItem *previous);

                inline char * GetName()
                { return name; };
 
                inline char * GetFilename()
                { return filename; };

        protected:
                inline void SetName(const char *Name) {
                        strncpy(name,Name,SHORT_STR);
                        name[SHORT_STR-1]=0;
                };
                inline void SetFilename(const char *Filename) {
                        strncpy(filename,Filename,STR_LENGTH);
                        filename[STR_LENGTH-1]=0;
                };

        public:
                virtual void BuildIdx(sItemIdx *shuffleIdx);
                        
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

		virtual void Save(FILE *out)
		{};

                virtual void ParseSaveLine(const char *Line)
                {};
        protected:
                const char *ParseTypeFilenameName(const char *Str, char *Type, char *Filename, char *Name);
};

class cPlayListRegular: public cPlayListItem {
        private:
                char title[STR_LENGTH];
                char album[STR_LENGTH];
                char author[STR_LENGTH];

                int duration;
        public:
                cPlayListRegular(char *Filename,  char *Name) :
                        cPlayListItem(Filename,Name) {};
                virtual ~cPlayListRegular() {};

		virtual void Save(FILE *out);

                virtual void ParseSaveLine(const char *Line){};

                inline char *GetTitle() 
                { return title; };

                inline char *SetTitle( const char *const Title) { 
                        strncpy(title,Title,STR_LENGTH);
                        title[STR_LENGTH-1]=0;
                };

                inline char *GetAlbum()
                { return album; };

                inline char *SetAlbum( const char *const Album) {
                        strncpy(album,Album,STR_LENGTH);
                        album[STR_LENGTH-1]=0;
                };

                inline char *GetAuthor()
                { return author; };

                inline char *SetAuthor( const char *const Author) {
                        strncpy(author,Author,STR_LENGTH);
                        author[STR_LENGTH-1]=0;
                };

                inline int GetDuration()
                { return duration; };

                inline int SetDuration(int Duration) {
                        duration=Duration;
                };
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

struct sPlayListOptions {
	int shuffle;
	int autoRepeat;
};

class cPlayList : public cPlayListItem {
	friend class cEditList;
	friend class cReplayList;
	
	//char ListName[STR_LENGTH];
private:
        cPlayListItem *first;
        cPlayListItem *last;
        bool shuffleIdxOwner;
        sItemIdx *shuffleIdx;
       
        sPlayListOptions options;
  public:
        cPlayList(const char *Filename=NULL, const char *Name=NULL,
                        sItemIdx *shuffleIdx=NULL);
  private:
	cPlayList(const cPlayList &List) {};
	cPlayList &operator=(const cPlayList & List) {return *this;};
  public:
        virtual ~cPlayList();

        virtual void BuildIdx(sItemIdx *ShuffleIdx);
        inline void BuildIdx()
        {BuildIdx(shuffleIdx);}; 

        void PrepareForPlayback();
	void SetOptions(sPlayListOptions &Options);
	void GetOptions(sPlayListOptions &Options)
	{ Options=options; };
        
        virtual cPlayListItem *GetItemByIndex(int Index);
        inline cPlayListItem *GetShuffledItemByIndex(int Index) {   
                if (!shuffleIdx) return NULL;
                return shuffleIdx->Idx[Index].Item; 
        };
        cPlayListItem *GetItemByFilename(const char *Filename);
        cPlayListItem *GetAlbumByFilename(const char *Filename);
        int GetIndexByItem(const cPlayListItem *Item);

  private:
        bool RemoveItemFromList(cPlayListItem *Item);
  public:
        bool RemoveItem(cPlayListItem *Item);
        void AddItemAtEnd(cPlayListItem *Item);
	cPlayList *GetItemAlbum(cPlayListItem *Item);
        
        bool AddFile(char * Filename,char *Title = NULL);

        cPlayListItem *GetItem(int No);
	int GetNoItems();
	int GetNoItemsRecursive();

        bool ScanDir(char * dirname, bool recursive = true);
        bool AddDir(char * dirname,char *Title = NULL, bool recursive = true);
        bool AddM3U(char * Filename,char *Title = NULL);

	void Shuffle();

        char *CurrFile();
        char *NextFile();
	char *PrevFile();
	char *NextAlbumFile();
	char *PrevAlbumFile();
	
	virtual void Save(FILE *out);
	int Load(FILE *out, const char * StartLine=NULL);

        int LoadM3U(const char *Filename, const char *Name);
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
