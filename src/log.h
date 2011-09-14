/**
 * Header file for logging
 *
 * $Id: log.h,v 1.1.1.1 2009/07/24 08:15:50 hightman Exp $
 */

#ifndef __XS_LOG_20110520_H__
#define	__XS_LOG_20110520_H__

#ifdef __cplusplus
extern "C" {
#endif

/* The maximum length of each logging line */
#define	MAX_LOG_LINE	1024

/* Change log ident*/
char *log_setid(const char *ident);

/* Open log, stderr supported */
int log_open(const char *file, const char *ident);

/* Close log */
void log_close();

/* Simple logging function */
void log_printf(const char *fmt, ...);

/* Duplicate file description */
void log_dup2(int fd);

/* Debug log macro */
#ifdef DEBUG
#define	log_debug(fmt, ...)	log_printf("[%s:%d] " fmt, __FILE__, __LINE__, ##__VA_ARGS__)
#else
#define	log_debug(fmt, ...)	(void)0
#endif

#ifdef __cplusplus
}
#endif

#endif	/* __XS_LOG_20110520_H__ */
