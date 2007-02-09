/*
 * See the README file for copyright information and how to reach the authors.
 *
 * $Id: setup-softlog-menu.c,v 1.1 2007/02/09 23:46:47 lucke Exp $
 */


#include "setup-softdevice-menu.h"
#include "setup-softlog-menu.h"

/* ---------------------------------------------------------------------------
 */
cMenuSetupSoftlog::cMenuSetupSoftlog(cPlugin *plugin, const char *name)
{
  if (plugin)
    SetPlugin(plugin);

  newLogFileName = strdup(setupStore.softlog->GetLogFileName());
  newLogPriorities = setupStore.softlog->GetLogPriorities();

#if VDRVERSNUM >= 10334
  Add(new cOsdItem(" ", osUnknown, false));
#else
  Add(new cOsdItem(" ", osUnknown));
#endif

  Add(new cMenuEditStrItem(tr("Logfile"), newLogFileName, 64, NULL));
}

/* ---------------------------------------------------------------------------
 */
cMenuSetupSoftlog::~cMenuSetupSoftlog()
{
  free(newLogFileName);
  newLogFileName = NULL;
}

/* ---------------------------------------------------------------------------
 */
eOSState cMenuSetupSoftlog::ProcessKey(eKeys Key)
{
    eOSState state = cOsdMenu::ProcessKey(Key);

  switch (state)
  {
    case osUnknown:
      switch (Key)
      {
        case kOk:
          Store();
          state = osBack;
          break;
        default:
          break;
      }
      break;
    case osBack:
      break;
    default:
      break;
  }
  return state;
}

/* ---------------------------------------------------------------------------
 */
void cMenuSetupSoftlog::Store(void)
{
  if (strcmp(newLogFileName, setupStore.softlog->GetLogFileName()))
    setupStore.softlog->SetLogFile(newLogFileName);
  SetupStore ("softlog-file",       setupStore.softlog->GetLogFileName());

  if (newLogPriorities != setupStore.softlog->GetLogPriorities())
    setupStore.softlog->SetLogPriorities(newLogPriorities);
  SetupStore ("softlog-priorities", setupStore.softlog->GetLogPriorities());

}
