/* $Id: logger.c,v 1.5 2003-03-16 17:00:45 martin Exp $ */

#include <stdarg.h>
#include <errno.h>
#include <time.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#if defined (UNIX)
#include <syslog.h>
#endif

#include "logger.h"
#include "version.h"

/* use logfile or syslog (for unix) instead of stderr */
#define LOGNAME "gully.log"

/* forward decl */
static void err_doit(int errnoflag, const char *fmt, va_list ap);

static void log_doit(int, int, const char*, va_list ap);
static FILE * logfile = NULL;
static int open_logfile();
static void log2file(const char * buf);

void 
err_ret(const char *fmt, ...)
{
  va_list ap;

  va_start(ap, fmt);
  err_doit(1, fmt, ap);
  va_end(ap);
  return;
}

void 
err_sys(const char *fmt, ...)
{
  va_list ap;

  va_start(ap, fmt);
  err_doit(1, fmt, ap);
  va_end(ap);
  exit(1);
}


void 
err_dump(const char *fmt, ...)
{
  va_list ap;

  va_start(ap, fmt);
  err_doit(1, fmt, ap);
  va_end(ap);
  abort();
  exit(1);
}


void 
err_msg(const char *fmt, ...)
{
  va_list ap;

  va_start(ap, fmt);
  err_doit(0, fmt, ap);
  va_end(ap);
  return;
}

void 
err_quit(const char *fmt, ...)
{
  va_list ap;

  va_start(ap, fmt);
  err_doit(0, fmt, ap);
  va_end(ap);
  exit(1);
}

static void
err_doit(int errnoflag, const char *fmt, va_list ap)
{
  int errno_save;
  char buf[512];

  errno_save = errno;
  vsprintf(buf, fmt, ap);
  if(errnoflag)
    sprintf(buf + strlen(buf), ": %s", strerror(errno_save));
  strcat(buf,"\n");
  fflush(stdout);

  /* additionally we also log the error: */
  log_msg(buf);

  fputs(buf, stderr);
  fflush(NULL);
  return;
}

/* 
 * Open a log file where messages written by log_* are appended 
 * returns 1 for success, 0 on error
 */
static int
open_logfile()
{
  if (logfile == NULL) { 
    if((logfile = fopen(LOGNAME,"a")) == NULL) {
      fprintf(stderr,"Could not open log file %s: %s\n", LOGNAME, 
	      strerror(errno));
      return 0;
    }
    else { /* logfile successfully opened first time */
      setbuf(logfile, 0);
      log_msg("Gullydeckel %s (%s %s).\n", VERSION, __DATE__, __TIME__);
    }
  }
  return 1;
}

/* close log (on exit) 
 * This is not OS-dependent.
 */

void
log_close()
{
  if(logfile == NULL) return;

  log_msg("Closing log.\n");
  if(fclose(logfile) == EOF) perror("Close failed.");
}

/* 
 * Print logging message to file using a standard header.
 */
static void
log2file(const char * buf)
{
  time_t t;
  char tbuf[30]; /* time */
  static char last_message[128];
  static unsigned int message_count = 0;

  if(!logfile && !open_logfile()) return;

  /* on similar messages, just count them. */
  if (strncmp(buf, last_message, 127) == 0) {
    message_count++;
    return;
  }
  else if (message_count) {
    fprintf(logfile,"G2: Last message repeated %u times.\n", message_count);
    message_count = 0;
  }

  strncpy(last_message, buf, 127);

  t = time(NULL);
  
  /* remove trailing newline */
  strcpy(tbuf, ctime(&t));
  tbuf[strlen(tbuf)-1] ='\0';

  if(fprintf(logfile,"G2: %s: %s", tbuf, buf) == EOF)
    fprintf(stderr, "Error writing log message: %s\n", strerror(errno));
}

/* now OS-specific stuff */

#ifdef UNIX
extern int use_syslog; /* must be defined by user */

void 
log_open(const char * ident, int option, int facility)
{
  if(use_syslog)
    openlog(ident, option, facility);
  else open_logfile();
}

void 
log_ret(const char *fmt, ...)
{
  va_list ap;

  va_start(ap, fmt);
  log_doit(1, LOG_ERR, fmt, ap);
  va_end(ap);
  return;
}

void 
log_sys(const char *fmt, ...)
{
  va_list ap;

  va_start(ap,fmt);
  log_doit(1, LOG_ERR, fmt, ap);
  va_end(ap);
  exit(2);
}


void 
log_msg(const char *fmt, ...)
{
  va_list ap;

  va_start(ap, fmt);
  log_doit(0, LOG_ERR, fmt, ap);
  va_end(ap);
  return;
}


void 
log_quit(const char *fmt, ...)
{
  va_list ap;

  va_start(ap, fmt);
  log_doit(0, LOG_ERR, fmt, ap);
  va_end(ap);
  exit(2);
}

static void
log_doit(int errnoflag, int priority, const char *fmt, va_list ap)
{
  int errno_save;
  char buf[512];

  errno_save = errno;
  vsprintf(buf, fmt, ap);
  if(errnoflag)
    sprintf(buf,"\n");
  if(use_syslog)
    syslog(priority, buf);
  else 
    log2file(buf);
  return;
}

#else /* provide functions for use under win32 */

void 
log_open(const char * ident, int option, int facility)
{
  open_logfile();
  return;
}

void 
log_ret(const char *fmt, ...)
{
  va_list ap;

  va_start(ap, fmt);
  log_doit(1, 0, fmt, ap);
  va_end(ap);
  return;
}

void 
log_sys(const char *fmt, ...)
{
  va_list ap;

  va_start(ap,fmt);
  log_doit(1, 0, fmt, ap);
  va_end(ap);
  exit(2);
}


void 
log_msg(const char *fmt, ...)
{
  va_list ap;

  va_start(ap, fmt);
  log_doit(0, 0, fmt, ap);
  va_end(ap);
  return;
}


void 
log_quit(const char *fmt, ...)
{
  va_list ap;

  va_start(ap, fmt);
  log_doit(0, 0, fmt, ap);
  va_end(ap);
  exit(2);
}

/* 
 * Logging always to file if running under win32.
 */

static void
log_doit(int errnoflag, int priority, const char *fmt, va_list ap)
{
  int errno_save;
  char buf[512];

  errno_save = errno;
  vsprintf(buf, fmt, ap);
  if(errnoflag)
    sprintf(buf,"\n");

  log2file(buf);
  return;
}


#endif /* unix-win32 logging switch */
