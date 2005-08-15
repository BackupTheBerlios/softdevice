/*
 * Media Player plugin for VDR
 *
 * Copyright (C) 2005 Martin Wache
 *
 * This code is distributed under the terms and conditions of the
 * GNU GENERAL PUBLIC LICENSE. See the file COPYING for details.
 *
 * $Id: PlayList.h,v 1.9 2005/08/15 09:07:30 wachm Exp $
 */

#ifndef __PLAYLIST_H__
#define __PLAYLIST_H__

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

                inline void SetTitle( const char *const Title) { 
                        strncpy(title,Title,STR_LENGTH);
                        title[STR_LENGTH-1]=0;
                };

                inline char *GetAlbum()
                { return album; };

                inline void SetAlbum( const char *const Album) {
                        strncpy(album,Album,STR_LENGTH);
                        album[STR_LENGTH-1]=0;
                };

                inline char *GetAuthor()
                { return author; };

                inline void SetAuthor( const char *const Author) {
                        strncpy(author,Author,STR_LENGTH);
                        author[STR_LENGTH-1]=0;
                };

                inline int GetDuration()
                { return duration; };

                inline void SetDuration(int Duration) {
                        duration=Duration;
                };
}; 

struct sPlayListOptions {
	int shuffle;
	int autoRepeat;
};

class cPlayList : public cPlayListItem {
//	friend class cEditList;
//	friend class cReplayList;
	
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
	inline cPlayListItem *GetFirst()
	{ return first; };

	int GetNoItems();
	int GetNoItemsRecursive();

        bool ScanDir(char * dirname, bool recursive = true);
        bool AddDir(char * dirname,char *Title = NULL, bool recursive = true);
        bool AddM3U(char * Filename,char *Title = NULL);

	void Shuffle();
	inline bool IsDirty()
	{ return shuffleIdx ? shuffleIdx->reshuffled : 0; };

	inline void SetClean()
	{ if (shuffleIdx)  shuffleIdx->reshuffled = false; };

        inline int GetCurrShuffleIdx()
	{ return shuffleIdx ? shuffleIdx->currShuffleIdx : -1; };
        inline void SetCurrShuffleIdx(int Idx)
	{ if (shuffleIdx) shuffleIdx->currShuffleIdx = Idx; };

	inline int GetShuffleNIdx()
	{ return shuffleIdx ? shuffleIdx->nIdx : -1; };
	
	char *CurrFile();
        char *NextFile();
	char *PrevFile();
	char *NextAlbumFile();
	char *PrevAlbumFile();
	
	virtual void Save(FILE *out);
	int Load(FILE *out, const char * StartLine=NULL);

        int LoadM3U(const char *Filename, const char *Name);
};
        
#endif
