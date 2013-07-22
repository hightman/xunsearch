/**
 * Thread-safe file locking
 *
 * $Id: flock.c,v 1.1.1.1 2009/07/24 08:15:50 hightman Exp $
 */

#include "flock.h"
#include <errno.h>
#include <stdarg.h>
#include <stdlib.h>

/**
 * Lock or unlock operation
 * @param fd integer file description to operate
 * @param type integer locking type, F_WRLCK|F_RDLCK|F_UNLCK
 * @param offset integer starting offset of fd
 * @param whence integer type of offset, SEEK_SET|SEEK_CUR|SEEK_END
 * @param len integer locking length, 0 means until end of file
 * @param nonblock wait or not
 * @return integer 1 returned on successful, and 0 returned on failure
 */
int flock_exec(int fd, int type, off_t offset, int whence, off_t len, int nonblock)
{
	int rc;
	struct flock fl;

	if (fd < 0) {
		return 0;
	}
	fl.l_type = type;
	fl.l_start = offset;
	fl.l_whence = whence;
	fl.l_len = len;
	fl.l_pid = 0;
	do {
		rc = fcntl(fd, nonblock ? F_SETLK : F_SETLKW, &fl);
	} while (rc < 0 && errno == EINTR);

	return (rc == 0);
}

/**
 * Initializing file lock structure
 * @param fl
 * @param fpath NULL to auto assignmented
 * @return 1->succesful, 0->failed
 */
int flock_init(flock_t *fl, const char *fpath)
{
	// initilized?
	if (fl->flag & 0x01) {
		flock_destroy(fl);
	}

	if (fpath == NULL) {
		char mypath[] = "/tmp/flk.XXXXXX";
		fl->fd = mkstemp(mypath);
		if (fl->fd >= 0)
			unlink(mypath);
	} else {
		fl->fd = open(fpath, O_CREAT | O_RDWR, 0600);
	}
	fl->flag = 0x01;
	return (fl->fd >= 0);
}

/**
 * Enable thread-safe
 * @param fl
 * @return 1->succesful, 0->failed
 */
int flock_set_thread_safe(flock_t *fl)
{
	if (pthread_rwlock_init(&fl->plock, NULL) == 0) {
		fl->flag |= 0x02;
		return 1;
	}
	return 0;
}

/**
 * Exclusive lock (read-write protected)
 * @param fl
 * @return 1->succesful, 0->failed
 */

int flock_wrlock(flock_t *fl)
{
	if (!(fl->flag & 0x02) || pthread_rwlock_wrlock(&fl->plock) == 0) {
		return FLOCK_WR(fl->fd);
	}
	return 0;
}

/**
 * Shared lock (read protected)
 * @param fl
 * @return 1->succesful, 0->failed
 */
int flock_rdlock(flock_t *fl)
{
	if (!(fl->flag & 0x02) || pthread_rwlock_rdlock(&fl->plock) == 0) {
		return FLOCK_RD(fl->fd);
	}
	return 0;
}

/**
 * Releasing lock
 * @param fl
 * @return 1->succesful, 0->failed
 */
int flock_unlock(flock_t *fl)
{
	if (FLOCK_UN(fl->fd) && (!(fl->flag & 0x02) || pthread_rwlock_unlock(&fl->plock) == 0)) {
		return 1;
	}
	return 0;
}

/**
 * Destroy all of the tempory lock resources
 * @param fl
 */
void flock_destroy(flock_t *fl)
{
	if (fl->flag & 0x02) {
		pthread_rwlock_destroy(&fl->plock);
	}
	close(fl->fd);
	fl->fd = -1;
	fl->flag = 0;
}
