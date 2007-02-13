/*
 * Configurable logging and traceing
 *
 * See the README file for copyright information and how to reach the author.
 *
 * $Id: setup-softlog.c,v 1.3 2007/02/13 20:03:59 lucke Exp $
 */

#include "setup-softlog.h"

#include <stdarg.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/syscall.h>
#include <sys/time.h>

#define MAXSYSLOGBUF 256

/* ---------------------------------------------------------------------------
 */
cSetupSoftlog::cSetupSoftlog()
{
  logFile = stderr;
  logFileName = strdup("stderr");
  appendPID = true;
  enabledPriorities = SOFT_LOG_ERROR | SOFT_LOG_INFO | SOFT_LOG_DEBUG;
  traceFlags = 0;
}

/* ---------------------------------------------------------------------------
 */
cSetupSoftlog::~cSetupSoftlog()
{
}

/* ---------------------------------------------------------------------------
 */
int cSetupSoftlog::IsEnabled(int priority)
{
  return priority & enabledPriorities;
}

/* ---------------------------------------------------------------------------
 */
int cSetupSoftlog::LogPriority(int priority)
{
  // no syslog loging for trace messages
  if (priority & SOFT_LOG_TRACE)
    return NO_LOG;

  if (IsEnabled(priority) & SOFT_LOG_ERROR)
    return LOG_ERR;
  if (IsEnabled(priority) & SOFT_LOG_INFO)
    return LOG_INFO;
  if (IsEnabled(priority) & SOFT_LOG_DEBUG)
    return LOG_DEBUG;

  // unknown logtype -> return invalid priority
  return NO_LOG;
}

/* ---------------------------------------------------------------------------
 */
void cSetupSoftlog::CloseLogFile()
{
  if (logFile && logFile != stderr && logFile != stdout)
    fclose(logFile);

  free(logFileName);
  logFileName = NULL;
}

/* ---------------------------------------------------------------------------
 */
int cSetupSoftlog::SetLogPriorities(int newPriorities)
{
    int oldenabledPriorities = enabledPriorities;

  /* -------------------------------------------------------------------------
   * set new priority flags, but error messages are allways enabled
   */
  enabledPriorities = (newPriorities | SOFT_LOG_ERROR);
  return oldenabledPriorities;
}

/* ---------------------------------------------------------------------------
 */
int cSetupSoftlog::GetLogPriorities()
{
  return enabledPriorities;
}

/* ---------------------------------------------------------------------------
 */
int cSetupSoftlog::SetTraceFlags(int newFlags)
{
    int oldTraceFlags = traceFlags;

  traceFlags = newFlags;
  return oldTraceFlags;
}

/* ---------------------------------------------------------------------------
 */
int cSetupSoftlog::GetTraceFlags()
{
  return traceFlags;
}

/* ---------------------------------------------------------------------------
 */
int cSetupSoftlog::SetAppendMode(int newMode)
{
    int oldMode = appendPID;

  if (newMode != appendPID) {
    appendPID = newMode;
    SetLogFile(logFileName);
  }

  return oldMode;
}

/* ---------------------------------------------------------------------------
 */
int cSetupSoftlog::GetAppendMode()
{
  return appendPID;
}

/* ---------------------------------------------------------------------------
 */
int cSetupSoftlog::SetLogFile(const char *fileName)
{
  /* -------------------------------------------------------------------------
   * Don't close stderr. Handle fileName "stderr" special so that it is
   * possible to switch back from file logging to stderr.
   */
  CloseLogFile();
  if (!strcmp (fileName, "stderr")) {
    logFile = stderr;
  } else if (!strcmp (fileName, "stdout")) {
    logFile = stdout;
  } else {
      char realName [256];

    if (appendPID)
      snprintf (realName, sizeof(realName), "%s-%d", fileName, getpid());
    else
      snprintf (realName, sizeof(realName), "%s", fileName);

    if (!(logFile = fopen (realName, "w+"))) {
      syslog(LOG_ERR,
             "[softlog] could not open logfile (%s), using stderr",
             realName);
      fprintf(stderr,
              "[softlog] could not open logfile (%s), using stderr",
              realName);
      logFile = stderr;
      logFileName = strdup("stderr");
      return 0;
    }
  }

  logFileName = strdup(fileName);
  return 1;
}

/* ---------------------------------------------------------------------------
 */
char *cSetupSoftlog::GetLogFileName()
{
  return logFileName;
}

/* ---------------------------------------------------------------------------
 */
void cSetupSoftlog::DisableLog2File()
{
  CloseLogFile();
  logFile = NULL;
}

/* ---------------------------------------------------------------------------
 */
void cSetupSoftlog::Log(int currPriority, int traceFlags, char *format, ...)
{
    va_list argList;
    int     priority;
    char    fmt[MAXSYSLOGBUF];

  if (!IsEnabled(currPriority))
    return;

  va_start(argList, format);
  priority = LogPriority(currPriority);
  snprintf(fmt, sizeof(fmt), "[%ld] %s", syscall(__NR_gettid), format);

  if (priority != NO_LOG)
    vsyslog(priority, fmt, argList);

  if (logFile) {
      struct timeval now;
      struct tm *tmp;

    gettimeofday(&now, NULL);
    tmp = localtime(&now.tv_sec);
    snprintf(fmt, sizeof(fmt), "%02d:%02d:%02d.%04d [%ld] %s",
             tmp->tm_hour, tmp->tm_min, tmp->tm_sec, (int) now.tv_usec/1000,
             syscall(__NR_gettid), format);
    vfprintf(logFile, fmt, argList);
    fflush(logFile);
  }

  va_end(argList);
}

/* ---------------------------------------------------------------------------
 */
bool cSetupSoftlog::Parse(const char *name, const char *value)
{
  if (!strcasecmp(name,"softlog-priorities"))
    SetLogPriorities(atoi(value));
  else if (!strcasecmp(name,"softlog-file"))
    SetLogFile(value);
  else if (!strcasecmp(name,"softlog-appendpid"))
    SetAppendMode(atoi(value));
  else
    return false;

  return true;
}
