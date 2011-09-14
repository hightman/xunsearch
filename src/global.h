/**
 * Global variables (+mm)
 *
 * $Id$
 */

#ifndef __XS_GLOBAL_20090531_H__
#define	__XS_GLOBAL_20090531_H__

#define	DEFAULT_DB_NAME		"db"
#define	DEFAULT_TEMP_DIR	"tmp/"
#define	DEFAULT_DATA_DIR	"data/"
#define	SEARCH_LOG_FILE		"search.log"
#define	SEARCH_LOG_DB		"log_db"
#define	MAX_EXPAND_LEN		15

#ifdef HAVE_MM

#include "mm.h"

extern MM *mm_global;

#define	G_DECL()			MM *mm_global
#define	G_INIT(mb)			mm_global = mm_create(mb * (1<<20))
#define	G_DEINIT()			mm_destroy(mm_global)
#define	G_MALLOC(sz)		mm_malloc(mm_global, sz)
#define	G_FREE(p)			mm_free(mm_global, p)

// global lock, value of N: 1~4
#define	G_LOCK(n)			mm_lock##n(mm_global)
#define	G_UNLOCK(n)			mm_unlock##n(mm_global)

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
	n##_var_gl = (type *) G_MALLOC(sizeof(type));	\
	memset(n##_var_gl, 0, sizeof(type));			\
} while (0)
#define	G_VAR_FREE(n)			do {				\
	if (n##_var_gl == NULL) break;					\
	G_FREE(n##_var_gl);								\
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

#endif	/* __XS_GLOBAL_20090531_H__ */
