/*
 * Media Player plugin for VDR
 *
 * Copyright (C) 2005 Martin Wache
 *
 * This code is distributed under the terms and conditions of the
 * GNU GENERAL PUBLIC LICENSE. See the file COPYING for details.
 *
 * $Id: FileIndex.c,v 1.2 2006/04/02 20:19:12 wachm Exp $
 */

#include "FileIndex.h"

//#define INDEXDEB(out...) printf("INDEXDEB: " out)

#ifndef INDEXDEB
#define INDEXDEB(out...)
#endif


//--cIndex------------------------------------------------------------------

void cIndex::copyName(char* dest,int size, const char *const src) {
        strlcpy(dest,src,size);
        stripTrailingNonLetters(dest);
        //printf("copyName to '%s'\n");
};


void cIndex::Save(FILE *out) {
        
        //don't save empty entries (at least for now...)
        if ( !*GetTitle() )
                return;
	
        fprintf(out,"\"%s\",",GetFilename());
	fprintf(out,"\"%s\",",GetTitle());
        fprintf(out,"\"%s\",",GetAlbum());
        fprintf(out,"\"%s\",",GetAuthor());
        fprintf(out,"\"%d\",",GetDuration());
        fprintf(out,"\n");
};

void cIndex::ParseSaveLine(const char *pos ) {
        char str[STR_LENGTH];
 
	skipSpaces(pos);
 	// read title
        if ( !(pos = ReadQuotedString(str,pos) ) ) {
                printf("Could not parse title. Ignoring.\n");
                return;
        };
	SetTitle(str);
	
	nextField(pos);
 	// read album
        if ( !(pos = ReadQuotedString(str,pos) ) ) {
                printf("Could not parse album. Ignoring.\n");
                return ;
        };
	SetAlbum(str);

	nextField(pos);
	// read author
        if ( !(pos = ReadQuotedString(str,pos) ) ) {
                printf("Could not parse author. Ignoring.\n");
                return;
        };
	SetAuthor(str);

	nextField(pos);
	// read duration
        if ( !(pos = ReadQuotedString(str,pos) ) ) {
                printf("Could not parse duration. Ignoring.\n");
                return;
        };
	SetDuration(atoi(str));
};
 
void cIndex::ParseM3uExtInf(const char *pos) {
	char tmp[100];
	int len=0;

        INDEXDEB("Parse extinfo '%s'\n",pos);

	if ( !pos || *pos !='#' )
		return;
	pos++;
	
	skipSpaces(pos);
	if ( strncmp(pos,"EXTINF",6) )
		return;
	pos+=6;

	skipSpaces(pos);
	if ( *pos !=':' )
		return;
	pos++;
	
	skipSpaces(pos);
        //INDEXDEB("duration '%s' \n",pos);
        if ( !*pos || sscanf(pos,"%100[0-9]%n",tmp,&len) == 0 ) {
                printf("EXTINF Could not parse duration \"%s\". Ignoring.\n",pos);
                return;
        };
        duration=atoi(tmp);
        //INDEXDEB("duration parsed '%s'= %d\n",tmp,duration);
        pos=pos + len;

	skipSpaces(pos);
	if ( *pos !=',' )
		return;
	pos++;
	
	skipSpaces(pos);
	if ( !*pos || sscanf(pos,"%100[^-]%n",tmp,&len) == 0 ) {
                printf("EXTINF Could not parse author \"%s\". Ignoring.\n",pos);
                return;
        };
        pos=pos + len;
	SetAuthor(tmp);
	
	skipSpaces(pos);
	if ( *pos !='-' )
		return;
	pos++;
	
	skipSpaces(pos);
	// the rest should be the title
	SetTitle(pos);

	INDEXDEB("Parsed M3U EXTINF: %d '%s' '%s'\n",duration,author,title);
};	

//---cIndexIdx------------------------------------------------------------

cIndexIdx::~cIndexIdx() {
        if (!index)
                       return;

        while ( nIndex-- ) 
                delete index[nIndex].file;

        free(index);
};


cIndex *cIndexIdx::GetIndex(const char *Filename) {
        if (!index || !Filename)
                return NULL;
        
        int Hash=SimpleHash(Filename);
        
        for (int i=0; i < nIndex; i++ ) 
                if ( Hash == index[i].hash && 
                            strcmp(index[i].file->GetFilename(),Filename)==0 )
                        return index[i].file;
        
        
        return NULL;
};

#define MIN_INDEX_SIZE 200

bool cIndexIdx::ReallocIndex() {
        void *tmp;
        
        if ( !(tmp=realloc(index,indexSize+MIN_INDEX_SIZE*
                                        sizeof(sIndexRef))) ) {
                fprintf(stderr,"reallocation of file index failed!\n");
                return false;
        };

        index=(sIndexRef *) tmp;
        indexSize+=MIN_INDEX_SIZE;
        return true;
};

cIndex *cIndexIdx::AddIndex(const char *Filename) {
        if (!Filename)
                return NULL;
        // realloc in case the array is too small
        if ( nIndex >= indexSize && !ReallocIndex() )
                return NULL;
        
        index[nIndex].hash=SimpleHash(Filename);
        index[nIndex].file=new cIndex(Filename);
        nIndex++;

        return index[nIndex-1].file;
};

void cIndexIdx::SaveIndex(const char *Filename) {
	if (!index)
		return;

        FILE *out=fopen(Filename,"w");
        if (!out)
                return;

        for (int i=0; i < nIndex; i++ ) 
                if (index[i].file)
                        index[i].file->Save(out);

        return;
};

bool cIndexIdx::ReadIndex(const char *Filename) {
        char line[500];
        char str[STR_LENGTH];
        const char *pos;
        cIndex *Idx;
        
        FILE *in=fopen(Filename,"r");
        if (!in)
                return false;
                
        while( !feof(in) && fgets(line,500,in)) {
                chomp(line);

                pos=line;
                skipSpaces(pos);
                // read filename
                if ( !(pos = ReadQuotedString(str,pos) ) ) {
                        INDEXDEB("Could not parse filename. Ignoring.\n");
                        continue;
                };
                Idx=GetOrAddIndex(str);
                if (!Idx)
                        return false;
                
                nextField(pos);
                Idx->ParseSaveLine(pos);
        };
        return true;
};

        
