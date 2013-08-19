/**
 * Simple and easy logging
 * 
 * $Id: log.c,v 1.1.1.1 2009/07/24 08:15:50 hightman Exp $
 */

#include <stdio.h>
#include <time.h>
#include <string.h>
#include <unistd.h>

#include "flock.h"
#include "log.h"

/* static local fd */
static int lfd = -1;
static int lvl = LOG_NOTICE; /* error */
static char lid[64];

/* get log level string */
static const char *_log_level[] = {
	"EMERG",
	"ALERT",
	"CRIT",
	"ERROR",
	"WARN",
	"NOTICE",
	"INFO",
	"DEBUG",
	"INFO",
};

/**
 * Change log id
 */
const char *log_ident(const char *ident)
{
	if (ident != NULL) {
		memset(lid, 0, sizeof(lid));
		strncpy(lid, ident, sizeof(lid) - 1);
	}
	return lid;
}

/**
 * Change log level
 */
int log_level(int level)
{
	if (level >= 0 && level <= 7) {
		lvl = level;
	}
	return lvl;
}

/**
 * Close log file
 */
void log_close()
{
	if (lfd >= 0) {
		close(lfd);
		lfd = -1;
	}
}

/**
 * Open log
 */
int log_open(const char *file, const char *ident, int level)
{
	log_close();
	lfd = strcasecmp(file, "stderr") ? open(file, O_WRONLY | O_CREAT | O_APPEND, 0600) : STDERR_FILENO;
	log_ident(ident);
	log_level(level);
	return lfd;
}

/**
 * Log printf
 */
void _log_printf(int level, const char *fmt, ...)
{
	if (lfd >= 0 && (level <= lvl || level == LOG_PRINTF)) {
		int sz;
		time_t now;
		struct tm tm;
		char buf[MAX_LOG_LINE];
		va_list ap;

		time(&now);
		localtime_r(&now, &tm);
		sprintf(buf, "%d-%02d-%02d %02d:%02d:%02d %s[%d] %s ",
				tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday,
				tm.tm_hour, tm.tm_min, tm.tm_sec, lid, getpid(), _log_level[level]);

		sz = strlen(buf);
		va_start(ap, fmt);
		vsnprintf(buf + sz, MAX_LOG_LINE - sz, fmt, ap);
		va_end(ap);

		sz = strlen(buf);
		buf[sz++] = '\n';

		FLOCK_WR(lfd);
		lfd == STDERR_FILENO ? write(lfd, buf + 11, sz - 11) : write(lfd, buf, sz);
		FLOCK_UN(lfd);
	}
}

/**
 * Duplicate file description
 */
void log_dup2(int fd)
{
	if (lfd >= 0) {
		dup2(lfd, fd);
	}
}
