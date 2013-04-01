/**
 * Project & Database <=> Virtual User
 * 
 * $Id$
 */

#ifndef __XS_USER_20090514_H__
#define	__XS_USER_20090514_H__

#include <sys/types.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Database flag
 */
#define	XS_DBF_TOCLEAN			0x01	// marked to clean
#define	XS_DBF_FORCE_COMMIT		0x02	// forced to commit(first|after clean)
#define	XS_DBF_STUB				0x04	// stub database

#define	XS_DBF_REBUILD_BEGIN	0x08	// index rebuild begin
#define	XS_DBF_REBUILD_END		0x10	// index rebuild end
#define	XS_DBF_REBUILD_WAIT		0x20	// index rebuild begin during import running
#define	XS_DBF_REBUILD_STOP		0x40	// index rebuild forced to stop
#define	XS_DBF_REBUILD_MASK		0x78

#define	XS_MAX_NAME_LEN			32		// max name len

/**
 * Database information (chain-list)
 * Only used for indexd, unused in searchd
 */
typedef struct xs_db
{
	char name[XS_MAX_NAME_LEN];
	// name of database -> $HOME/$name (we can use stub file to support remote db)
	short flag; // db flag, 0x01->to be cleand
	short scws_multi; //scws multi setting
	int fd; // temp fd to save received data
	int count; // count of uncommitted documents
	int lcount; // last count of record point
	pid_t pid; // pid of import process (0 -> not writing)
	time_t ltime; // last commit time

	struct xs_db *next;
} XS_DB;

/**
 * User of connection session
 */
typedef struct xs_user
{
	char name[XS_MAX_NAME_LEN]; // name of user/project
	char home[128]; // home directory of the user/project
	struct xs_db *db; // pointer to my db-list (null?)

	struct xs_user *next;
} XS_USER;

/**
 * global functions
 */
int xs_user_check_name(const char *name, int len); // return CMD_OK or CMD_ERR_xxx
void xs_user_init(); // init
void xs_user_deinit(); // deinit
XS_USER *xs_user_put(XS_USER *user); // save user to cache
XS_USER *xs_user_nget(const char *name, int len); // load user from cache
void xs_user_del(const char *name); // delete user from cache
XS_DB *xs_user_get_db(XS_USER *user, const char *name, int len); // load user db

#ifdef __cplusplus
}
#endif

#endif	/* __XS_USER_20090514_H__ */
