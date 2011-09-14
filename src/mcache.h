/**
 * MyCache implement (Header file)
 * Hash + List/RBTree
 * $Id$
 */

#ifndef __XS_MCACHE_20090609_H__
#define	__XS_MCACHE_20090609_H__

#ifdef __cplusplus
extern "C" {
#endif

#ifndef MM
#define MM	void
#endif

#ifndef MC
#define	MC	void
#endif

// dash type
#define	MC_DASH_CHAIN	0
#define	MC_DASH_RBTREE	1

#define	MC_FLAG_COPY_KEY		1
#define	MC_FLAG_COPY_VALUE		2
#define	MC_FLAG_COPY			3

// MC errno?
#define	MC_OK			0		/* Successfully */
#define	MC_EMEMORY		1		/* out of memory */
#define	MC_EINVALID		2		/* invalid argument */
#define	MC_EFOUND		3		/* not found */
#define	MC_EUNIMP		4		/* Un-Implemented */
#define	MC_EDISALLOW	5		/* Disallowed now */

#define	MC_RB_RED		0
#define	MC_RB_BLACK		1

// create or delete the MCACHE
MC *mc_create(MM *mm); // NULL -> use malloc/free
void mc_destroy(MC *mc);
int mc_set_dash_type(MC *mc, int type); // type = MC_DASH_CHAIN | MC_DASH_RBTREE	(0 or errno)
int mc_set_hash_size(MC *mc, int size); // size -> a big prime number				(0 or errno)
int mc_set_copy_flag(MC *mc, int flag); // mode = 0 | MC_FLAG_ ...

// set max memory, default is: 8 * 1024 * 1024
void mc_set_max_memory(MC *mc, int bytes);

// API
void *mc_get(MC *mc, const char *key);
int mc_put(MC *mc, const char *key, void *value, int vlen); // 0 or errno, life = 0, if copy mode off, vlen no used.
int mc_del(MC *mc, const char *key); // 0 or errno

// get error description
const char *mc_strerror(int err);

#ifdef __cplusplus
}
#endif

#endif	/* __XS_MCACHE_20090609_H__ */
