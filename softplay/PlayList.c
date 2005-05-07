/*
 * Media Player plugin for VDR
 *
 * Copyright (C) 2005 Martin Wache
 *
 * This code is distributed under the terms and conditions of the
 * GNU GENERAL PUBLIC LICENSE. See the file COPYING for details.
 *
 * $Id: PlayList.c,v 1.1 2005/05/07 20:05:42 wachm Exp $
 */
#include "PlayList.h"
#include <string.h>
#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>

#include "vdr/player.h"

#define LISTDEB(out...) {printf("LISTDEB: ");printf(out);}

#ifndef LISTDEB
#define LISTDEB(out...)
#endif

// ----cEditList------------------------------------------------------------
cPlayList::cEditList::cEditList(cPlayList * List):cOsdMenu(List->ListName) {
        playList=List;
        for (int i=0; i<playList->nItems;i++)

                Add(new cOsdItem(
                   playList->GetItem(i)->Title,
                   osUnknown),false);
        SetHelp("Hide");
        lastActivity=time(NULL);
        SetCurrent(Get(playList->currItem));
};

cPlayList::cEditList::~cEditList() {
};

void cPlayList::cEditList::UpdateStatus() {
        int current;
        int total;
        char Status[60];
        
        if (!cControl::Control() || !cControl::Control()->GetIndex(current,total))
                return;
        
        snprintf(Status,60,"Time %02d:%02d:%02d/%02d:%02d:%02d Title %d/%d",
                        current/3600,current/60%60,current%60,
                        total/3600,total/60%60,total%60,
                        playList->currItem+1,playList->nItems);
        SetStatus(Status);
};

eOSState cPlayList::cEditList::ProcessKey(eKeys Key) {
        eOSState state = cOsdMenu::ProcessKey(Key);


        if (Key!=kNone) 
                lastActivity=time(NULL);

        if (Current() != playList->currItem && 
                        time(NULL) - lastActivity > 120 ) {
                LISTDEB("SetCurrent current title %d \n",playList->currItem);
                SetCurrent(Get(playList->currItem));
                Display();
        };

        switch (state) {
                default: switch (Key) {
                                 case kOk:
                                         // skip to current track
                                         playList->currItem = Current()-1;
                                         state = osUser3;
                                         break;
                                 case kRed:
                                 case kBack:
                                         state= osEnd;
                                         break;

                                 default:    
                                         break;
                         }
        }
        UpdateStatus();
        return state;
}

// -----cPlayList------------------------------------------------------------
cPlayList::cPlayList() {
        nItems=0;
        currItem=-1;
        strncpy(ListName,"Playlist 1",STR_LENGTH);
        ListName[STR_LENGTH-1]=0;
        memset(shuffleIdx,0,sizeof(shuffleIdx));
};

cPlayList::~cPlayList() {
};

bool cPlayList::AddFile(char * filename, char *title) {
        LISTDEB("AddFile %s\n",filename);
        if ( nItems >= MAX_ITEMS ) 
                return false;
        strncpy(Items[nItems].Filename,filename,STR_LENGTH);
        Items[nItems].Filename[STR_LENGTH-1]=0;
        if (title) {
                strncpy(Items[nItems].Title,title,STR_LENGTH);
                Items[nItems].Title[STR_LENGTH-1]=0;
        };
        shuffleIdx[nItems]=nItems;
        nItems++;
        return true;
};

bool cPlayList::AddDir(char * dirname, bool recursive) {
        struct dirent **namelist;
        int n;
        char Name[STR_LENGTH];

        LISTDEB("AddDir %s recursive %d\n",dirname,recursive);
        
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
                if ( namelist[i]->d_type == 0 || namelist[i]->d_type == DT_LNK ) {
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
                else  ret=AddFile(Name,namelist[i]->d_name);

                free(namelist[i]);	  
        }
        free(namelist);
        return true;
};

char * cPlayList::NextFile() {
        currItem++;
        if ( currItem >= nItems ) {
                return NULL;
                //if (repeat )
                //        currItem=0;
        };
       
        return Items[shuffleIdx[currItem]].Filename;
};
       
void cPlayList::Shuffle() {
        LISTDEB("Shuffle playlist\n");
        for (int i=0; i<nItems ; i++) {
                Items[i].Played=0;
        };

        int index=0;
        for (int i=0; i<nItems ; i++) {
                int add=(int)( random()*(float)(nItems-i)/RAND_MAX)+1;
                // skip add unplayed songs...
                LISTDEB("shuffleIdx %d add %d\n",i,add);
                while (add) {
                        do {
                               index=(index+1)%nItems;
                        } while (Items[index].Played) ;
                        add--;
                };
                shuffleIdx[i]=index;
                LISTDEB("shuffleidx %d index %d\n",i,index);
                Items[index].Played=1;
                
        };
};

cPlayList::ListItem * cPlayList::GetItem(int index) {
        printf("GetItem %d shuffle %d\n",index,shuffleIdx[index]);fflush(stdout);
       // if (shuffleIdx[index]<nItems)
                return &Items[shuffleIdx[index]];
        //else return &Items[0];
};

