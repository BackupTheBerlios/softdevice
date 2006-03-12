/*
 * Media Player plugin for VDR
 *
 * Copyright (C) 2005 Martin Wache
 *
 * This code is distributed under the terms and conditions of the
 * GNU GENERAL PUBLIC LICENSE. See the file COPYING for details.
 *
 * $Id: FileIndex.h,v 1.1 2006/03/12 20:23:23 wachm Exp $
 */

#ifndef __FILEINDEX_H__
#define __FILEINDEX_H__

#include <string.h>

#include "softplay.h"

class cIndex {
        protected:
                char filename[STR_LENGTH];
                char title[STR_LENGTH];
                char album[STR_LENGTH];
                char author[STR_LENGTH];

                int duration;

                void copyName(char* dest,int size, const char *const src);
        public:
                cIndex(const char *Filename=NULL) {
                        if (Filename)
                                copyName(filename,sizeof(filename),Filename);
                        else filename[0]=0;
                                
                        title[0]=album[0]=author[0]=0;
                        duration=0;
                };
                virtual ~cIndex() {};
  
  		inline const char *GetFilename() 
                { return filename; };

                
                inline const char *GetTitle() 
                { return title; };

                inline void SetTitle( const char *const Title) { 
                        copyName(title,sizeof(title),Title);
                };

                inline const char *GetAlbum()
                { return album; };

                inline void SetAlbum( const char *const Album) {
                        copyName(album,sizeof(album),Album);
                };

                inline const char *GetAuthor()
                { return author; };

                inline void SetAuthor( const char *const Author) {
                        copyName(author,sizeof(author),Author);
                };

                inline const int GetDuration()
                { return duration; };

                inline void SetDuration(int Duration) {
                        duration=Duration;
                };

                void Save(FILE *out);
                
                void ParseSaveLine(const char *pos );
                void ParseM3uExtInf(const char *pos);
};

struct sIndexRef {
        int hash;
        cIndex *file;
};

class cIndexIdx {
        protected:
                sIndexRef *index;
                int nIndex;
                int indexSize;
        
		bool ReallocIndex();
        public:
                cIndexIdx() : index(NULL),nIndex(0),indexSize(0)
                {};
                   
                virtual ~cIndexIdx();

		cIndex *GetIndex(const char *Filename);

		cIndex *AddIndex(const char *Filename);

		inline cIndex *GetOrAddIndex(const char *Filename) {
                        cIndex *Idx=GetIndex(Filename);
                        return Idx ? Idx : AddIndex(Filename);
                };

                void SaveIndex(const char *File);

                bool ReadIndex(const char *File);
};     

#endif //__FILEINDEX_H__

