/*
 * See the README file for copyright information and how to reach the authors.
 *
 * $Id: setup-softlog-menu.h,v 1.1 2007/02/09 23:46:47 lucke Exp $
 */

#ifndef __SETUP_SOFTLOG_MENU_H
#define __SETUP_SOFTLOG_MENU_H

#ifndef STAND_ALONE
#include <vdr/menu.h>
#include <vdr/plugin.h>
#include <vdr/i18n.h>
#else
#include "VdrReplacements.h"
#endif

#include "setup-softlog.h"

/* ---------------------------------------------------------------------------
 */
class cMenuSetupSoftlog : public cMenuSetupPage {
  private:
    char  *newLogFileName;
    int   newLogPriorities;

  protected:
    virtual eOSState ProcessKey(eKeys Key);
    virtual void Store(void);

  public:
    cMenuSetupSoftlog(cPlugin *plugin = NULL, const char *name = NULL);
    virtual ~cMenuSetupSoftlog();
};
#endif

