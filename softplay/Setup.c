/*
 * softplay.c: A plugin for the Video Disk Recorder
 *
 * See the README file for copyright information and how to reach the author.
 *
 * $Id: Setup.c,v 1.1 2006/03/12 20:23:23 wachm Exp $
 */

#include "Setup.h"

#define SETUPDBG(out...) {printf("[Setup-Softplay] ");printf(out);}

#ifndef SETUPDBG
#define SETUPDBG(out...)
#endif

static inline int clamp (int min, int val, int max)
{
  if (val < min)
    return min;
  if (val > max)
    return max;
  return val;
}

//-- cSetup ------------------------------------------------------------

cSoftplaySetup SoftplaySetup;

void cSoftplaySetup::Store() {
        if (!plugin) {
                printf("[Setup-Softplay] can't save values, no plugin!\n");
                return;
        };
        
        plugin->SetupStore ("UseReceiver", receiver);
        plugin->SetupStore ("UseFileIndex", fileIndex);
	plugin->SetupStore ("LastBrowsedDir", lastDir);
	plugin->SetupStore ("openLastDir", openLastDir);
};        

bool cSoftplaySetup::Parse( const char *Name, const char *Value) {
        if ( !Value || !Name )
                return false;
        
        if ( strcasecmp(Name,"UseReceiver") == 0 ) {
                receiver=clamp( 0, atoi(Value), 1 );
                SETUPDBG("set live-TV while audio to %d\n",receiver);
                return true;
        } else if ( strcasecmp(Name,"UseFileIndex") == 0 ) {
                fileIndex=clamp(0, atoi(Value), 1 );
                SETUPDBG("set use file index to %d\n",fileIndex);
                return true;
        } else if ( strcasecmp(Name,"LastBrowsedDir") == 0) {
		strncpy(lastDir,Value,sizeof(lastDir));
		SETUPDBG("set lastDir to '%s'\n",lastDir);
		return true;
	} else if ( strcasecmp(Name,"openLastDir") == 0) {
		openLastDir=clamp(0,atoi(Value), 1);
                SETUPDBG("set openLastDir to %d\n",openLastDir);
		return true;
        } else if ( strcasecmp(Name,"returnToLiveTV") == 0) {
		returnToLiveTV=clamp(0,atoi(Value), 1);
                SETUPDBG("set returnToLiveTV to %d\n",returnToLiveTV);
		return true;
	} else return false;
};
                
//-- cSoftplaySetupMenu-------------------------------------------------

cSoftplaySetupMenu::cSoftplaySetupMenu( cPlugin *plugin, cSoftplaySetup *Data ) 
        : data(Data), origData(*Data) 
{ 
        SetPlugin(plugin);
          
        Add(new cMenuEditBoolItem(tr("Use Fileindex"),
                            (int*)&data->fileIndex, tr("no"), tr("yes")));

        Add(new cMenuEditBoolItem(tr("Watch live TV while audio replay"),
                            (int*)&data->receiver, tr("no"), tr("yes")));

        Add(new cMenuEditBoolItem(tr("Remember last directory"),
                            (int*)&data->openLastDir, tr("no"), tr("yes")));

        Add(new cMenuEditBoolItem(tr("return to live TV after replay"),
                            (int*)&data->returnToLiveTV, tr("no"), tr("yes")));

};

eOSState cSoftplaySetupMenu::ProcessKey(eKeys Key) {
        eOSState state = cOsdMenu::ProcessKey(Key);

        if ( state != osUnknown )
                return state;
        
        if ( Key == kOk ) {
                data->Store();
                state = osBack;
                return state;
        } else if ( Key == kBack ) {
                // restore all changed settings
                *data = origData;
                state = osBack;
                return state;
        }
        return state;
}

