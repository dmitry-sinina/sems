/*
 * Copyright (C) 2002-2003 Fhg Fokus
 *
 * This file is part of SEMS, a free SIP media server.
 *
 * SEMS is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version. This program is released under
 * the GPL with the additional exemption that compiling, linking,
 * and/or using OpenSSL is allowed.
 *
 * For a license to use the SEMS software under conditions
 * other than those described here, or to purchase support for this
 * software, please contact iptel.org by e-mail at the following addresses:
 *    info@iptel.org
 *
 * SEMS is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include <ctype.h>
#include <stdio.h>
#include <unistd.h>
#include <algorithm>

#ifndef DISABLE_SYSLOG_LOG
# include <syslog.h>
#endif

#include <vector>
#include <string>
#include <utility>

#include "AmApi.h"	/* AmLoggingFacility */
#include "AmThread.h"   /* AmMutex */
#include "log.h"

__thread pid_t _self_tid = 0;
__thread pid_t _self_pid = 0;

int log_level  = AmConfig::LogLevel;	/**< log level */

/** Map log levels to text labels */
const char* log_level2str[] = { "ERROR", "WARNING", "INFO", "DEBUG" };

/** Registered logging hooks */
static vector<AmLoggingFacility*> log_hooks;
static AmMutex log_hooks_mutex;

#ifndef DISABLE_SYSLOG_LOG

/**
 * Syslog Logging Facility (built-in plug-in)
 */
class SyslogLogFac : public AmLoggingFacility {
  int facility;		/**< syslog facility */

  void init() {
    openlog(SEMS_APP_NAME, LOG_PID | LOG_CONS, facility);
    setlogmask(-1);
  }

 static SyslogLogFac *_instance;

 public:
  SyslogLogFac()
    : AmLoggingFacility("syslog",AmConfig::LogLevel),
      facility(LOG_DAEMON)
  {
    init();
  }

  ~SyslogLogFac() {
    closelog();
  }

  int onLoad() {
    /* unused (because it is a built-in plug-in */
    return 0;
  }

  bool setFacility(const char* str);
  void log(int, pid_t, pid_t, const char*, const char*, int, char*);

  static SyslogLogFac &instance(){
	  if(!_instance) _instance = new SyslogLogFac();
	  return *_instance;
  }

};
SyslogLogFac *SyslogLogFac::_instance = NULL;

/** Set syslog facility */
bool SyslogLogFac::setFacility(const char* str) {
  static int local_fac[] = {
    LOG_LOCAL0, LOG_LOCAL1, LOG_LOCAL2, LOG_LOCAL3,
    LOG_LOCAL4, LOG_LOCAL5, LOG_LOCAL6, LOG_LOCAL7,
  };

  int new_facility = -1;

  if (!strcmp(str, "DAEMON")){
    new_facility = LOG_DAEMON;
  }
  else if (!strcmp(str, "USER")) {
    new_facility = LOG_USER;
  }
  else if (strlen(str) == 6 && !strncmp(str, "LOCAL", 5) &&
           isdigit(str[5]) && str[5] - '0' < 8) {
    new_facility = local_fac[str[5] - '0'];
  }
  else {
    ERROR("unknown syslog facility '%s'\n", str);
    return false;
  }

  if (new_facility != facility) {
    facility = new_facility;
    closelog();
    init();
  }

  return true;
}

void SyslogLogFac::log(int level, pid_t pid, pid_t tid, const char* func, const char* file, int line, char* msg)
{
  static const int log2syslog_level[] = { LOG_ERR, LOG_WARNING, LOG_INFO, LOG_DEBUG };
#ifdef _DEBUG

  // replace \r\n through a dot
  for(char* c=msg; (*c); c++)
    if(*c == '\r' || *c == '\n')
      *c = '.';

# ifndef NO_THREADID_LOG
#  ifdef LOG_LOC_DATA_ATEND
  syslog(log2syslog_level[level], "%s: %s [#%lx] [%s %s:%d]",
	 log_level2str[level], msg, (unsigned long)tid, func, file, line);
#  else
  syslog(log2syslog_level[level], "[#%lx] [%s, %s:%d] %s: %s",
	 (unsigned long)tid, func, file, line, log_level2str[level], msg);
#  endif
# else /* NO_THREADID_LOG */
#  ifdef LOG_LOC_DATA_ATEND
  syslog(log2syslog_level[level], "%s: %s [%s] [%s:%d]",
      log_level2str[level], msg, func, file, line);
#  else 
  syslog(log2syslog_level[level], "[%s, %s:%d] %s: %s",
	 func, file, line, log_level2str[level], msg);
#  endif
# endif /* NO_THREADID_LOG */

#else /* !_DEBUG */
#  ifdef LOG_LOC_DATA_ATEND
  syslog(log2syslog_level[level], "%s: %s [%s:%d]",
      log_level2str[level], msg, file, line);
#  else
  syslog(log2syslog_level[level], "[%u/%s:%d] %s: %s",
     tid, file, line, log_level2str[level], msg);
#  endif

#endif /* !_DEBUG */
}

int set_syslog_facility(const char* str)
{
  return (SyslogLogFac::instance().setFacility(str) == true);
}

#endif /* !DISABLE_SYSLOG_LOG */

class StderrLogFac : public AmLoggingFacility {
    static StderrLogFac *_instance;
    StderrLogFac()
        : AmLoggingFacility("stderr",AmConfig::LogLevel)
    { }
  public:
    static StderrLogFac &instance() {
        if(!_instance) _instance = new StderrLogFac();
        return *_instance;
    }
    ~StderrLogFac() { }
    int onLoad() {  return 0; }
    void log(int level_, pid_t pid, pid_t tid, const char* func, const char* file, int line, char* msg_)
    {
        fprintf(stderr, COMPLETE_LOG_FMT);
        fflush(stderr);
    }
};
StderrLogFac *StderrLogFac::_instance = NULL;

/**
 * Initialize logging
 */
void init_logging()
{
  _self_pid = GET_PID();
  _self_tid = GET_TID();

  log_hooks.clear();

#ifndef DISABLE_SYSLOG_file
  register_log_hook(&SyslogLogFac::instance());
#endif

  INFO("Logging initialized\n");
}

/**
 * Run log hooks
 */
void run_log_hooks(int level, pid_t pid, pthread_t tid, const char* func, const char* file, int line, char* msg)
{
  AmLock l(log_hooks_mutex);
  (void)l;

  if (!log_hooks.empty()) {
    for (vector<AmLoggingFacility*>::iterator it = log_hooks.begin();
         it != log_hooks.end(); ++it) {
      AmLoggingFacility* fac = *it;
      if(level <= fac->getLogLevel())
        fac->log(level, pid, tid, func, file, line, msg);
    }
  }
}

/**
 * Register the log hook
 */
void register_log_hook(AmLoggingFacility* fac)
{
  AmLock lock(log_hooks_mutex);
  log_hooks.push_back(fac);
  inc_ref(fac);
}

void unregister_log_hook(AmLoggingFacility* fac){
	AmLock lock(log_hooks_mutex);
	vector<AmLoggingFacility*>::iterator fac_it = std::find(log_hooks.begin(),log_hooks.end(),fac);
	if(fac_it!=log_hooks.end())
		log_hooks.erase(fac_it);
}

bool has_higher_levels(int log_level_arg){
	AmLock lock(log_hooks_mutex);
	if(log_hooks.empty())
		return false;
	for (vector<AmLoggingFacility*>::iterator it = log_hooks.begin();
		it != log_hooks.end(); ++it)
	{
		if(log_level_arg < (*it)->getLogLevel())
			return true;
	}
	return false;
}

void set_log_level(int log_level_arg){
	ERROR("set syslog loglevel to %d",log_level_arg);
	SyslogLogFac::instance().setLogLevel(log_level_arg);
	ERROR("global log_level is %d",log_level);
}

void register_stderr_facility() {
	register_log_hook(&StderrLogFac::instance());
}

void set_stderr_log_level(int log_level_arg) {
	StderrLogFac::instance().setLogLevel(log_level_arg);
}

/**
 * Print stack-trace through logging function
 */
void log_stacktrace(int ll)
{
   void* callstack[128];
   int i, frames = backtrace(callstack, 128);
   char** strs = backtrace_symbols(callstack, frames);
   for (i = 0; i < frames; ++i) {
     _LOG(ll,"stack-trace(%i): %s", i, strs[i]);
   }
   free(strs);
}
