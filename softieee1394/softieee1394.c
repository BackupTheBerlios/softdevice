/*
 * softieee1394.c: A plugin for the Video Disk Recorder
 *
 * See the README file for copyright information and how to reach the author.
 *
 * $Id: softieee1394.c,v 1.2 2005/07/24 20:40:04 lucke Exp $
 */

#include <vdr/plugin.h>
#include <avformat.h>
#include "i18n.h"
#include "avc-handler.h"
#include "../softdevice/softdevice.h"

static const char *VERSION        = "0.0.0";
static const char *DESCRIPTION    = "Plugin for DV firewire devices";
static const char *MAINMENUENTRY  = "Softieee1394";

/* ---------------------------------------------------------------------------
 */
class cMenuDevices : public cOsdMenu
{
  private:
    cAVCHandler *avcHandler;

  public:
    cMenuDevices(cAVCHandler *avcHandler);
    virtual ~cMenuDevices();
    virtual eOSState ProcessKey(eKeys key);

    void    GetDeviceNames(void);
};

/* ---------------------------------------------------------------------------
 */
cMenuDevices::cMenuDevices(cAVCHandler *avcHandler) : cOsdMenu(tr("Devices"))
{
  this->avcHandler = avcHandler;
};

/* ---------------------------------------------------------------------------
 */
cMenuDevices::~cMenuDevices(void) {};

/* ---------------------------------------------------------------------------
 */
void cMenuDevices::GetDeviceNames(void)
{
    int         i = 0;
    const char  *name;

  while ((name = avcHandler->GetDeviceNameAt(i++)))
  {
    Add(new cOsdItem(name,osUnknown),false);
  }
}

eOSState cMenuDevices::ProcessKey(eKeys key)
{
    eOSState  state = cOsdMenu::ProcessKey(key);
    octlet_t  devId;

  if (state == osUnknown)
  {
    switch (key)
    {
      case kOk:
        devId = avcHandler->GetGuidForName(Get(Current())->Text());
        if (devId)
        {
          cControl::Launch(new cAVCControl(devId));
        }
        return osEnd;
      default:
        break;
     }
  }
  return state;
}

/* ---------------------------------------------------------------------------
 */
class cPluginSoftieee1394 : public cPlugin {
private:
  // Add any member variables or functions you may need here.
  cAVCHandler   *avcHandler;

public:
  cPluginSoftieee1394(void);
  virtual ~cPluginSoftieee1394();
  virtual const char *Version(void) { return VERSION; }
  virtual const char *Description(void) { return tr(DESCRIPTION); }
  virtual const char *CommandLineHelp(void);
  virtual bool ProcessArgs(int argc, char *argv[]);
  virtual bool Initialize(void);
  virtual bool Start(void);
  virtual void Stop(void);
  virtual void Housekeeping(void);
  virtual const char *MainMenuEntry(void) { return tr(MAINMENUENTRY); }
  virtual cOsdObject *MainMenuAction(void);
  virtual cMenuSetupPage *SetupMenu(void);
  virtual bool SetupParse(const char *Name, const char *Value);
  };

/* ---------------------------------------------------------------------------
 */
cPluginSoftieee1394::cPluginSoftieee1394(void)
{
  // Initialize any member variables here.
  // DON'T DO ANYTHING ELSE THAT MAY HAVE SIDE EFFECTS, REQUIRE GLOBAL
  // VDR OBJECTS TO EXIST OR PRODUCE ANY OUTPUT!
}

/* ---------------------------------------------------------------------------
 */
cPluginSoftieee1394::~cPluginSoftieee1394()
{
  // Clean up after yourself!
}

/* ---------------------------------------------------------------------------
 */
const char *cPluginSoftieee1394::CommandLineHelp(void)
{
  // Return a string that describes all known command line options.
  return NULL;
}

/* ---------------------------------------------------------------------------
 */
bool cPluginSoftieee1394::ProcessArgs(int argc, char *argv[])
{
  // Implement command line argument processing here if applicable.
  return true;
}

/* ---------------------------------------------------------------------------
 */
bool cPluginSoftieee1394::Initialize(void)
{
  // Initialize any background activities the plugin shall perform.
  avcHandler = new cAVCHandler();
  return true;
}

/* ---------------------------------------------------------------------------
 */
bool cPluginSoftieee1394::Start(void)
{
  // Start any background activities the plugin shall perform.
  RegisterI18n(Phrases);
  return true;
}

/* ---------------------------------------------------------------------------
 */
void cPluginSoftieee1394::Stop(void)
{
  // Stop any background activities the plugin shall perform.
}

/* ---------------------------------------------------------------------------
 */
void cPluginSoftieee1394::Housekeeping(void)
{
  // Perform any cleanup or other regular tasks.
}

/* ---------------------------------------------------------------------------
 */
cOsdObject *cPluginSoftieee1394::MainMenuAction(void)
{
    cMenuDevices *Menu=new cMenuDevices (avcHandler);

  Menu->GetDeviceNames();
  return Menu;
#if 0
  // Perform the action when selected from the main VDR menu.
  cControl::Launch(new cAVCControl((octlet_t) 0x0800460101633dbfLL));
  return NULL;
#endif
}

/* ---------------------------------------------------------------------------
 */
cMenuSetupPage *cPluginSoftieee1394::SetupMenu(void)
{
  // Return a setup menu in case the plugin supports one.
  return NULL;
}

/* ---------------------------------------------------------------------------
 */
bool cPluginSoftieee1394::SetupParse(const char *Name, const char *Value)
{
  // Parse your own setup parameters and store their values.
  return false;
}

VDRPLUGINCREATOR(cPluginSoftieee1394); // Don't touch this!
