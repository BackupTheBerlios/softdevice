/*
 * Media Player plugin for VDR
 *
 * Copyright (C) 2005 Martin Wache
 *
 * This code is distributed under the terms and conditions of the
 * GNU GENERAL PUBLIC LICENSE. See the file COPYING for details.
 *
 * $Id: PlayList.h,v 1.11 2006/03/12 20:28:52 wachm Exp $
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
                
class cItemIdx {
        private:
                cPlayList *owner;
                
                int nIdx;
                sIdx *Idx;
                int nAlbum;
                sIdx *Album;
        public: 
                bool reshuffled;
                int currShuffleIdx;
                
                cItemIdx(cPlayList *Owner);
                ~cItemIdx();

                cPlayList *RemoveItem(cPlayListItem *Item);

                void AddItemAtEnd(cPlayList *Album, cPlayListItem *Item);

                cPlayListItem * GetTrackByFilename(const char *Filename);
                cPlayListItem * GetAlbumByFilename(const char *Filename);
  
                inline cPlayListItem* GetItemByIndex(int Index) 
                { return Idx[Index].Item; };
                
                int GetIndexByItem(const cPlayListItem *Item);
               
                const char *CurrFile();
                const char *NextFile();
                const char *PrevFile();
                const char *NextAlbumFile();
                const char *PrevAlbumFile();

                inline int GetNIdx() 
                { return nIdx; };

                void Shuffle();

                void Move(int From, int To);
};      

class cPlayListItem {
        private:
		char *namePos;
                char filename[STR_LENGTH];
        protected:
                cPlayListItem *next;
                cPlayListItem *previous;
      
        public:
                inline cPlayListItem *GetNext()
                {return next;};

                inline cPlayListItem *GetPrev()
                {return previous;};

        public:
                cPlayListItem(const char *Filename=NULL);
                virtual ~cPlayListItem();

                void InsertSelfIntoList(cPlayListItem *Next,
                                cPlayListItem *previous);

                void RemoveSelfFromList();

                inline const char * GetName()
                { return namePos; };
 
                inline const char * GetFilename()
                { return filename; };

                inline void SetFilename(const char *Filename) {
                        strncpy(filename,Filename,STR_LENGTH);
                        filename[STR_LENGTH-1]=0;
			namePos=rindex(filename,'/');
			if (namePos)
                                namePos++;
                        else namePos=filename;
                };

        public:
                virtual void BuildIdx(cItemIdx *shuffleIdx);
                        
 		virtual void Save(FILE *out);

                virtual void ParseSaveLine(const char *Line)
                {};


        protected:
                const char *ParseTypeFilename(const char *Str, 
                                char *Type, char *Filename);

};

struct sPlayListOptions {
        char name[40];
	int shuffle;
	int autoRepeat;
};

class cPlayList : public cPlayListItem {
private:
        cPlayListItem *first;
        cPlayListItem *last;
        bool shuffleIdxOwner;
        cItemIdx *shuffleIdx;
       
  public:
        sPlayListOptions options;
        cPlayList(const char *Filename=NULL,cItemIdx *shuffleIdx=NULL);
  private:
	cPlayList(const cPlayList &List) {};
	cPlayList &operator=(const cPlayList & List) {return *this;};
  public:
        virtual ~cPlayList();

        virtual void BuildIdx(cItemIdx *ShuffleIdx);
        inline void BuildIdx()
        {BuildIdx(shuffleIdx);}; 

        void PrepareForPlayback();
	void SetOptions(sPlayListOptions &Options);
	void GetOptions(sPlayListOptions &Options);
        
  private:

        void AddItemAtEndToList(cPlayListItem *Item);

        bool RemoveItemFromList(cPlayListItem *Item);

  public:
        bool RemoveItem(cPlayListItem *Item); 

        void AddItemAtEnd(cPlayListItem *Item);

        cPlayListItem *GetItem(int No);
	inline cPlayListItem *GetFirst()
	{ return first; };

        bool AddFile(char * Filename);
        bool AddDir(char * dirname, bool recursive = true);
        bool AddM3U(char * Filename);

        virtual void Save(FILE *out);
	int Load(FILE *out, const char * StartLine=NULL);

        bool ScanDir(char * dirname, bool recursive = true);
        
        int LoadM3U(const char *Filename);

        // interface to cItemIdx    
        cItemIdx *GetShuffleIdx() 
        { return shuffleIdx; };
	inline bool IsDirty()
	{ return shuffleIdx ? shuffleIdx->reshuffled : 0; };

	inline void SetClean()
	{ if (shuffleIdx)  shuffleIdx->reshuffled = false; };

        inline int GetCurrIdx()
	{ return shuffleIdx ? shuffleIdx->currShuffleIdx : -1; };
        inline void SetCurrIdx(int Idx)
	{ if (shuffleIdx) shuffleIdx->currShuffleIdx = Idx; };
	inline cPlayListItem* GetCurrItem()
	{ return shuffleIdx ? shuffleIdx->GetItemByIndex(
                        shuffleIdx->currShuffleIdx) : NULL; };

	inline int GetNIdx()
	{ return shuffleIdx ? shuffleIdx->GetNIdx() : -1; };
	
	inline const char *CurrFile()
        { return shuffleIdx ? shuffleIdx->CurrFile() : NULL; };
        inline const char *NextFile()
        { return shuffleIdx ? shuffleIdx->NextFile() : NULL; };
	inline const char *PrevFile()
        { return shuffleIdx ? shuffleIdx->PrevFile() : NULL; };
	inline const char *NextAlbumFile()
        { return shuffleIdx ? shuffleIdx->NextAlbumFile() : NULL; };
	inline const char *PrevAlbumFile()
        { return shuffleIdx ? shuffleIdx->PrevAlbumFile() : NULL; };
	
        inline cPlayListItem *GetItemByIndex(int Index) {   
                return shuffleIdx ?
                        shuffleIdx->GetItemByIndex(Index) : NULL; 
        };
        
        inline cPlayListItem *GetItemByFilename(const char *Filename) { 
                return shuffleIdx ?
                        shuffleIdx->GetTrackByFilename(Filename) : NULL; 
        };

        inline cPlayListItem *GetAlbumByFilename(const char *Filename) {
                return shuffleIdx ?
                        shuffleIdx->GetAlbumByFilename(Filename) : NULL; 
        };

        inline int GetIndexByItem(const cPlayListItem *Item) {
                return shuffleIdx ? 
                        shuffleIdx->GetIndexByItem(Item) : -1; 
        };

};
        
#endif
