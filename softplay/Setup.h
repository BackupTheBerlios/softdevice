/*
 * softplay.c: A plugin for the Video Disk Recorder
 *
 * See the README file for copyright information and how to reach the author.
 *
 * $Id: Setup.h,v 1.1 2006/03/12 20:23:23 wachm Exp $
 */

#ifndef __SETUP_H__
#define __SETUP_H__

#include "vdr/menuitems.h"
#include "vdr/plugin.h"

class cSoftplaySetup {
        friend class cSoftplaySetupMenu;
        
        private:
                cPlugin *plugin;
                
                int receiver;
                int fileIndex;
		char lastDir[120];
		int openLastDir;
                int returnToLiveTV;
        public:
                cSoftplaySetup() 
                {};
                ~cSoftplaySetup()
                {};
                
                inline void SetPlugin(cPlugin *Plugin) {
                        plugin=Plugin;
                };

		inline void SetLastDir( const char * Dir ) {
			strncpy(lastDir,Dir,sizeof(lastDir));
		};

		inline const char *GetLastDir() {
			return lastDir;
		};			

                inline bool UseReceiver() {
                        return receiver;
                };
                
                inline bool UseFileIndex() {
                        return fileIndex;
                };

		inline bool OpenLastDir() {
			return openLastDir;
		};

                inline bool ReturnToLiveTV() {
                        return returnToLiveTV;
                };

                void Store( void );

                bool Parse( const char *Name, const char *Value);

};

extern cSoftplaySetup SoftplaySetup;

class cSoftplaySetupMenu: public cMenuSetupPage {
        private :
                cSoftplaySetup *data;
                cSoftplaySetup origData;
                
        public:
                cSoftplaySetupMenu( cPlugin *plugin, cSoftplaySetup *Data ) ; 
                
                ~cSoftplaySetupMenu()
                {};

        protected:
                virtual eOSState ProcessKey(eKeys Key);
                
                virtual void Store( void ) {
                        if (data)
                                data->Store();
                };
};
                
#endif //__SETUP_H__
