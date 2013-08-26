/**
 * Connection header file
 * 
 * $Id$
 */

#ifndef __XS_CONN_20090514_H__
#define	__XS_CONN_20090514_H__

#include <event2/event.h>
#include <event2/event_struct.h>
#include <sys/time.h>
#include "xs_cmd.h"
#include "user.h"

#ifdef __cplusplus
extern "C" {
#endif

/* if there is no any IO data comming in 5sec, auto disconnect */
#define	CONN_TIMEOUT		5
#define	CONN_BUFSIZE		1024

/* simple macro for connection operator */
#define	CONN_FD()			event_get_fd(&conn->ev)
#define	CONN_RECV()			conn_data_recv(conn)
#define	CONN_FLUSH()		conn_data_send(conn, NULL, 0)

/* ftphp cmd list for connection session */
typedef struct xs_cmds
{
	XS_CMD *cmd;
	struct xs_cmds *next;
} XS_CMDS;

/* TODO: xs io buffer chain */
typedef struct xs_iobuf
{
	char *buf; // buffer pointer
	int size; // buffer total size
	int off; // buffer sent offset (always 0 for non-first buffer)
	struct xs_iobuf *next;
} XS_IOBUF;

/* xs connection agent */
typedef struct xs_conn
{
	struct event ev; // socket fd event struct (libevent)
	struct timeval tv; // timeout setting
	char rcv_buf[CONN_BUFSIZE]; // buffer for network IO. recv
	char snd_buf[CONN_BUFSIZE]; // buffer for network IO. send
	unsigned short rcv_size; // recv buffer size
	unsigned short snd_size; // send buffer size
	unsigned short flag; // some special flag for current connection
	unsigned short last_res; // last respond arg (error code)
	int zcmd_left; // un-finished cmd left?
	XS_CMD *zcmd; // un-finished cmd
	XS_CMDS *zhead, *ztail; // head & tail of cmd group
	XS_USER *user; // the CONN associated with a user?
	XS_DB *wdb; // current writable db
	void *zarg; // arg for zcmd_exec
} XS_CONN;

/* 
 * zcmd_exec handler function type
 * @return CMD_RES_xxx
 * 
 * CMD_RES_CONT: ok, continue to run other cmd
 * CMD_RES_PAUSE: ok, but we should pause to do other things(submit task/end task)
 * CMD_RES_NEXT: nothing to be done, passed to next handler
 * CMD_RES_UNIMP: un-implemented feature
 * return value of: CMD_RES_ERR(...), CMD_RES_OK(...)
 * others: error/quit
 *
 * all return value can combine with following flags
 * CMD_RES_SAVE: save the cmd into cmds-list
 *
 */
typedef int (*zcmd_exec_t)(XS_CONN *);

/* zcmd_exec table (cmd -> handler), special cmd: CMD_DEFAULT [put to last] */
typedef struct zcmd_exec_tab
{
	int cmd;
	zcmd_exec_t func;
} zcmd_exec_tab;

/* free cmds list of connection */
void conn_free_cmds(XS_CONN *conn);

/* send data on conn? -1 ERROR, 0->OK (this is same to blocking mode) */
int conn_data_send(XS_CONN *conn, void *buf, int len);

/* return CMD_RES_CONT on sucess, or CMD_RES_IOERR or ioerror */
int conn_respond(XS_CONN *conn, int cmd, int arg, const char *buf, int len);

/* void conn_quit */
int conn_quit(XS_CONN *conn, int res);

/* create new connection, return NULL on failure */
XS_CONN *conn_new(int sock);

/* recv data on conn, return bytes received, or 0 -> EOF, -1 -> ERROR */
int conn_data_recv(XS_CONN *conn);

/* zcmd exec accord to zcmd table, table should end with NULL */
int conn_zcmd_exec_table(XS_CONN *conn, zcmd_exec_tab *table);

/* zcmd exec */
int conn_zcmd_exec(XS_CONN *conn, zcmd_exec_t func);

/* return last func() retval (but quit have some exception) */
int conn_cmds_parse(XS_CONN *conn, zcmd_exec_t func);

/**
 * socket server bind & listen
 * @param bind_path the address/port or socket path to bind
 *        format: <port>|<addr:port>|<path>
 */
int conn_server_listen(const char *bind_path);

/* init the global conn server (called once) */
void conn_server_init();

/* shutdown listen server */
void conn_server_shutdown();

/* set default zcmd handler */
void conn_server_set_zcmd_handler(zcmd_exec_t func);

/* set pause handler, called when the rc of zcmd_exec is CMD_RES_PAUSE */
void conn_server_set_pause_handler(void (*func)(XS_CONN *));

/* set timeout handler */
void conn_server_set_timeout_handler(void (*func)());

/* set timeout in seconds */
void conn_server_set_timeout(int sec);

/* set conn server running as multi-threads supported */
void conn_server_set_multi_threads();

/* get number of all accepted request for the server */
int conn_server_get_num_accept();

/* set the max number of request to be processed */
void conn_server_set_max_accept(int max_accept);

/* increase num task of server */
void conn_server_add_num_task(int num);

/* push-back the conn event to listening thread */
void conn_server_push_back(XS_CONN *conn);

/* Start the accept server */
void conn_server_start(int listen_sock);

/* notify the accept server to stop listening */
#define conn_server_stop_notify()	conn_server_push_back(NULL)

/* conn flag */
#define	CONN_FLAG_ZMALLOC		0x01	// need to free zcmd
#define	CONN_FLAG_IN_RQST		0x02	// in request
#define	CONN_FLAG_CH_SORT		0x04	// sort result by user-defined type (use to check cache)
#define	CONN_FLAG_CH_DB			0x08	// not default database (use to check cache)
#define	CONN_FLAG_CH_COLLAPSE	0x10	// not default collapse
#define	CONN_FLAG_CACHE_LOCKED	0x20	// G_LOCK_CACHE()
#define	CONN_FLAG_EXACT_FACETS	0x40	// exact facets search
#define	CONN_FLAG_ON_SCWS		0x80	// for scws only
#define	CONN_FLAG_MATCHED_TERM	0x100	// append matched terms in result doc

/* server flag */
#define	CONN_SERVER_THREADS	1		// multi-threads server flag
#define	CONN_SERVER_STOPPED	2		// accept server stopped
#define	CONN_SERVER_TIMEOUT	4		// has timeout?

/* following can be return value of conn_parse_cmd()/conn_zcmd_exec() */
#define	CMD_RES_CONT		0x01	// continue
#define	CMD_RES_PAUSE		0x02	// pause in event(maybe used to task)
#define	CMD_RES_UNIMP		0x03	// un-implemented
#define	CMD_RES_IOERR		0x04	// io error (recv/send)	[quit]
#define	CMD_RES_NOMEM		0x05	// out of memory		[quit]
#define	CMD_RES_NEXT		0x06	// passed to next handler (just for conn_zcmd_global)

/* following can not be return value of conn_parse_cmd()/conn_zcmd_exec() */
#define	CMD_RES_FINISH		0x81	// finish for searchd (convert to cont)
#define	CMD_RES_CLOSED		0x82	// closed by remote		[quit]
#define	CMD_RES_TIMEOUT		0x83	// io timeout			[quit]
#define	CMD_RES_STOPPED		0x84	// accept server stopped[quit]
#define	CMD_RES_QUIT		0x85	// normal quit			[quit]
#define	CMD_RES_ERROR		0x86	// fatal error			[quit]
#define	CMD_RES_OTHER		0x87	// unknown other reason	[quit]

/* special flag */
#define	CMD_RES_MASK		0xff	// mask of real res
#define	CMD_RES_SAVE		0x100	// save the zcmd into cmds

/* CMD_ERR with err_code & msg, such as: CONN_RES_ERR(TIMEOUT) */
#define	CONN_RES_ERR(x)			conn_respond(conn, CMD_ERR, CMD_ERR_CODE(x), CMD_ERR_STR(x), 0)
#define	CONN_RES_ERR2(x,y)		conn_respond(conn, CMD_ERR, CMD_ERR_CODE(x), y, 0)
#define	CONN_RES_ERR3(x,y,z)	conn_respond(conn, CMD_ERR, CMD_ERR_CODE(x), y, z)

/* CMD_OK with a ok_code, such as: CMD_RES(DB_CHANED) */
#define	CONN_RES_OK(x)			conn_respond(conn, CMD_OK, CMD_OK_CODE(x), NULL, 0)
#define	CONN_RES_OK2(x,y)		conn_respond(conn, CMD_OK, CMD_OK_CODE(x), y, 0)
#define	CONN_RES_OK3(x,y,z)		conn_respond(conn, CMD_OK, CMD_OK_CODE(x), y, z)

#define	CONN_QUIT(x)			conn_quit(conn, CMD_RES_##x)

/* logging with socket fd */
#define	log_info_conn(fmt,...)		log_info("[sock:%d] " fmt, CONN_FD(), ##__VA_ARGS__)
#define	log_notice_conn(fmt,...)	log_notice("[sock:%d] " fmt, CONN_FD(), ##__VA_ARGS__)
#define	log_warning_conn(fmt,...)	log_warning("[sock:%d] " fmt, CONN_FD(), ##__VA_ARGS__)
#define	log_error_conn(fmt,...)		log_error("[sock:%d] " fmt, CONN_FD(), ##__VA_ARGS__)

#ifdef DEBUG
#define	log_debug_conn(fmt,...)		log_debug("[sock:%d] " fmt, CONN_FD(), ##__VA_ARGS__)
#define	debug_free(p)				do { \
	log_debug("memory free (ADDR:%p)", p); \
	free(p); \
} while(0)
#define	debug_malloc(p,s,t)			do { \
	p = (t *) malloc(s); \
	log_debug("memory alloc (ADDR:%p, SIZE:%d)", p, s); \
} while(0)
#else
#define	log_debug_conn				log_debug
#define	debug_free(p)				free(p)
#define	debug_malloc(p,s,t)			p = (t *) malloc(s)
#endif

#ifdef __cplusplus
}
#endif

#endif	/* __XS_CONN_20090514_H__ */
