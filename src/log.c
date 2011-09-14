/**
 * Simple and easy logging
 * 
 * $Id: log.c,v 1.1.1.1 2009/07/24 08:15:50 hightman Exp $
 */

#include <stdio.h>
#include <stdarg.h>
#include <time.h>
#include <string.h>
#include <unistd.h>

#include "flock.h"
#include "log.h"

/* static local fd */
static int log_fd = -1;
static char log_id[64];

/**
 * Change log id
 */
char *log_setid(const char *ident)
{
	if (ident != NULL)
	{
		memset(log_id, 0, sizeof(log_id));
		strncpy(log_id, ident, sizeof(log_id) - 1);
	}
	return log_id;
}

/**
 * close log file
 */
void log_close()
{
	if (log_fd >= 0)
	{
		close(log_fd);
		log_fd = -1;
	}
}

/**
 * Open log file
 * @param file
 */
int log_open(const char *file, const char *ident)
{
	log_close();
	log_fd = strcasecmp(file, "stderr") ? open(file, O_WRONLY | O_CREAT | O_APPEND, 0600) : STDERR_FILENO;
	log_setid(ident);
	return log_fd;
}

/**
 * log printf
 */
void log_printf(const char *fmt, ...)
{
	if (log_fd >= 0)
	{
		int sz;
		time_t now;
		struct tm tm;
		char buf[MAX_LOG_LINE];
		va_list ap;

		time(&now);
		localtime_r(&now, &tm);
		sprintf(buf, "%d-%02d-%02d %02d:%02d:%02d %s[%d] ",
			tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday,
			tm.tm_hour, tm.tm_min, tm.tm_sec, log_id, getpid());

		sz = strlen(buf);
		va_start(ap, fmt);
		vsnprintf(buf + sz, MAX_LOG_LINE - sz, fmt, ap);
		va_end(ap);

		sz = strlen(buf);
		buf[sz++] = '\n';

		FLOCK_WR(log_fd);
		log_fd == STDERR_FILENO ? write(log_fd, buf + 11, sz - 11) : write(log_fd, buf, sz);
		FLOCK_UN(log_fd);
	}
}

/**
 * Duplicate file description
 */
void log_dup2(int fd)
{
	if (log_fd >= 0)
		dup2(log_fd, fd);
}
