/*
 * setup-softdevice-menu.h
 *
 * See the README file for copyright information and how to reach the authors.
 *
 * $Id: setup-softdevice-menu.h,v 1.2 2007/02/10 00:02:14 lucke Exp $
 */

#ifndef __SETUP_SOFTDEVICE_MENU_H
#define __SETUP_SOFTDEVICE_MENU_H

#include "setup-softdevice.h"

/* ---------------------------------------------------------------------------
 */
class cMenuSetupVideoParm : public cOsdMenu
{
  private:
    cSetupStore *data, copyData;

  protected:
    virtual eOSState ProcessKey(eKeys Key);

  public:
    cMenuSetupVideoParm(const char *name);
};

/* ---------------------------------------------------------------------------
 */
class cMenuSetupCropping : public cOsdMenu
{
  private:
    cSetupStore *data, copyData;

  protected:
    virtual eOSState ProcessKey(eKeys Key);

  public:
    cMenuSetupCropping(const char *name);
};

/* ---------------------------------------------------------------------------
 */
class cMenuSetupPostproc : public cOsdMenu
{
  private:
    cSetupStore *data, copyData;

  protected:
    virtual eOSState ProcessKey(eKeys Key);

  public:
    cMenuSetupPostproc(const char *name);
};

/* ---------------------------------------------------------------------------
 */
class cMenuSetupSoftdevice : public cMenuSetupPage {
  private:
    cSetupStore *data, copyData;
    cPlugin     *plugin;

  protected:
    virtual eOSState ProcessKey(eKeys Key);
    virtual void Store(void);

  public:
    cMenuSetupSoftdevice(cPlugin *plugin = NULL);
};
#endif

