/**
 * XunSearch Index Daemon server
 *
 * $Id$
 */
#ifdef HAVE_CONFIG_H
#    include "config.h"
#endif

#ifndef	PREFIX
#    define	PREFIX	"."
#endif

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <dirent.h>
#include <sys/param.h>
#include <sys/stat.h>
#include <errno.h>
#include <signal.h>
#include <fcntl.h>
#include <time.h>

#include "conn.h"
#include "log.h"
#include "pcntl.h"
#include "global.h"
#include "indexd.h"

/**
 * Flags for main
 */
#define	FLAG_FOREGROUND		0x01
#define	FLAG_NO_ERROR		0x02
#define	FLAG_G_INITED		0x04
#define	FLAG_ON_EXIT		0x08

#define	IS_RQST_CMD(c)		(c==CMD_INDEX_SUBMIT||c==CMD_DOC_TERM||c==CMD_DOC_INDEX||c==CMD_DOC_VALUE)

/**
 * Global variables
 */
G_DECL();
G_VAR_DECL(user_base, void *);

/**
 * Local static variables
 */
static time_t time_logging;
static int queue_size;
static char xs_import[128], xs_logging[128], *prog_name;
static volatile int main_flag, import_num;

/**
 * Show version information
 */
static void show_version()
{
	printf("%s: %s/%s (index server)\n", prog_name, PACKAGE_NAME, PACKAGE_VERSION);
	exit(0);
}

/**
 * Usage help
 */
static void show_usage()
{
	printf("%s (%s/%s) - Index Submit Server\n", prog_name, PACKAGE_NAME, PACKAGE_VERSION);
	printf("Copyright (C)2007-2011 hightman, HangZhou YunSheng Network Co., Ltd.\n\n");

	printf("Usage: %s [options]\n", prog_name);
	printf("  -F               Run the server on foreground (non-daemon)\n");
	printf("  -H <home>        Specify the working directory\n");
	printf("                   Default: " PREFIX "\n");
	printf("  -b <port>|<address:port>|<path>\n");
	printf("                   Binding adddress/port or path for the server, (default: " DEFAULT_BIND_PATH ")\n");
	printf("  -l <log_file>    Specify the log output file, (default: none)\n");
	printf("                   E.g: " DEFAULT_TEMP_DIR "%s.log, stderr\n", prog_name);
	printf("  -q <num>         Set the queue size to trigger commit, (default: %d)\n", DEFAULT_QUEUE_SIZE);
	printf("  -e <bin_path>    Set the external executable program path, (default: " DEFAULT_BIN_PATH ")\n");
	printf("  -k [fast]<stop|start|restart|reload> Server process running control\n");
	printf("  -v               Show version information\n");
	printf("  -h               Display this help page\n\n");
	printf("Report bugs to " PACKAGE_BUGREPORT "\n");
	exit(0);
}

/**
 * Call external program to parse logging database
 * Run every 2 hours automatically
 */
static void xs_logging_call(XS_USER *user)
{
	time_t now = time(NULL);

	if (user == NULL)
		log_debug("check to call xs-logging (IMPORT_NUM:%d, TIME_CC:%d)", import_num, now - time_logging);
	if (user != NULL || ((now - time_logging) > 7200))
	{
		pid_t pid = fork();
		if (pid == 0)
		{
			// redirect logging message
			log_dup2(STDOUT_FILENO);
			log_dup2(STDERR_FILENO);

			// call external program
			if (user == NULL)
				exit(execl(xs_logging, "xs-logging", "-SQT", DEFAULT_DATA_DIR, NULL));
			else
				exit(execl(xs_logging, "xs-logging", "-QT", user->home, NULL));			
		}
		else if (pid > 0)
		{
			// increase the import process number
			import_num++;
			if (user == NULL)
				time_logging = now;
		}
		else
		{
			// fork error(), the file will try to import in next calling
			log_printf("failed to fork child logging process (ERROR:%s)", strerror(errno));
		}
	}
}

/**
 * Call external program to import, write to the Xapian database
 * @param db
 */
static void db_import_call(XS_DB *db, XS_USER *user)
{
	pid_t pid;
	char dbpath[256], sndfile[128];

	// temporary swap file
	sprintf(dbpath, "%s/%s", user->home, db->name);
	sprintf(sndfile, DEFAULT_TEMP_DIR "%s_%s.snd", user->name, db->name);
	if (db->flag & XS_DBF_REBUILD_BEGIN)
		strcat(dbpath, ".re");

	// check old send file first
	if (access(sndfile, R_OK) == 0)
		log_printf("priority to commit unfinished sndfile (FILE:%s)", sndfile);
	else
	{
		char rcvfile[128], *suffix;
		int i = 0;

		// check to commit older file first
		sprintf(rcvfile, DEFAULT_TEMP_DIR "%s_%s.rcv", user->name, db->name);
		suffix = rcvfile + strlen(rcvfile);
#if SIZEOF_OFF_T < 8
		for (i = MAX_SPLIT_FILES; i > 0; i--)
		{
			sprintf(suffix, ".%d", i);
			if (!access(rcvfile, R_OK))
			{
				if (rename(rcvfile, sndfile) == 0)
					log_printf("priority to commit older splitted file (OLD:%s)", rcvfile);
				else
				{
					i = 0;
					log_printf("failed to rename older file, commit new file (OLD:%s, ERROR:%s)",
						rcvfile, strerror(errno));
				}
				break;
			}
		}
#endif	/* LARGEFILE */

		// use new rcvfile
		if (i == 0)
		{
			// re-check parameters [maye come from user command]
			if (db->count == 0 || db->fd < 0)
			{
				log_printf("not necessary commit request (DB:%s.%s)", user->name, db->name);
				return;
			}

			// release some resource
			close(db->fd);
			db->fd = -1;
			db->count = db->lcount = 0;

			// rename the file
			*suffix = '\0';
			if (rename(rcvfile, sndfile) != 0)
			{
				log_printf("failed to rename rcvfile (FILE:%s, ERROR:%s)",
					rcvfile, strerror(errno));
				return;
			}

			// clean force flag
			db->flag &= ~XS_DBF_FORCE_COMMIT;
		}
	}

	// fork child process to run the import
	if ((pid = fork()) == 0)
	{
		// ensure that the parent process run first
		usleep(50000);
		// redirect logging message
		log_dup2(STDOUT_FILENO);
		log_dup2(STDERR_FILENO);

		// TODO: cleanup some global resources?!

		// call external program
		exit(execl(xs_import, "xs-import", "-Q", dbpath, sndfile, NULL));
	}
	else if (pid > 0)
	{
		// save pid in parent
		log_debug("success to fork child import process (PID:%d, DB:%p)", pid, db);
		db->pid = pid;
		// increase the import process number
		import_num++;
	}
	else
	{
		// fork error(), the file will try to import in next calling
		log_printf("failed to fork child import process (DB:%s.%s, ERROR:%s)",
			user->name, db->name, strerror(errno));
	}
}

/**
 * Database auto-commit check
 * @param quit Is the server will quit at once (called in main_cleanup)
 */
static void db_commit_check()
{
	XS_USER *user;
	XS_DB *db;
	time_t now;

	time(&now);
	user = (XS_USER *) G_VAR(user_base);
	log_debug("check to commit database (EXIT:%s)", (main_flag & FLAG_ON_EXIT) ? "yes" : "no");
	while (user != NULL)
	{
		for (db = user->db; db != NULL; db = db->next)
		{
			if (main_flag & FLAG_ON_EXIT)
			{
				// in quit mode, just close the file description
				// rcvfile will continue to use in the next running
				if (db->fd >= 0)
					close(db->fd);
				// notify child import process to quit also
				if (db->pid > 0)
					kill(db->pid, SIGTERM);
				continue;
			}

			// no data, import running?
			if (db->count == 0 || db->fd < 0 || db->pid != 0)
				continue;

			// allow to submit small request
			if (import_num >= MAX_IMPORT_NUM && db->count > MIN_COMMIT_COUNT)
			{
				log_printf("server is too busy to skip commit (IMPORT_NUM:%d, DB:%s.%s, COUNT:%d)",
					import_num, user->name, db->name, db->count);
				continue;
			}

			// avoid committing too frequent
			if (!(db->flag & XS_DBF_FORCE_COMMIT)
				&& db->count < MIN_COMMIT_COUNT && (now - db->ltime) < MIN_COMMIT_TIME)
			{
				log_printf("skip to commit temporary (DB:%s.%s, COUNT:%d)",
					user->name, db->name, db->count);
				continue;
			}

			// do commit
			log_printf("commit index data automatically (DB:%s.%s, COUNT:%d)",
				user->name, db->name, db->count);
			db_import_call(db, user);
		}
		user = user->next;
	}
}

/**
 * Try to get db by pid
 * @param pid
 * @return XS_DB pointer on success or NULL on failure
 */
static XS_DB *db_get_by_pid(pid_t pid, XS_USER **db_user)
{
	XS_USER *user;
	XS_DB *db = NULL;

	user = (XS_USER *) G_VAR(user_base);
	while (user != NULL)
	{
		for (db = user->db; db != NULL; db = db->next)
		{
			if (db->pid == pid)
			{
				*db_user = user;
				return db;
			}
		}
		user = user->next;
	}

	return NULL;
}

/**
 * Try to get current wdb for connection
 * @param conn
 * @return XS_DB pointer or NULL on failure
 */
static inline XS_DB *get_conn_wdb(XS_CONN *conn)
{
	if (conn->wdb == NULL)
		conn->wdb = xs_user_get_db(conn->user, DEFAULT_DB_NAME, sizeof(DEFAULT_DB_NAME) - 1);
	return conn->wdb;
}

/**
 * Remove the directory recursively
 * @param fpath
 * @return zero returned on success, and -1 returned on failure
 */
static int rmdir_r(const char *fpath)
{
	struct stat st;
	struct dirent *de;
	char buf[PATH_MAX], *fname;
	DIR *dirp;

	if (!(dirp = opendir(fpath)))
	{
		log_printf("can not open directory (DIR:%s)", fpath);
		return 0;
	}

	for (fname = buf; (*fname = *fpath) != '\0'; fname++, fpath++);
	if (fname[-1] != '/') *fname++ = '/';

	while ((de = readdir(dirp)) != NULL)
	{
		fpath = de->d_name;
		if (!*fpath) continue;
		if (fpath[0] == '.' && (!fpath[1] || (fpath[1] == '.' && !fpath[2])))
			continue;

		strcpy(fname, fpath);
		if (stat(buf, &st) != 0)
		{
			if (errno != ENOENT)
				break;
		}
		else if (S_ISDIR(st.st_mode))
		{
			if (rmdir_r(buf) != 0)
				break;
		}
		else if (unlink(buf) != 0)
		{
			break;
		}
	}

	closedir(dirp);
	if (de == NULL)
	{
		fname[-1] = '\0';
		return rmdir(buf);
	}
	return -1;
}

/**
 * Safe write data into file description
 * @return zero on success, -1 on failure
 */
static int safe_write(int fd, void *buf, int size)
{
	int bytes;
	off_t off = lseek(fd, 0, SEEK_CUR);

	while ((bytes = write(fd, buf, size)) != size)
	{
		if (bytes > 0)
		{
			size -= bytes;
			buf = (char *) buf + bytes;
		}
		else if (errno == EINTR || errno == EAGAIN)
		{
			if (errno == EAGAIN)
				usleep(50000);
		}
		else
		{
			lseek(fd, off, SEEK_SET);
			return -1;
		}
	}
	return 0;
}

/**
 * Cleanup then exit
 */
static inline void main_cleanup()
{
	// add exit flag for other signal handler (child)
	main_flag |= FLAG_ON_EXIT;

	if (main_flag & FLAG_G_INITED)
	{
		log_printf("server terminated, force to check commit for all db");
		db_commit_check();

		xs_user_deinit();
		G_VAR_FREE(user_base);
		G_DEINIT();
	}
	log_close();
}

/**
 * Termination handler for signal
 */
int signal_term(int sig)
{
	log_printf("caught system %ssignal[%d], terminate immediately",
		(sig == SIGTERM ? "" : "exceptional "), sig);
	main_cleanup();
	return main_flag & FLAG_NO_ERROR ? 0 : -1;
}

/**
 * Rename db after rebuilding
 * @param db
 * @param user
 */
static void rebuild_wdb_end(XS_DB *db, XS_USER *user)
{
	// NOTE: size must greater than (sizeof(user->home)+sizeof(db->name))
	char dbre[256], dbpath[256];

	// rename the db.re => db
	sprintf(dbre, "%s/%s.re", user->home, db->name);
	sprintf(dbpath, "%s/%s", user->home, db->name);
	log_debug("rebuild finished, rename (PATH:%s -> %s)", dbre, dbpath);
	if (rmdir_r(dbpath) != 0 || rename(dbre, dbpath) != 0)
	{
		log_printf("failed to rename rebuilt database (DB:%s.%s, ERROR:%s)",
			user->name, db->name, strerror(errno));
	}

	// clean flag
	db->flag &= ~(XS_DBF_REBUILD_BEGIN | XS_DBF_REBUILD_END | XS_DBF_REBUILD_WAIT);
}

/**
 * Child process reaper (import)
 */
void signal_child(pid_t pid, int status)
{
	char sndfile[256]; // size must greater than dbpath
	XS_USER *user = NULL;
	XS_DB *db;

	// TODO: check on_exit flag?!
	if (main_flag & FLAG_ON_EXIT)
	{
		log_printf("skip exit report from child process (PID:%d, EXIT:%d)", pid, status);
		return;
	}

	// reduce the import process num
	import_num--;
	if ((db = db_get_by_pid(pid, &user)) == NULL)
	{
		log_printf("can not get db by pid (PID:%d)", pid);
		return;
	}

	// reset db struct
	db->pid = 0;
	time(&db->ltime);

	// logging
	log_printf("child import process exit (DB:%s.%s%s, PID:%d, EXIT:%d)",
		user->name, db->name, (db->flag & XS_DBF_REBUILD_BEGIN ? ".re" : ""), pid, status);
	sprintf(sndfile, DEFAULT_TEMP_DIR "%s_%s.snd", user->name, db->name);

	// check result
	if (db->flag & XS_DBF_TOCLEAN)
	{
		// marked as clean
		db->flag ^= XS_DBF_TOCLEAN;
		db->flag |= XS_DBF_FORCE_COMMIT;

		unlink(sndfile);
		sprintf(sndfile, "%s/%s", user->home, db->name);
		log_debug("clean marked database (PATH:%s)", sndfile);
		if (rmdir_r(sndfile) != 0)
		{
			log_printf("failed to clean marked database (DB:%s.%s, ERROR:%s)",
				user->name, db->name, strerror(errno));
		}
	}
	else if (db->flag & XS_DBF_REBUILD_WAIT)
	{
		// marked as rebuild
		db->flag ^= XS_DBF_REBUILD_WAIT;
		unlink(sndfile);
		log_debug("rebuild marked database (DB:%s.%s)", user->name, db->name);
	}
	else if (status == 0)
	{
		// quit normal, remove sndfile
		if (unlink(sndfile) != 0)
			log_printf("failed to remove sndfile (PATH:%s, ERROR:%s)", sndfile, strerror(errno));
		// check to rebuild end!
		if (db->flag & XS_DBF_REBUILD_END)
			rebuild_wdb_end(db, user);
	}
	else
	{
		// quit excepional, keep sndfile for next trying
	}
}

/**
 * Shutdown gracefully
 */
void signal_int()
{
	log_printf("caught SIGINT, shutdown gracefully");
	conn_server_push_back(NULL);
}

/**
 * Reload handler
 */
void signal_reload(int sig)
{
	log_printf("caught reload signal[%d], but nothing to do", sig);
}

/**
 * Update the effective size of swap file
 * @param fd file description
 */
static inline void update_eff_size(int fd)
{
	off_t size = lseek(fd, 0, SEEK_CUR);

	if (size > (sizeof(XS_CMD) + sizeof(struct xs_import_hdr)))
	{
		lseek(fd, sizeof(XS_CMD) + offsetof(struct xs_import_hdr, eff_size), SEEK_SET);
		write(fd, &size, sizeof(off_t));
		lseek(fd, size, SEEK_SET);
	}
	else
	{
		XS_CMD cmd;
		struct xs_import_hdr hdr;

		memset(&cmd, 0, sizeof(XS_CMD));
		memset(&hdr, 0, sizeof(struct xs_import_hdr));
		cmd.cmd = CMD_IMPORT_HEADER;
		cmd.blen = sizeof(struct xs_import_hdr);
		hdr.eff_size = sizeof(XS_CMD) + sizeof(struct xs_import_hdr);

		lseek(fd, 0, SEEK_SET);
		write(fd, &cmd, sizeof(XS_CMD));
		write(fd, &hdr, sizeof(struct xs_import_hdr));
		ftruncate(fd, hdr.eff_size);
	}
}

/**
 * Check the effective size of swap
 * @param fd file description
 */
static inline void check_eff_size(int fd)
{
	off_t file_size, eff_size;

	file_size = lseek(fd, 0, SEEK_END);
	lseek(fd, sizeof(XS_CMD) + offsetof(struct xs_import_hdr, eff_size), SEEK_SET);

	if (read(fd, &eff_size, sizeof(off_t)) != sizeof(off_t)
		|| eff_size > file_size || eff_size <= (sizeof(XS_CMD) + sizeof(struct xs_import_hdr)))
	{
		// invalid file, reset import header		
		lseek(fd, 0, SEEK_SET);
		update_eff_size(fd);
	}
	else
	{
		// just update the filesize
		// in fact, we need not truncate file to zero size
		if (eff_size < file_size)
		{
			log_printf("reset filesize (FILE_SIZE:%ld, EFF_SIZE:%ld)", file_size, eff_size);
			ftruncate(fd, eff_size);
		}
		lseek(fd, eff_size, SEEK_SET);
	}
}

/**
 * Begin to rebuild current db
 * @param conn
 * @return CMD_RES_xxx
 */
static int rebuild_conn_wdb(XS_CONN *conn)
{
	XS_DB *db = get_conn_wdb(conn);

	log_debug_conn("rebuild current wdb (DB:%p)", db);
	if (db == NULL)
		return CONN_RES_ERR(NODB);
	else if (db->flag & XS_DBF_STUB)
		return CMD_RES_UNIMP;
	else if (db->flag & XS_DBF_REBUILD_BEGIN)
		return CONN_RES_ERR(REBUILDING);
	else
	{
		// NOTE: may need to clean db_name.re, .rcv[.X] first?
		// clean un-committed data (.rcv)
		if (db->fd >= 0)
		{
			log_debug_conn("clean uncommitted index data for rebuilding");
			lseek(db->fd, 0, SEEK_SET);
			update_eff_size(db->fd);
			db->count = db->lcount = 0;
		}
		if (db->pid > 0 && !(db->flag & XS_DBF_TOCLEAN) && !kill(db->pid, SIGTERM))
		{
			// BUG: if import exit normal HERE may cause some problem	
			db->flag |= XS_DBF_REBUILD_WAIT;
			log_debug_conn("notify running import to quit & set rebuild_wait flag");
		}
		db->flag |= XS_DBF_REBUILD_BEGIN;
		return CONN_RES_OK(DB_REBUILD);
	}
}

/**
 * Clean current db
 * @param conn
 * @return CMD_RES_xxx
 */
static int remove_conn_wdb(XS_CONN *conn)
{
	XS_DB *db = get_conn_wdb(conn);

	log_debug_conn("clean current wdb (DB:%p)", db);
	if (db == NULL)
		return CONN_RES_ERR(NODB);
	else if (db->flag & XS_DBF_STUB)
		return CMD_RES_UNIMP;
	else if (db->flag & XS_DBF_REBUILD_BEGIN)
		return CONN_RES_ERR(REBUILDING);
	else
	{
		// NOTE: path buffer must bigger than:
		// sizeof((XS_USER *)->home) + sizeof((XS_DB *)->name) + 1
		char dbpath[256];
		struct stat st;

		sprintf(dbpath, "%s/%s", conn->user->home, db->name);
		if (stat(dbpath, &st))
		{
			log_conn("failed to stat path of database (PATH:%s, ERROR:%s)", dbpath, strerror(errno));
			return errno == ENOENT ? CONN_RES_OK(DB_CLEAN) : CONN_RES_ERR(STAT);
		}
		if (!S_ISDIR(st.st_mode))
		{
			// stub database unsupported
			log_debug_conn("dbpath is not a regular directory, unsupported");
			db->flag |= XS_DBF_STUB;
			return CMD_RES_UNIMP;
		}

		// clean un-committed data (.rcv)
		db->flag |= XS_DBF_FORCE_COMMIT; // force commit after cleaning
		if (db->fd >= 0)
		{
			log_debug_conn("clean uncommitted index data");
			lseek(db->fd, 0, SEEK_SET);
			update_eff_size(db->fd);
			db->count = db->lcount = 0;
		}

		if (db->pid > 0 && !kill(db->pid, SIGTERM))
		{
			// BUG: if import exit normal HERE, wdb has not DBF_TOCLEAN, may cause some problem
			// import program running (notify it to quit, set clean flag), clean ASYNC
			db->flag |= XS_DBF_TOCLEAN;
			log_debug_conn("notify running import to quit & set clean flag");

			return CONN_RES_OK(DB_CLEAN);
		}
		else
		{
			// remove the database from disk
			log_debug_conn("removing database directory (PATH:%s)", dbpath);
			if (rmdir_r(dbpath) == 0)
				return CONN_RES_OK(DB_CLEAN);

			log_conn("failed to remove database (PATH:%s, ERROR:%s)", dbpath, strerror(errno));
			return CONN_RES_ERR(REMOVE_DB);
		}
	}
}

/**
 * Save current request data into rcvfile
 * @param conn
 * @return CMD_RES_xxx
 */
static int save_conn_request(XS_CONN *conn)
{
	int rc = CMD_RES_CONT;
	off_t off;
	XS_CMD *cmd = conn->zcmd;
	XS_DB *db;

	// load default db
	if ((db = get_conn_wdb(conn)) == NULL)
	{
		rc = CONN_RES_ERR(NODB);
		goto save_end;
	}

	// open the rcvfile
	if (db->fd < 0)
	{
		char rcvfile[128];

		sprintf(rcvfile, DEFAULT_TEMP_DIR "%s_%s.rcv", conn->user->name, db->name);
		if ((db->fd = open(rcvfile, O_RDWR | O_CREAT, 0600)) < 0)
		{
			log_conn("failed to open rcvfile (PATH:%s, ERROR:%s)", rcvfile, strerror(errno));
			rc = CONN_RES_ERR(OPEN_FILE);
			goto save_end;
		}
		log_debug_conn("check file header for rcvfile (FILE:%s)", rcvfile);
		check_eff_size(db->fd);
	}

	// parse & sav ethe commands
	off = lseek(db->fd, 0, SEEK_CUR);
	if (cmd->cmd == CMD_INDEX_EXDATA)
	{
		char *buf = XS_CMD_BUF(cmd);
		unsigned int off = 0, blen = XS_CMD_BLEN(cmd);

		// NOTICE: cmd pointer was changed
		while ((blen - off) >= sizeof(XS_CMD))
		{
			cmd = (XS_CMD *) (buf + off);
			off += XS_CMD_SIZE(cmd);

			// buf data not enough
			if (off > blen) break;
			// skip exdata cmd
			if (cmd->cmd == CMD_INDEX_EXDATA) continue;
			// check to skip other cmd? (CMD_DOC_xxx, CMD_INDEX_REQUEST, REMOVE, SUBMIT)
			if (cmd->cmd != CMD_INDEX_REQUEST && !IS_RQST_CMD(cmd->cmd))
				continue;

			// write the data
			if (safe_write(db->fd, cmd, XS_CMD_SIZE(cmd)) != 0)
			{
				rc = CMD_RES_IOERR;
				break;
			}

			// add count (submit|remove)
			if (cmd->cmd == CMD_INDEX_SUBMIT || cmd->cmd == CMD_INDEX_REMOVE)
				db->count++;
		}
	}
	else if (cmd->cmd == CMD_INDEX_REMOVE)
	{
		if (safe_write(db->fd, cmd, XS_CMD_SIZE(cmd)) == 0)
			db->count++;
		else
			rc = CMD_RES_IOERR;
	}
	else if (cmd->cmd == CMD_INDEX_SUBMIT)
	{
		XS_CMDS *cmds;

		// save cmds
		for (cmds = conn->zhead; cmds != NULL; cmds = cmds->next)
		{
			if (safe_write(db->fd, cmds->cmd, XS_CMD_SIZE(cmds->cmd)) != 0)
				break;
		}
		// save submit cmd
		if (cmds != NULL || safe_write(db->fd, cmd, XS_CMD_SIZE(cmd)) != 0)
			rc = CMD_RES_IOERR;
		else
			db->count++;
	}
	// update effective size or restore offset
	if (rc == CMD_RES_IOERR)
	{
		lseek(db->fd, off, SEEK_SET);
		rc = CONN_RES_ERR(IOERR);
		goto save_end;
	}
	update_eff_size(db->fd);

	// check to commit
	if (db->count >= queue_size && db->pid == 0)
	{
		log_printf("auto commit (DB:%s.%s, COUNT:%d)", conn->user->name, db->name, db->count);
		db_import_call(db, conn->user);
	}
#if SIZEOF_OFF_T < 8
	else if ((db->count - db->lcount) > queue_size)
	{
		struct stat st;

		db->lcount = db->count;
		if (!fstat(db->fd, &st) && st.st_size > MAX_SPLIT_SIZE)
		{
			char rcvfile[128], rcvfile2[128], *suffix;
			int i = 1;

			// get filename
			sprintf(rcvfile, DEFAULT_TEMP_DIR "%s_%s.rcv", conn->user->name, db->name);
			strcpy(rcvfile2, rcvfile);
			suffix = rcvfile2 + strlen(rcvfile2);

			do
			{
				sprintf(suffix, ".%d", i++);
			}
			while (access(rcvfile2, R_OK) == 0);

			log_printf("auto split index data (FILE:%s, COUNT:%d)", rcvfile2, db->count);
			close(db->fd);
			db->fd = -1;
			db->count = db->lcount = 0;

			// rename the file
			if (rename(rcvfile, rcvfile2) != 0)
			{
				log_printf("failed to rename splitted file (FILE:%s, ERROR:%s)",
					rcvfile2, strerror(errno));
			}
		}
	}
#endif	/* LAREFILE */

	// finished
	rc = CONN_RES_OK(RQST_FINISHED);

save_end:
	// clean cmds & return
	conn_free_cmds(conn);
	return rc;
}

/**
 * execute zcmd
 * @return CMD_RES_xxx (CONT|PAUSE|...errors...)
 */
static int index_zcmd_exec(XS_CONN *conn)
{
	int rc = CMD_RES_CONT;
	XS_CMD *cmd = conn->zcmd;

	// check rqst cmd, allow: CMD_INDEX_SUBMIT, CMD_DOC_TERM, CMD_DOC_INDEX, CMD_DOC_VALUE
	if ((conn->flag & CONN_FLAG_IN_RQST) && !IS_RQST_CMD(cmd->cmd))
	{
		if (!XS_CMD_DONT_ANS(cmd))
			return CONN_RES_ERR(WRONGPLACE);
		return rc;
	}

	// handler other commands
	switch (cmd->cmd)
	{
		case CMD_DELETE_PROJECT:
			// delete current project
			log_debug_conn("try to delete project (USER:%s, HOME:%s)", conn->user->name, conn->user->home);
			if (rmdir_r(conn->user->home) != 0)
			{
				log_conn("unable to remove user home completely (HOME:%s, ERROR:%s)",
					conn->user->home, strerror(errno));
				rc = CONN_RES_ERR(REMOVE_HOME);
			}
			else
			{
				xs_user_del(conn->user->name);
				conn->user = NULL;
				conn->wdb = NULL;
				rc = CONN_RES_OK(PROJECT_DEL);
			}
			break;
			// force to flush logging
		case CMD_FLUSH_LOGGING:
			if (import_num >= MAX_IMPORT_NUM)
				rc = CONN_RES_ERR(BUSY);
			else
			{
				log_conn("force to call xs-logging (USER:%s)", conn->user->name);
				xs_logging_call(conn->user);
				rc = CONN_RES_OK(LOG_FLUSHED);
			}
			break;
		case CMD_INDEX_REBUILD:
			// begin to rebuild db
			if (cmd->arg1 == 0)
				rc = rebuild_conn_wdb(conn);
			else
			{
				if (!conn->wdb || !(conn->wdb->flag & XS_DBF_REBUILD_BEGIN))
					rc = CONN_RES_ERR(WRONGPLACE);
				else
				{
					if (conn->wdb->pid > 0 || conn->wdb->count > 0)
					{
						conn->wdb->flag |= (XS_DBF_REBUILD_END | XS_DBF_FORCE_COMMIT);
						log_debug_conn("save the rebuild end flag for next committing (PID:%d,COUNT:%d,FLAG:0x%04x)",
							conn->wdb->pid, conn->wdb->count, conn->wdb->flag);
					}
					else
					{
						rebuild_wdb_end(conn->wdb, conn->user);
					}
					rc = CONN_RES_OK(DB_REBUILD);
				}
			}
			break;
		case CMD_INDEX_CLEAN_DB:
			// clean database
			rc = remove_conn_wdb(conn);
			break;
		case CMD_INDEX_GET_DB:
			// get current db info
			if (get_conn_wdb(conn) == NULL)
				rc = CONN_RES_ERR(NODB);
			else
			{
				char buf[256];

				snprintf(buf, sizeof(buf) - 1, "{\"name\":\"%s\", \"flag\":%d, \"fd\":%d, \"count\":%d, \"pid\":%d}",
					conn->wdb->name, conn->wdb->flag, conn->wdb->fd, conn->wdb->count, conn->wdb->pid);
				rc = CONN_RES_OK2(DB_INFO, buf);
			}
			break;
		case CMD_INDEX_SET_DB:
			// set current db
		{
			char *name = XS_CMD_BUF(cmd);
			int len = XS_CMD_BLEN(cmd);

			rc = xs_user_check_name(name, len);
			if (rc == CMD_ERR_EMPTY)
				rc = CONN_RES_ERR(EMPTY);
			else if (rc == CMD_ERR_TOOLONG)
				rc = CONN_RES_ERR(TOOLONG);
			else if (rc == CMD_ERR_INVALIDCHAR)
				rc = CONN_RES_ERR(INVALIDCHAR);
			else
			{
				XS_DB *db = xs_user_get_db(conn->user, name, len);

				if (db == NULL)
					rc = CONN_RES_ERR(NODB);
				else
				{
					conn->wdb = db;
					rc = CONN_RES_OK(DB_CHANGED);
				}
			}
		}
			break;
			// force to commit current db
		case CMD_INDEX_COMMIT:
			if (get_conn_wdb(conn) == NULL)
				rc = CONN_RES_ERR(NODB);
			else if (conn->wdb->pid != 0)
				rc = CONN_RES_ERR(RUNNING);
			else if (import_num >= MAX_IMPORT_NUM)
				rc = CONN_RES_ERR(BUSY);
			else
			{
				log_conn("force to commit database (DB:%s.%s)", conn->user->name, conn->wdb->name);
				db_import_call(conn->wdb, conn->user);
				rc = CONN_RES_OK(DB_COMMITED);
			}
			break;
			// request + ... (DOC) ... + submit => respond
		case CMD_INDEX_REQUEST:
			conn->flag |= CONN_FLAG_IN_RQST;
			rc = CMD_RES_CONT | CMD_RES_SAVE;
			break;
			// submit, remove, bat export data
		case CMD_INDEX_SUBMIT:
		case CMD_INDEX_REMOVE:
		case CMD_INDEX_EXDATA:
			if (cmd->cmd == CMD_INDEX_SUBMIT)
				conn->flag &= ~CONN_FLAG_IN_RQST;
			rc = save_conn_request(conn);
			break;
			// doc commands, just saved into cmd list
		case CMD_DOC_TERM:
		case CMD_DOC_VALUE:
		case CMD_DOC_INDEX:
			rc = CMD_RES_CONT | CMD_RES_SAVE;
			break;
			// others, passed to next handler
		default:
			rc = CMD_RES_NEXT;
			break;
	}
	return rc;
}

/**
 * Timeout handler of listening server
 */
static void index_server_timeout()
{
	db_commit_check();
	if (import_num < MAX_IMPORT_NUM)
		xs_logging_call(NULL);
}

/**
 * Main function(entrance)
 * @param argc
 * @param argv
 */
int main(int argc, char *argv[])
{
	char prog_path[PATH_MAX], *ctrl = NULL;
	const char *bind, *home, *bpath;
	int cc;

	// init the global value
	home = PREFIX;
	bind = DEFAULT_BIND_PATH;
	queue_size = DEFAULT_QUEUE_SIZE;
	bpath = DEFAULT_BIN_PATH;

	time(&time_logging);
	time_logging -= 3600;

#ifndef HAVE_SETPROCTITLE
	save_main_args(argc, argv);
#endif

	// get prog_name & prog_path
	realpath(argv[0], prog_path);
	if ((prog_name = strrchr(argv[0], '/')) != NULL) prog_name++;
	else prog_name = argv[0];

	log_debug("parse arguments");
	// parse arguments, NOTE: optarg maybe changed by setproctitle()
	while ((cc = getopt(argc, argv, "FvhH:b:k:l:q:e:?")) != -1)
	{
		switch (cc)
		{
			case 'F': main_flag |= FLAG_FOREGROUND;
				break;
			case 'H': home = optarg;
				break;
			case 'b': bind = optarg;
				break;
			case 'k': ctrl = optarg;
				break;
			case 'l':
				if (log_open(optarg, NULL) < 0)
					fprintf(stderr, "WARNING: failed to open log file (FILE:%s)\n", optarg);
				break;
			case 'q':
				queue_size = atoi(optarg);
				if (queue_size < 1)
					queue_size = DEFAULT_QUEUE_SIZE;
				break;
			case 'e': bpath = optarg;
				break;
			case 'v':
				show_version();
				break;
			case 'h':
				show_usage();
				break;
			case '?':
			default:
				fprintf(stderr, "Use `-h' option to get more help messages\n");
				goto main_end;
		}
	}

	// check home directory
	if (chdir(home) < 0)
	{
		fprintf(stderr, "ERROR: failed to change work directory (DIR:%s, ERROR:%s)\n",
			home, strerror(errno));
		goto main_end;
	}
	// Check the temporary directory: tmp/ (writable required)
	if (access(DEFAULT_TEMP_DIR, W_OK) < 0)
	{
		fprintf(stderr, "ERROR: temp directory not exists or not writable (DIR:" DEFAULT_TEMP_DIR ")\n");
		goto main_end;
	}
	// check the import/logging binary executable file
	snprintf(xs_import, sizeof(xs_import), "%s/xs-import", bpath);
	snprintf(xs_logging, sizeof(xs_logging), "%s/xs-logging", bpath);
	if (access(xs_import, X_OK) < 0)
	{
		fprintf(stderr, "ERROR: xs-import program check failed (FILE:%s, ERROR:%s)\n",
			xs_import, strerror(errno));
		goto main_end;
	}
	if (access(xs_logging, X_OK) < 0)
	{
		fprintf(stderr, "ERROR: xs-logging program check failed (FILE:%s, ERROR:%s)\n",
			xs_logging, strerror(errno));
		goto main_end;
	}

	// Just run the control signal `-k'
	if (ctrl != NULL)
		pcntl_kill(bind, ctrl, prog_name);

	// basic setup: mask, signal, log_id
	umask(022);
	log_setid("indexd");
	pcntl_base_signal();

	// become daemon or not?
	log_debug("start the server (FLAG: 0x%04x)", main_flag);
	if (!(main_flag & FLAG_FOREGROUND))
		pcntl_daemon();
	else
	{
		log_open("stderr", NULL);
		fprintf(stderr, "WARNING: run on foreground, log messages are redirected to <stderr>\n");
	}

	// check running & save the pid
	log_debug("check running and save pid");
	if ((cc = pcntl_running(bind, 1)) != 0)
	{
		if (cc > 0)
			log_printf("ERROR: the server is running (BIND_ON:%s)", bind);
		else
			log_printf("ERROR: unable to save the pid (ERROR:%s)", strerror(errno));
		goto main_end;
	}

	// init global variables
	log_debug("init global states");
	G_INIT(0);
	G_VAR_ALLOC(user_base, void *);
	xs_user_init();
	main_flag |= FLAG_G_INITED;

	// create socket server (NOTE: should before setproctitle)
	if ((cc = conn_server_listen(bind)) < 0)
	{
		log_printf("socket server listen/bind failed");
		goto main_end;
	}

	// change the process title
	setproctitle("server");

	// start the listen server
	conn_server_init();
	conn_server_set_zcmd_handler(index_zcmd_exec);
	conn_server_set_timeout_handler(index_server_timeout);
	conn_server_start(cc);

	// finished gracefully
	main_flag |= FLAG_NO_ERROR;
	// end the main
main_end:
	log_debug("main end");
	main_cleanup();
	exit(main_flag & FLAG_NO_ERROR ? 0 : -1);
}
