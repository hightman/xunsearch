/**
 * Global variables (+mm)
 *
 * $Id$
 */

#ifndef __XS_GLOBAL_20090531_H__
#define	__XS_GLOBAL_20090531_H__

#define	LOGGING_CALL_PERIOD	7200		// seconds
#define	DEFAULT_DB_NAME		"db"
#define	DEFAULT_TEMP_DIR	"tmp/"
#define	DEFAULT_DATA_DIR	"data/"
#define	SEARCH_LOG_FILE		"search.log"
#define	CUSTOM_DICT_FILE	"dict_user.txt"
#define	SEARCH_LOG_DB		"log_db"
#define	DEFAULT_BACKLOG		63			// default backlog for listen()
#define	MAX_EXPAND_LEN		15
#define DEFAULT_SCWS_MULTI	3			// default scws multi level

#ifdef HAVE_MM

#include "mm.h"

extern MM *mm_global;

#define	G_DECL()			MM *mm_global
#define	G_INIT(mb)			mm_global = mm_create((mb) <<20)
#define	G_DEINIT()			mm_destroy(mm_global)
#define	G_MALLOC(sz)		mm_malloc(mm_global, sz)
#define	G_FREE(p)			mm_free(mm_global, p)

// global lock, value of N: 1~4
#ifdef DEBUG
#define	G_LOCK(n)			log_debug("G_LOCK(%d)", n); mm_lock##n(mm_global)
#define	G_UNLOCK(n)			log_debug("G_UNLOCK(%d)", n); mm_unlock##n(mm_global)
#else
#define	G_LOCK(n)			mm_lock##n(mm_global)
#define	G_UNLOCK(n)			mm_unlock##n(mm_global)
#endif

#define	G_LOCK_USER()		G_LOCK(1)
#define	G_UNLOCK_USER()		G_UNLOCK(1)

#define	G_LOCK_CACHE()		G_LOCK(2)
#define	G_UNLOCK_CACHE()	G_UNLOCK(2)

// global var define
#define	G_VAR(n)				*n##_var_gl
#define	G_VAR_DECL(n, type)		type *n##_var_gl
#define	G_VAR_DECL_EX(n, type)	extern type *n##_var_gl
#define	G_VAR_PTR(n)			n##_var_gl
#define	G_VAR_ALLOC(n, type)	do {				\
	if (n##_var_gl != NULL) break;					\
	DEBUG_G_MALLOC(n##_var_gl, sizeof(type), type);	\
	memset(n##_var_gl, 0, sizeof(type));			\
} while (0)
#define	G_VAR_FREE(n)			do {				\
	if (n##_var_gl == NULL) break;					\
	DEBUG_G_FREE(n##_var_gl);						\
	n##_var_gl = NULL;								\
} while (0)

#else	/* HAVE_MM */

#include <stdlib.h>

#define	G_DECL()
#define	G_INIT(mb)
#define	G_DEINIT()
#define	G_MALLOC(sz)			malloc(sz)
#define	G_FREE(p)				free(p)

#define	G_LOCK_USER()	
#define	G_UNLOCK_USER()	

#define	G_LOCK_CACHE()
#define	G_UNLOCK_CACHE()

// global var define
#define	G_VAR(n)				n##_var_gl
#define	G_VAR_DECL(n, type)		type n##_var_gl
#define	G_VAR_DECL_EX(n, type)	extern type n##_var_gl
#define	G_VAR_PTR(n)			&n##_var_gl
#define	G_VAR_ALLOC(n, type)
#define	G_VAR_FREE(n)

#endif	/* HAVE_MM */

#ifdef DEBUG
#define	DEBUG_G_MALLOC(p,s,t)	do { \
	p = (t *) G_MALLOC(s); \
	log_debug("G_MALLOC (ADDR:%p, SIZE:%d)", p, s); \
} while(0)
#define	DEBUG_G_FREE(p)			do { \
	log_debug("G_FREE (ADDR:%p)", p); \
	G_FREE(p); \
} while(0)
#else
#define	DEBUG_G_MALLOC(p,s,t)	p = (t *) G_MALLOC(s)
#define	DEBUG_G_FREE			G_FREE
#endif

#define	XS_HACK_UUID			"201109231337"

#endif	/* __XS_GLOBAL_20090531_H__ */
