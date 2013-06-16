/**
 * Task header file
 *
 * $Id$
 */

#ifndef __XS_TASK_20110703_H__
#define	__XS_TASK_20110703_H__

#include "conn.h"

/**
 * max number of results per search Query
 * also be used for cache number
 */
#define	MAX_SEARCH_RESULT		100

/**
 * max number of facets search field
 * defined as multiples of 4
 */
#define	MAX_SEARCH_FACETS		8

/**
 * max length of query string for CMD_QUERY_
 */
#define	MAX_QUERY_LENGTH		192

int task_add_search_log(XS_CONN *conn);	// add search log
void task_cancel(void *arg); // called on canceling task
void task_exec(void *arg); // called on executing task
void task_init();	// init task env (worker only)
void task_deinit();	// deinit task env (worker only)

#endif	/* __XS_TASK_20110703_H__ */

