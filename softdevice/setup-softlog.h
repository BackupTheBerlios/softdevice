/*
 * Configurable logging and traceing
 *
 * See the README file for copyright information and how to reach the author.
 *
 * $Id: setup-softlog.h,v 1.3 2007/02/24 14:04:14 lucke Exp $
 */

#ifndef __SETUP_SOFTLOG_H
#define __SETUP_SOFTLOG_H

#include <stdio.h>
#include <string.h>
#include <syslog.h>

#ifndef STAND_ALONE
#include <vdr/plugin.h>
#else
#include "VdrReplacements.h"
#endif

#define SOFT_LOG_ERROR   1
#define SOFT_LOG_INFO    2
#define SOFT_LOG_DEBUG   4
#define SOFT_LOG_TRACE   8

#define NO_LOG          (LOG_EMERG - 1)

class cSetupSoftlog {
  private:
    char  *logFileName;
    int   enabledPriorities,
          traceFlags;
    bool  appendPID;
    FILE  *logFile;

    int   IsEnabled(int priority),
          LogPriority(int priority);
    char  Priority2Char(int priority);
    void  CloseLogFile();

  public:
    cSetupSoftlog();
    virtual ~cSetupSoftlog();

    virtual void  Log(int currPriorities, int traceFlags, char *format, ...),
                  DisableLog2File();
    virtual int   SetLogPriorities(int newPriorities),
                  GetLogPriorities(),
                  SetTraceFlags(int newFlags),
                  GetTraceFlags(),
                  SetAppendMode(int newMode),
                  GetAppendMode(),
                  SetLogFile(const char *fileName);
    virtual char  *GetLogFileName();
    virtual bool  Parse(const char *name, const char *value);
};

#endif
