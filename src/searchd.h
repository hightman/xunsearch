/**
 * Header file for search daemon server
 *
 * $Id$
 */

#ifndef __XS_SEARCHD_20090531_H__
#define	__XS_SEARCHD_20090531_H__

// default constant defines
#define	DEFAULT_STEMMER			"english"	// default stemmr (compatiable with importd.h?)
#define	DEFAULT_BIND_PATH		"8384"		// default server bind path
#define	DEFAULT_WORKER_NUM		3			// number of worker process

#define	MAX_THREAD_NUM			32			// max number of work threads
#define	MAX_WORKER_TIME			60			// unit: seconds (< socket_timeout of client)

// worker process protection
//#define	MAX_WORKER_ACCEPT		10000		// 每个工作进程在处理多少个请求后自杀，０不自杀
//#define	MAX_WORKER_LIFE			7200		// 每个工作进程的最大存活时间

// NOTE: cache 的锁定设计可能会导致 bug
// 当 cache 命中并且操作的时候有可能被其它线程/进程删除, 这种情况下的行为无法确定
// 根据 LRU 的策略, 一取得数据就提到最前, 发生的机率不大.
#ifdef HAVE_MEMORY_CACHE
#	define	DEFAULT_MM_SIZE		32	// mm_global+cache (unit: MB)
#else
#	define	DEFAULT_MM_SIZE		4	// smaller if memory cache disabled (mm_global)
#endif

#endif	/* __XS_SEARCHD_20090531_H__ */
