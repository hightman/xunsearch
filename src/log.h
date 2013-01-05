/**
 * Header file for logging
 *
 * $Id: log.h,v 1.1.1.1 2009/07/24 08:15:50 hightman Exp $
 */

#ifndef __XS_LOG_20110520_H__
#define	__XS_LOG_20110520_H__

#include <stdarg.h>

#ifdef __cplusplus
extern "C" {
#endif

/* The maximum length of each logging line */
#define	MAX_LOG_LINE	1024
#define LOG_EMERG		0	/* system is unusable */
#define LOG_ALERT		1	/* action must be taken immediately */
#define LOG_CRIT		2	/* critical conditions */
#define LOG_ERR			3	/* error conditions */
#define LOG_WARNING		4	/* warning conditions */
#define LOG_NOTICE		5	/* normal but significant condition */
#define LOG_INFO		6	/* informational */
#define LOG_DEBUG		7	/* debug-level messages */
#define	LOG_PRINTF		8	/* forced infomation */

/* Set/Get log ident */
const char *log_ident(const char *ident);

/* Set/Get log level */
int log_level(int level);

/* Open log, stderr supported, level: 0 ~ 7 */
int log_open(const char *file, const char *ident, int level);

/* Close log */
void log_close();

/* Simple logging function */
void _log_printf(int level, const char *fmt, ...);

/* Duplicate file description */
void log_dup2(int fd);

/* Log macro defines */
#ifdef DEBUG
#define	log_debug(fmt,...)		_log_printf(LOG_DEBUG, "[%s:%d] " fmt, __FILE__, __LINE__, ##__VA_ARGS__)
#else
#define	log_debug(...)			(void)0
#endif

#define	log_info(fmt,...)		_log_printf(LOG_INFO, fmt, ##__VA_ARGS__)
#define	log_notice(fmt,...)		_log_printf(LOG_NOTICE, fmt, ##__VA_ARGS__)
#define	log_warning(fmt,...)	_log_printf(LOG_WARNING, fmt, ##__VA_ARGS__)
#define	log_error(fmt,...)		_log_printf(LOG_ERR, fmt, ##__VA_ARGS__)
#define	log_crit(fmt,...)		_log_printf(LOG_CRIT, fmt, ##__VA_ARGS__)
#define	log_alert(fmt,...)		_log_printf(LOG_ALERT, fmt, ##__VA_ARGS__)
#define	log_emerg(fmt,...)		_log_printf(LOG_EMERG, fmt, ##__VA_ARGS__)
#define	log_printf(fmt,...)		_log_printf(LOG_PRINTF, fmt, ##__VA_ARGS__)

#ifdef __cplusplus
}
#endif

#endif	/* __XS_LOG_20110520_H__ */
