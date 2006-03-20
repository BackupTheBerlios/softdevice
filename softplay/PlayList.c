/*
 * Media Player plugin for VDR
 *
 * Copyright (C) 2005 Martin Wache
 *
 * This code is distributed under the terms and conditions of the
 * GNU GENERAL PUBLIC LICENSE. See the file COPYING for details.
 *
 * $Id: PlayList.c,v 1.14 2006/03/20 20:04:30 wachm Exp $
 */
#include "softplay.h"
#include "PlayList.h"
#include <string.h>
#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>

#include "vdr/player.h"

#include "FileIndex.h"

#define LISTDEB(out...) {printf("LISTDEB: ");printf(out);}

#ifndef LISTDEB
#define LISTDEB(out...)
#endif

// ---cItemIdx-------------------------------------------------------------

cItemIdx::cItemIdx(cPlayList *Owner ) {
        owner = Owner;
	currShuffleIdx=-1;
	nIdx=0;
	Idx=new sIdx[MAX_ITEMS];
	memset(Idx,0,sizeof(sIdx[MAX_ITEMS]));
	nAlbum=0;
	Album=new sIdx[MAX_ITEMS/10];
	memset(Album,0,sizeof(sIdx[MAX_ITEMS/10]));
};

cItemIdx::~cItemIdx() {
        delete Idx;
        delete Album;
};

cPlayList *cItemIdx::RemoveItem(cPlayListItem *Item) {
        cPlayList *album;
        if ( dynamic_cast <cPlayList*>(Item) ) {
                // remove a album 
                for (int i=0; i<nAlbum; i++) 
                        if (Album[i].Item==Item) {
                                album=Album[i].Album;
                                memmove(&Album[i],&Album[i+1],
                                        sizeof(Album[i])*(nAlbum-i));
                                nAlbum--;
                                return album;
                        };

        } else {
                // remove a track
                for (int i=0; i<nIdx; i++) 
                        if (Idx[i].Item==Item) {
                                album=Idx[i].Album;
                                memmove(&Idx[i],&Idx[i+1],
                                        sizeof(Idx[i])*(nIdx-i));
                                nIdx--;
                                if (i<currShuffleIdx)
                                        currShuffleIdx--;
                                return album;
                        };
        };
        return NULL;
};     

void cItemIdx::AddItemAtEnd(cPlayList *album, cPlayListItem *Item) {
        if ( dynamic_cast <cPlayList*>(Item) ) {
                Album[nAlbum].Album=album;
                Album[nAlbum].Item=Item;
                Album[nAlbum].Hash=SimpleHash(Item->GetFilename());
                LISTDEB("Hash %x Filename %s\n",Album[nAlbum].Hash,Item->GetFilename());
                nAlbum++;
        } else {
                Idx[nIdx].Album=album;
                Idx[nIdx].Item=Item;
                Idx[nIdx].Hash=SimpleHash(Item->GetFilename());
                LISTDEB("Hash %x Filename %s\n",
                                Idx[nIdx].Hash,Item->GetFilename());
                nIdx++;
        }
}; 

cPlayListItem *cItemIdx::GetTrackByFilename(const char *Filename) {
	int32_t hash=SimpleHash(Filename);
        
        for ( int i=0; i < nIdx; i++ ) 
                if ( hash==Idx[i].Hash && Idx[i].Item
                        && strcmp(Filename,Idx[i].Item->GetFilename()) ==0)
                        return Idx[i].Item;
        return NULL;
};

cPlayListItem *cItemIdx::GetAlbumByFilename(const char *Filename) {
	int32_t hash=SimpleHash(Filename);
        
        for ( int i=0; i < nAlbum; i++ ) 
                if ( hash==Album[i].Hash && Album[i].Item
                        && strcmp(Filename,Album[i].Item->GetFilename())==0)
                        return Album[i].Item;
        return NULL;
};

int cItemIdx::GetIndexByItem(const cPlayListItem *Item) {
        for ( int i=0; i < nIdx; i++ ) 
                if ( Item == Idx[i].Item )
                        return i;
        return -1;
};

const char * cItemIdx::NextFile() {
        LISTDEB("NextFile currShuffleIdx %d\n",currShuffleIdx);
        int saveCurrShuffleIdx=currShuffleIdx;

        do {
		++currShuffleIdx;
                if ( owner->options.autoRepeat && currShuffleIdx >= nIdx ) {
                        currShuffleIdx=-1;
                        if (owner->options.shuffle)
                                Shuffle();
                        currShuffleIdx=0;
                };
        } while ( currShuffleIdx < nIdx 
                        && !Idx[currShuffleIdx].Item );
	
        LISTDEB("NextFile currShuffleIdx %d nIdx %d\n",currShuffleIdx,nIdx);

        if ( currShuffleIdx > nIdx )
                currShuffleIdx=nIdx;

	if ( Idx[currShuffleIdx].Item )
		return Idx[currShuffleIdx].Item->GetFilename();

        currShuffleIdx=saveCurrShuffleIdx;
	return NULL;
};

const char * cItemIdx::PrevFile() {
        LISTDEB("PrevFile currShuffleIdx %d\n",currShuffleIdx);
        do {
		--currShuffleIdx;
                if (owner->options.autoRepeat && currShuffleIdx<0)
                        currShuffleIdx=nIdx-1;

        } while ( currShuffleIdx > 0
                        && !Idx[currShuffleIdx].Item );
	
        LISTDEB("PrevFile currShuffleIdx %d\n",currShuffleIdx);
 
        if ( currShuffleIdx < 0 )
                currShuffleIdx=0;

	if ( Idx[ currShuffleIdx ].Item)
		return Idx[ currShuffleIdx ].Item->GetFilename();
	return NULL;
};

const char * cItemIdx::NextAlbumFile() {
        cPlayList *currAlbum=Idx[currShuffleIdx].Album;
        LISTDEB("NextAlbumFile currShuIdx %d currAlbum %p\n",
                        currShuffleIdx,currAlbum);
        int saveCurrShuffleIdx=currShuffleIdx;

        do {
		++currShuffleIdx;
                if ( owner->options.autoRepeat && 
                     currShuffleIdx >= nIdx ) {
                        if (owner->options.shuffle)
                                Shuffle();
                        currShuffleIdx=0;
                };
        } while ( currShuffleIdx < nIdx 
                 && ( !Idx[currShuffleIdx].Item 
                    || currAlbum==Idx[currShuffleIdx].Album ) );
	
        LISTDEB("NextAlbumFile currShuffleIdx %d \n",currShuffleIdx);
        
        if ( currShuffleIdx > nIdx )
                currShuffleIdx=nIdx;
        
	if ( Idx[ currShuffleIdx ].Item )
		return Idx[ currShuffleIdx ].Item->GetFilename();

        currShuffleIdx=saveCurrShuffleIdx;
	return NULL;
};

const char * cItemIdx::PrevAlbumFile() {
        LISTDEB("PrevAlbumFile currShuffleIdx %d\n",currShuffleIdx);
        cPlayList *currAlbum=Idx[currShuffleIdx].Album;
        do {
	        --currShuffleIdx;
                if ( owner->options.autoRepeat && currShuffleIdx < 0 )
                        currShuffleIdx = nIdx - 1;
        } while ( currShuffleIdx > 0  
                  && ( !Idx[currShuffleIdx].Item 
                    || currAlbum == Idx[currShuffleIdx].Album ) );
	
        LISTDEB("PrevFile currShuffleIdx %d\n",currShuffleIdx);

        if (currShuffleIdx < 0 )
                currShuffleIdx=0;
        
	if (Idx[currShuffleIdx].Item)
		return Idx[currShuffleIdx].Item->GetFilename();
	return NULL;
};

const char * cItemIdx::CurrFile() {
        LISTDEB("CurrFile currShuffleIdx %d\n",currShuffleIdx);
        
	while ( !Idx[currShuffleIdx].Item && currShuffleIdx < nIdx )
                currShuffleIdx++;
        
        LISTDEB("CurrFile currShuffleIdx %d\n",currShuffleIdx);

	if (Idx[currShuffleIdx].Item)
		return Idx[currShuffleIdx].Item->GetFilename();
	return NULL;
};

void cItemIdx::Shuffle() {
        LISTDEB("Shuffle playlist nIdx %d currShuffleIdx %d\n",
                        nIdx,currShuffleIdx);

        sIdx index;
        for (int i=0; i<2*nIdx ; i++) {
		//FIXME - I guess the ranges are not completly correct
                int xchange1=(int)( (float)(random())*(float)(nIdx-1-currShuffleIdx)/(float)(RAND_MAX))
			+currShuffleIdx+1;
                int xchange2=(int)( (float)(random())*(float)(nIdx-1-currShuffleIdx)/(float)(RAND_MAX))
			+currShuffleIdx+1;
                if (xchange1 >=nIdx)
                        LISTDEB("Martin, depp!! %d\n",xchange1);
                if (xchange2 >=nIdx)
                        LISTDEB("Martin, depp!! %d\n",xchange2);
                
		LISTDEB("Shuffle %4d - %4d \n",xchange1,xchange2);
                index=Idx[xchange1];
                Idx[xchange1]=Idx[xchange2];
                Idx[xchange2]=index;
        };
	reshuffled=true;
/*
	for (int i=0; i<nItems ; i++) 
		LISTDEB("shuffleIdx[%d]: %d\n",i,shuffleIdx->Idx[i]);       
*/
};

void cItemIdx::Move(int From, int To) {
        LISTDEB("Move From %d to %d currIdx %d\n",From,To,currShuffleIdx);
//        for (int i=(To>From?From:To)-1; i < (To>From?To:From) +1; i++ ) 
//                if ( i > 0 && i <  nIdx )
//                        LISTDEB("Item %d '%s'\n",i,Idx[i].Item->GetFilename());

        if ( From == To )
                return;
        
	if ( From >= 0 && From <  nIdx
             && To >= 0 && To <  nIdx ) {
                sIdx dummy=Idx[From];
                if ( From < To ) {
                        memmove(&Idx[From],&Idx[From+1],
                                        sizeof(Idx[To])*(To-From));
                        if ( currShuffleIdx == From)
                                currShuffleIdx = To;
                        else if ( currShuffleIdx > From && 
                                        currShuffleIdx <= To )
                                currShuffleIdx--;
                } else {
                        memmove(&Idx[To+1],&Idx[To],
                                        sizeof(Idx[To])*(From-To));
                        if ( currShuffleIdx == From )
                                currShuffleIdx = To;
                        else if ( currShuffleIdx >= To && 
                                        currShuffleIdx < From )
                                currShuffleIdx++;
                };
                Idx[To]=dummy;
        };
        LISTDEB("After Move From %d to %d currIdx %d\n",From,To,currShuffleIdx);
        for (int i=(To>From?From:To)-1; i < (To>From?To:From) +2; i++ ) 
                if ( i >= 0 && i <  nIdx )
                        LISTDEB("Item %d '%s'\n",i,Idx[i].Item->GetFilename());
};


// ---cPlayListItem--------------------------------------------------------
cPlayListItem::cPlayListItem(const char *Filename) {
        if (Filename) {
                SetFilename(Filename);
        } else filename[0]=0;
        next=previous=NULL;
};

cPlayListItem::~cPlayListItem() {
};

void cPlayListItem::InsertSelfIntoList(cPlayListItem *Next,
                cPlayListItem *Previous) {
        next=Next;
        if (next) 
                next->previous=this;
        previous=Previous;
        if (previous)
                previous->next=this;
};

void cPlayListItem::RemoveSelfFromList() {
        if (next) 
                next->previous=previous;
        if (previous)
                previous->next=next;
};


void cPlayListItem::BuildIdx(cItemIdx *ShuffleIdx) {
};

const char *cPlayListItem::ParseTypeFilename(const char *pos, 
                char *Type, char *Filename) {
        int len;
	if (!*pos)
		return NULL;

        // read entry type
        if ( sscanf(pos,"%20[^:]:%n",Type,&len) == 0 ) {
                LISTDEB("Ignoring invalid line in list file: \"%s\"!\n",pos);
                return NULL;
        };
        pos = &pos[len];


        skipSpaces(pos);
        // read file name (should be always present)
        if ( !(pos = ReadQuotedString(Filename,pos) ) ) {
                LISTDEB("Could not parse filename at pos \"%s\". Ignoring.\n",pos);
                return NULL;
        };

	nextField(pos);
        return pos;
};

void cPlayListItem::Save(FILE *out) {
	fprintf(out,"File: ");
        fprintf(out,"\"%s\",",GetFilename());
};
	
// -----cPlayList------------------------------------------------------------
cPlayList::cPlayList(const char *Filename, cItemIdx *ShuffleIdx) 
                : cPlayListItem( (Filename? Filename : tr("Playlist 1")) ) {
        first=NULL;
        last=NULL;
        options.shuffle=false;
        options.autoRepeat=false;
        
        if (!ShuffleIdx) {
	        shuffleIdx=new cItemIdx(this);
                shuffleIdxOwner=true;
        } else {
                shuffleIdxOwner=false;
                shuffleIdx=ShuffleIdx;
        };
};

cPlayList::~cPlayList() {
        while (first)
                RemoveItem(first);
        if (shuffleIdxOwner) {
                delete shuffleIdx;
                shuffleIdx=NULL;
        };     
};

bool cPlayList::RemoveItemFromList(cPlayListItem *Item) {
        LISTDEB("RemoveItemFromList %p, first %p last %p\n",Item,first,last);

	// is it the first or the last item?
        if (first && Item == first)
                first = first->GetNext();
        if (last && Item == last) 
                last = last->GetPrev();
	// remove from list
        Item->RemoveSelfFromList();
	delete Item;

	return true;
};

void cPlayList::AddItemAtEndToList(cPlayListItem *Item) {
        if (!last) {
                Item->InsertSelfIntoList(NULL,NULL);
                first = last = Item;
        } else {
                Item->InsertSelfIntoList(NULL,last);
                last = Item;
        };
};

void cPlayList::AddItemAtEnd(cPlayListItem *Item) {
        AddItemAtEndToList(Item);
        if (shuffleIdx)
                shuffleIdx->AddItemAtEnd(this,Item);        
};                    

bool cPlayList::RemoveItem(cPlayListItem *Item) { 
        cPlayList *album=NULL;
        if (shuffleIdx )
                album=shuffleIdx->RemoveItem(Item);
        if (album)
                return album->RemoveItemFromList(Item);
        return false; 
};

void cPlayList::BuildIdx(cItemIdx *ShuffleIdx) {
};

bool cPlayList::AddFile(char * filename) {
        LISTDEB("AddFile %s\n",filename);
        cPlayListItem *Item= new cPlayListItem(filename);
        LISTDEB("Item created %p\n",Item);
        AddItemAtEnd(Item);
        LISTDEB("Item added first %p last %p\n",first,last);
        
        return true;
};

bool cPlayList::AddDir(char * dirname, bool recursive) {
        LISTDEB("AddDir %s\n",dirname);
        cPlayList *Item= new cPlayList(dirname,shuffleIdx);
        Item->ScanDir(dirname,recursive);
        AddItemAtEnd(Item);

        return true;
};

bool cPlayList::AddM3U(char * Filename) {
        LISTDEB("AddM3U %s\n",Filename);
        cPlayList *Item= new cPlayList(Filename,shuffleIdx);
        Item->LoadM3U(Filename);
        AddItemAtEnd(Item);

        return true;
};

cPlayListItem *cPlayList::GetItem(int No) {
        int count=0;
        cPlayListItem *Item=first;
        while ( Item && count!=No ) {
                count++;
                Item=Item->GetNext();
        };
        return Item;
};
  
void cPlayList::PrepareForPlayback() {
	LISTDEB("PrepareForPlayback\n");
	shuffleIdx->currShuffleIdx=-1;

        BuildIdx();
        if (options.shuffle)
                shuffleIdx->Shuffle();
};

void cPlayList::SetOptions( sPlayListOptions &Options) {

        if (Options.shuffle != options.shuffle 
                        && Options.shuffle)
                shuffleIdx->Shuffle();

        options=Options;
        SetFilename(Options.name);
};

void cPlayList::GetOptions( sPlayListOptions &Options) {
        Options=options;
        strncpy(Options.name,GetFilename(),40);
        Options.name[40-1]=0;
};
        
bool cPlayList::ScanDir(char * dirname, bool recursive) {
        struct dirent **namelist;
        int n;
        char Name[STR_LENGTH];

        LISTDEB("ScanDir %s recursive %d\n",dirname,recursive);
        
        n = scandir(dirname, &namelist, 0, alphasort);
        if (n<0) {
                printf("scandir error\n");
                return false;
        };

        for (int i=0; i<n; i++) {
                if ( !strcmp("..",namelist[i]->d_name) ||
                     !strcmp(".",namelist[i]->d_name) ) {
                        LISTDEB("ignore %s\n",namelist[i]->d_name);
                        continue;
                };

                bool ret=true;
                snprintf(Name,STR_LENGTH,"%s/%s",
                                dirname,namelist[i]->d_name);
                Name[STR_LENGTH-1]=0;

                // check type (non ext2/3 and symlinks)
                if ( namelist[i]->d_type == 0 || 
                                namelist[i]->d_type == DT_LNK ) {
                        struct stat stbuf;
                        if ( !stat(Name,&stbuf) ) {
                                if ( S_ISDIR(stbuf.st_mode) ) 
                                        namelist[i]->d_type = DT_DIR;
                                else if ( S_ISREG(stbuf.st_mode) )
                                        namelist[i]->d_type = DT_REG;
                        };
                };

                if (namelist[i]->d_type == DT_DIR && recursive )  
                        ret=AddDir(Name,recursive);
                else  ret=AddFile(Name);

                free(namelist[i]);	  
        }
        free(namelist);
        return true;
};

void cPlayList::Save(FILE *out) {
        cPlayListItem *Item;
        
        fprintf(out,"Start_List: ");
        fprintf(out,"\"%s\",",GetFilename());
        fprintf(out,"\n");
        
        Item=first;
        while (Item) {
                Item->Save(out);fprintf(out,"\n");
                Item=Item->GetNext();
        };

        fprintf(out,"End_List: ");
        fprintf(out,"\"%s\",",GetFilename());
        //fprintf(out,"\n");
};
        
      
int cPlayList::Load(FILE *in, const char *StartLine) {
        char line[500];
        const char *pos;
        char Type[STR_LENGTH];
        char Filename[STR_LENGTH];
       
        if (StartLine) {
                if ( !(pos=ParseTypeFilename(StartLine,Type,Filename)) ) {
                        LISTDEB("Could not parse Startline. Returning. \n");
                        return -1;
                };

                if ( strcmp(Type,"Start_List")!=0 ) {
                        LISTDEB("Startline not a List startline! Returning.\n");
                        return -1;
                };
                
                SetFilename(Filename);
        };              
                
        while( !feof(in) && fgets(line,500,in)) {
                chomp(line);

                LISTDEB("read line '%s' \n",line); 

                if ( !(pos=ParseTypeFilename(line,Type,Filename)) )
                        continue;
                       
                LISTDEB("filename \"%s\"\n",Filename); 
                if ( !strcmp(Type,"Start_List") ) {
                        LISTDEB("Found start of list\n");
                        cPlayList *Item = new cPlayList(Filename,shuffleIdx);
                        if ( Item->Load(in,line)<0 )
                                return -1;

                        AddItemAtEnd(Item);
                } else if ( !strcmp(Type,"End_List") ) {
                        LISTDEB("Found end of list \n");
                        if ( strcmp(Filename,GetFilename()) ) { 
                                LISTDEB("End of list doesn't match! Returning!\n");
                                return -1; 
                        };
                        return 0;
                } else if ( !strcmp(Type,"File") ) {
                        LISTDEB("Found file \n");
                        cPlayListItem *Item = new cPlayListItem(Filename);
                        Item->ParseSaveLine(pos);
                        AddItemAtEnd(Item);
                } else {
                        LISTDEB("Unknown type \"%s\"\n",Type);
                };
        };
        return 0;
};

int cPlayList::LoadM3U(const char *Filename) {
        FILE *list;
        char line[2][500];
        char dir[STR_LENGTH];
	int swapLine=0;
	line[0][0]=line[1][0]=0;
	
	LISTDEB("LoadM3U: %s\n",Filename);
        
        list = fopen( Filename, "r");
        
        if (!list) {
                LISTDEB("Could not open .m3u file!\n");
                return -1;
        };
        
        SetFilename(Filename);
        char *ncopy=rindex(Filename,'/');
        int count=(int) (ncopy-Filename);
        if ( ncopy && count > STR_LENGTH) {
                fprintf(stderr,"Could not extract directory!\n");
                return -1;
        };
        strncpy(dir,Filename,count);
        dir[count]=0;
        LISTDEB("dir: %s\n",dir);

	
        while ( fgets(line[swapLine],500,list) ) {
                chomp(line[swapLine]);
                LISTDEB("read line \"%s\"\n",line[swapLine]);
                char *pos=line[swapLine];
                
                
		//skip leading white spaces
		//skipSpaces(pos);
		while ( *pos==' ' )
                        pos++;

		if ( *pos=='#' ) {
			// either a comment or EXTINF
			swapLine=!swapLine;
                        continue;
		};

                char ItemFile[STR_LENGTH];
                if ( IsStream(pos) ) {
                        snprintf(ItemFile,STR_LENGTH,"%s",pos);
                } else {
                        // regular file
                        // poor man's windows to linux filename transformation
                        char *tmp=pos;
                        while ( (tmp=index(tmp,'\\')) )
                                *tmp='/';

                        // don't copy leading "./"
                        if (pos[0]=='.' && pos[1]=='/')
                                pos+=2;

                        snprintf(ItemFile,STR_LENGTH,"%s/%s",dir,pos);
                };
                
		// check for extinfos...
		char *extInfoPos=line[!swapLine];
		skipSpaces((const char *)extInfoPos);
		if ( *extInfoPos=='#' && FileIndex ) {
			cIndex *Idx=FileIndex->GetOrAddIndex(ItemFile);
			if (Idx)
				Idx->ParseM3uExtInf(extInfoPos);
		};

                AddItemAtEnd(new cPlayListItem(ItemFile));
		swapLine=!swapLine;
        };

        return 0;
};

