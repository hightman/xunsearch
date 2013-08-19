/**
 * Project/User
 *
 * $Id$
 */

#ifdef HAVE_CONFIG_H
#    include "config.h"
#endif

#include <string.h>
#include <strings.h>

#include "global.h"
#include "log.h"
#include "user.h"
#include "xs_cmd.h"

#define	IS_VALID_CHAR(x)		(((x)>='0'&&(x)<='9')||((x)>='a'&&(x)<='z')||(x)=='-'||(x)=='_')
#define	STRING_EQUAL(s, d, l)	(!strncasecmp(s, d, l) && s[l] == '\0')

G_VAR_DECL_EX(user_base, void *);

/**
 * Free all the memory allocated for the user
 * @param user
 */
static inline void xs_user_free(XS_USER *user)
{
	XS_DB *db;

	while ((db = user->db) != NULL) {
		user->db = db->next;
		DEBUG_G_FREE(db);
	}
	DEBUG_G_FREE(user);
}

/**
 * Check is it an invalid name string [only allow: 0-9a-z-_]
 * @return CMD_ERR_xxx or CMD_OK
 */
int xs_user_check_name(const char *name, int len)
{
	if (len <= 0) {
		return CMD_ERR_EMPTY;
	}
	if (len >= XS_MAX_NAME_LEN) {
		return CMD_ERR_TOOLONG;
	}

	while (len--) {
		if (!IS_VALID_CHAR(name[len])) {
			return CMD_ERR_INVALIDCHAR;
		}
	}
	return CMD_OK;
}

/**
 * Initialize head of the user chain list
 */
void xs_user_init()
{
	log_debug("init the global user cache");
	G_VAR(user_base) = NULL;
}

/**
 * Free all resources associated with user chain
 */
void xs_user_deinit()
{
	XS_USER *user, *next;

	log_debug("deinit the global user cache");
	next = (XS_USER *) (G_VAR(user_base));
	while ((user = next) != NULL) {
		next = user->next;
		xs_user_free(user);
	}
}

/**
 * Get user from memory cache chain by its name & length
 * @param name name of the user
 * @param len length of the name
 * @return XS_USER pointer on success, NULL on failure
 */
XS_USER *xs_user_nget(const char *name, int len)
{
	XS_USER *user;

	user = (XS_USER *) (G_VAR(user_base));
	for (; user != NULL; user = user->next) {
		if (STRING_EQUAL(user->name, name, len)) {
			return user;
		}
	}
	return NULL;
}

/**
 * Save user to memory cache chain
 * @param user data of user to save
 * @return XS_USER pointer in memory or NULL on failure
 */
XS_USER *xs_user_put(XS_USER *user)
{
	XS_USER *new_user;

	G_LOCK_USER();
	if ((new_user = xs_user_nget(user->name, strlen(user->name))) == NULL) {
		DEBUG_G_MALLOC(new_user, sizeof(XS_USER), XS_USER);
		if (new_user == NULL) {
			log_error("G_MALLOC failed when create new user (SIZE:%d)", sizeof(XS_USER));
		} else {
			memcpy(new_user, user, sizeof(XS_USER));
			new_user->next = G_VAR(user_base);
			G_VAR(user_base) = new_user;
		}
	}
	G_UNLOCK_USER();

	return new_user;
}

/**
 * Delete user data from memory cache by its name
 * @param name
 */
void xs_user_del(const char *name)
{
	XS_USER *user;

	G_LOCK_USER();
	user = (XS_USER *) (G_VAR(user_base));
	if (!strcasecmp(user->name, name)) {
		G_VAR(user_base) = user->next;
		DEBUG_G_FREE(user);
	} else {
		XS_USER *prev;
		while (user->next != NULL) {
			prev = user;
			user = user->next;

			if (!strcasecmp(user->name, name)) {
				prev->next = user->next;
				xs_user_free(user);
				break;
			}
		}
	}
	G_UNLOCK_USER();
}

/**
 * Get DB of an user by its name
 * If not exists, create the DB and add to the list of memory
 * @param user
 * @param name
 * @param len
 * @return XS_DB pointer in memory or NULL on failure
 */
XS_DB *xs_user_get_db(XS_USER *user, const char *name, int len)
{
	XS_DB *db;

	G_LOCK_USER();
	for (db = user->db; db != NULL; db = db->next) {
		if (STRING_EQUAL(db->name, name, len)) {
			break;
		}
	}
	if (db == NULL && len <= XS_MAX_NAME_LEN) {
		DEBUG_G_MALLOC(db, sizeof(XS_DB), XS_DB);
		if (db != NULL) {
			// NOTE: Please ensure that len is less than XS_MAX_NAME_LEN+1
			memset(db, 0, sizeof(XS_DB));
			memcpy(db->name, name, len);
			db->next = user->db;
			db->flag = XS_DBF_FORCE_COMMIT;
			db->scws_multi = DEFAULT_SCWS_MULTI;
			db->fd = -1;
			user->db = db;
		}
	}
	G_UNLOCK_USER();

	return db;
}

