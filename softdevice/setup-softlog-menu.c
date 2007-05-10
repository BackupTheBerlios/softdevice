/*
 * See the README file for copyright information and how to reach the authors.
 *
 * $Id: setup-softlog-menu.c,v 1.4 2007/05/10 19:54:44 wachm Exp $
 */


#include "setup-softdevice-menu.h"
#include "setup-softlog-menu.h"

/* ---------------------------------------------------------------------------
 */
cMenuSetupSoftlog::cMenuSetupSoftlog(cPlugin *plugin, const char *name)
{
  if (plugin)
    SetPlugin(plugin);

  strncpy(newLogFileName, softlog->GetLogFileName(), 256);
  newLogPriorities = softlog->GetLogPriorities();
  newAppendMode = softlog->GetAppendMode();

  Add(new cMenuEditBitItem(tr("Info Messages"),
                           (uint *) &newLogPriorities,
                           SOFT_LOG_INFO));
  Add(new cMenuEditBitItem(tr("Debug Messages"),
                           (uint *) &newLogPriorities,
                           SOFT_LOG_DEBUG));
  Add(new cMenuEditBitItem(tr("Trace Messages"),
                           (uint *) &newLogPriorities,
                           SOFT_LOG_TRACE));

#if VDRVERSNUM >= 10334
  Add(new cOsdItem(" ", osUnknown, false));
#else
  Add(new cOsdItem(" ", osUnknown));
#endif

  Add(new cMenuEditStrItem(tr("Logfile"), newLogFileName, 256, tr(FileNameChars)));
  Add(new cMenuEditBoolItem(tr("Append PID"), &newAppendMode));
}

/* ---------------------------------------------------------------------------
 */
cMenuSetupSoftlog::~cMenuSetupSoftlog()
{
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
  if (strcmp(newLogFileName, softlog->GetLogFileName()))
    softlog->SetLogFile(newLogFileName);
  SetupStore ("softlog-file",       softlog->GetLogFileName());

  softlog->SetAppendMode(newAppendMode);
  SetupStore ("softlog-appendpid",  softlog->GetAppendMode());

  if (newLogPriorities != softlog->GetLogPriorities())
    softlog->SetLogPriorities(newLogPriorities);
  SetupStore ("softlog-priorities", softlog->GetLogPriorities());

}
