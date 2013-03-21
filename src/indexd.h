/**
 * Database(index) management
 *
 * $Id$
 */

#ifndef __XS_INDEXD_20090530_H__
#define	__XS_INDEXD_20090530_H__

#define	DEFAULT_QUEUE_SIZE		3000		// default queue size
#define	DEFAULT_BIN_PATH		"./bin"		// default external binary path
#define	DEFAULT_BIND_PATH		"8383"		// default server bind path

#define	MIN_COMMIT_COUNT		100			// number of changed
#define	MIN_COMMIT_TIME			180			// seconds
#define	MAX_IMPORT_NUM			5			// number of concurrent import processes

#if SIZEOF_OFF_T < 8
#define	MAX_SPLIT_FILES			10			// max split files (xxx_xx.rcv.[NUM])
#define	MAX_SPLIT_SIZE			1610612736L	// 1.5GB
#endif	/* LARG FILE */

#endif	/* __XS_INDEXD_20090530_H__ */
