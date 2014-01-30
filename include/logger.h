/* $Id: logger.h,v 1.3 2003-03-02 13:52:42 martin Exp $
 * error printing/ logging functions (rich stevens) 
 */

#ifndef __LOGGER_H
#define __LOGGER_H

/* non-fatal, syscall related */
void err_ret(const char *fmt, ...);

/* fatal, syscall related */
void err_sys(const char *fmt, ...);

/* fatal, core dump, syscall related */
void err_dump(const char *fmt, ...);

/* non-fatal, non-syscall related */
void err_msg(const char *fmt, ...);

/* fatal, non-syscall related */
void err_quit(const char *fmt, ...);

void log_open(const char *fmt,int option, int facility);

void log_close(void);

/* non-fatal, syscall related */
void log_ret(const char *fmt, ...);

/* fatal, syscall related */
void log_sys(const char *fmt, ...);

/* non-fatal, non-syscall related */
void log_msg(const char *fmt, ...);

/* fatal, non-syscall related */
void log_quit(const char *fmt, ...);

#endif /* logger.h */
