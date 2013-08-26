/**
 * Connection related things
 * Server listen handler functions
 * 
 * $Id$
 */
#ifdef HAVE_CONFIG_H
#    include "config.h"
#endif

#include <unistd.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/un.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <fcntl.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#include "global.h"
#include "log.h"
#include "conn.h"

/**
 * Type definitions & variable/function declarations
 */
struct xs_server
{
	int flag; // server flag
	struct event listen_ev;
	struct event pipe_ev;
	struct timeval tv; // timeout of listening socket
	unsigned int max_accept; // max accept number for the server
	unsigned int num_accept; // current accepted socket number
	unsigned int num_burst; // number of burst connection now
	unsigned int max_burst; // max burst number
	unsigned int num_task; // number of thread tasks
	time_t uptime; // start time

	zcmd_exec_t zcmd_handler; // called to execute zcmd
	void (*pause_handler)(XS_CONN *); // called to run external task
	void (*timeout_handler)(); // called when listening socket timeout
};

static struct xs_server conn_server;
static int pipe_fd[2];
static pthread_mutex_t pipe_mutex;
static void client_ev_cb(int fd, short event, void *arg);

/**
 * Quick macros
 */
#define	CONN_EVENT_ADD()	event_add(&conn->ev, (conn->tv.tv_sec > 0 ? &conn->tv : NULL))

/**
 * Check is it a pure numeric string [0-9]
 */
static inline int is_numeric(char *s)
{
	do {
		if (*s < '0' || *s > '9')
			return 0;
	} while (*++s);
	return 1;
}

/**
 * Free cmds list of connection
 * @param conn
 */
void conn_free_cmds(XS_CONN *conn)
{
	XS_CMDS *cmds;

	while ((cmds = conn->zhead) != NULL) {
		conn->zhead = cmds->next;
		debug_free(cmds->cmd);
		debug_free(cmds);
	}
	conn->ztail = conn->zhead;
}

/**
 * Write data to connection socket (Equivalent to blocking mode)
 * @return zero on success or -1 on failure
 */
int conn_data_send(XS_CONN *conn, void *buf, int size)
{
	int n = 0;

	// forced to flush
	if (buf == NULL) {
		log_debug_conn("flush response (SIZE:%d)", conn->snd_size);
		if (conn->snd_size == 0) {
			return 0;
		}
		buf = conn->snd_buf;
		size = conn->snd_size;
		conn->snd_size = 0;
		goto send_try;
	}

	// check to flush
	if (size > (sizeof(conn->snd_buf) - conn->snd_size) && CONN_FLUSH() != 0) {
		return -1;
	}

	// check to copy
	if (size <= (sizeof(conn->snd_buf) - conn->snd_size)) {
		memcpy(conn->snd_buf + conn->snd_size, buf, size);
		conn->snd_size += size;
		return 0;
	}

	// TODO: HERE may cause blocking, replaced with EV_WRITE events in future
send_try:
	if ((n = send(CONN_FD(), buf, size, 0)) != size) {
		if (n > 0) {
			size -= n;
			buf = (char *) buf + n;
			goto send_try;
		}

		if (errno == EINTR || errno == EAGAIN) {
			if (errno == EAGAIN) {
				log_info_conn("got EAGAIN on sending data (SIZE:%d)", size);
				usleep(5000);
			}
			goto send_try;
		}
		return -1;
	}
	return 0;
}

/**
 * Send respond command
 * XS_CMD = { cmd, arg>>8&0xff, arg&0xff, 0, len } + buf
 * @return CMD_RES_CONT | CMD_RES_IOERR
 */
int conn_respond(XS_CONN *conn, int cmd, int arg, const char *buf, int len)
{
	XS_CMD xcmd;

	len = buf == NULL ? 0 : (len == 0 ? strlen(buf) : len);
	xcmd.cmd = cmd;
	xcmd.blen1 = 0;
	xcmd.blen = len;
	XS_CMD_SET_ARG(&xcmd, arg);
	conn->last_res = (unsigned short) arg;

	// send cmd header
	if (conn_data_send(conn, &xcmd, sizeof(XS_CMD)) != 0) {
		return CMD_RES_IOERR;
	}

	// send cmd buffer
	if (len > 0 && conn_data_send(conn, (void *) buf, len) != 0) {
		return CMD_RES_IOERR;
	}

	return CMD_RES_CONT;
}

/**
 * Quit connection
 * @return CMD_RES_QUIT
 */
int conn_quit(XS_CONN *conn, int res)
{
	switch (res) {
		case CMD_RES_CLOSED:
			log_info_conn("quit, closed by client");
			break;
		case CMD_RES_IOERR:
			log_error_conn("quit, IO error (ERROR:%s)", strerror(errno));
			break;
		case CMD_RES_NOMEM:
			log_error_conn("quit, out of memory");
			break;
		case CMD_RES_TIMEOUT:
			log_warning_conn("quit, IO timeout (TIMEOUT:%d)", (int) conn->tv.tv_sec);
			break;
		case CMD_RES_STOPPED:
			log_notice_conn("quit, server stopped");
			break;
		case CMD_RES_QUIT:
			log_info_conn("quit, normally");
			break;
		case CMD_RES_ERROR:
			log_warning_conn("quit, result error (CODE:%d)", (int) conn->last_res);
			break;
		case CMD_RES_OTHER:
		default:
			log_warning_conn("quit, unknown reason (RES:%d)", res);
			break;
	}

	// flush all output buffer
	if (res != CMD_RES_IOERR) {
		CONN_FLUSH();
	}

	// check to free zcmd
	if (conn->zcmd != NULL && (conn->flag & CONN_FLAG_ZMALLOC)) {
		debug_free(conn->zcmd);
	}

	// check to free cmds group
	conn_free_cmds(conn);
	// close socket & free-self
	close(CONN_FD());

	debug_free(conn);
	if (conn_server.num_burst > 0) {
		conn_server.num_burst--;
	}
	return CMD_RES_QUIT;
}

/**
 * Create new connection
 * @param sock
 * @return XS_CONN *
 */
XS_CONN *conn_new(int sock)
{
	XS_CONN *conn;

	debug_malloc(conn, sizeof(XS_CONN), XS_CONN);
	if (conn == NULL) {
		log_error("not enough memory to create connection (SOCK:%d)", sock);
		close(sock);
		return NULL;
	} else {
		int val = 1;

		// set socket option
		setsockopt(sock, IPPROTO_TCP, TCP_NODELAY, (void *) &val, sizeof(val));
		fcntl(sock, F_SETFL, O_NONBLOCK);

		// put to event list
		memset(conn, 0, sizeof(XS_CONN));
		conn->tv.tv_sec = CONN_TIMEOUT;
		event_assign(&conn->ev, event_get_base(&conn_server.listen_ev), sock, EV_READ, client_ev_cb, conn);
		CONN_EVENT_ADD();
		log_debug_conn("add connection to event base (CONN:%p, SOCK:%d)", conn, sock);

		conn_server.num_burst++;
		if (conn_server.num_burst > conn_server.max_burst) {
			conn_server.max_burst = conn_server.num_burst;
		}
		return conn;
	}
}

/**
 * Read data from connection socket
 * @param conn
 * @return bytes read-in (maybe -1 or 0 on failure)
 */
int conn_data_recv(XS_CONN *conn)
{
	int len, n, to_zcmd = 0;
	char *buf;

	if (conn->zcmd != NULL && conn->rcv_size == 0 && conn->zcmd_left > sizeof(conn->rcv_buf)) {
		len = conn->zcmd_left;
		buf = XS_CMD_BUFTAIL(conn->zcmd) - len;
		to_zcmd = 1;
		log_debug_conn("recv data into big zcmd (SIZE:%d)", len);
	} else {
		len = sizeof(conn->rcv_buf) - conn->rcv_size;
		buf = conn->rcv_buf + conn->rcv_size;
		log_debug_conn("recv data into conn buffer (SIZE:%d)", len);
	}

recv_try:
	if ((n = recv(CONN_FD(), buf, len, 0)) < 0) {
		if (errno == EINTR) {
			goto recv_try;
		}
		if (errno != EAGAIN) {
			log_debug_conn("failed to recv data (SIZE:%d, ERROR:%s)", len, strerror(errno));
		}
	} else if (n > 0) {
		log_debug_conn("data received (EXPECT:%d, ACTUAL:%d)", len, n);
		if (to_zcmd == 1) {
			conn->zcmd_left -= n;
		} else {
			conn->rcv_size += n;
		}
	}
	return n;
}

/* zcmd exec from table, table end with special cmd: CMD_DEFAULT */
int conn_zcmd_exec_table(XS_CONN *conn, zcmd_exec_tab *table)
{
	while (table->func != NULL) {
		if (table->cmd == conn->zcmd->cmd || table->cmd == CMD_DEFAULT) {
			return(*table->func)(conn);
		}
		table++;
	}
	return CMD_RES_NEXT;
}

/**
 * Sample code of zcmd handler (last called in handlers)
 * @return CMD_RES_CONT (UNIMP,SAVE,CONT,IORERR,NOMEM,PAUSE,QUIT,OTHER,ERROR...)
 */
static int conn_zcmd_last(XS_CONN *conn)
{
	int rc = CMD_RES_UNIMP;
	XS_CMD *cmd = conn->zcmd;

	// parse global command type [last]
	if (cmd->cmd == CMD_USE) {
		// buf=name, buf1=home
		char *name = XS_CMD_BUF(cmd);
		char *home = XS_CMD_BUF1(cmd);
		int name_len = XS_CMD_BLEN(cmd);
		int home_len = XS_CMD_BLEN1(cmd);
		XS_USER *user = xs_user_nget(name, name_len);

		log_debug_conn("load user from cache (USER:%p, NAME:%.*s)", user, name_len, name);
		if (user != NULL) {
			// replace new home directory
			if (home_len > 0 && home_len < sizeof(user->home)) {
				log_notice_conn("replace user home (NAME:%s, HOME:%.*s", user->name, home_len, home);
				memcpy(user->home, home, home_len);
				user->home[home_len] = '\0';
			}
		} else {
			XS_USER new_user;

			rc = xs_user_check_name(name, name_len);
			if (rc == CMD_ERR_EMPTY) {
				rc = CONN_RES_ERR(EMPTY);
			} else if (rc == CMD_ERR_TOOLONG) {
				rc = CONN_RES_ERR(TOOLONG);
			} else if (rc == CMD_ERR_INVALIDCHAR) {
				rc = CONN_RES_ERR(INVALIDCHAR);
			} else {
				struct stat st;

				memset(&new_user, 0, sizeof(XS_USER));
				memcpy(new_user.name, name, name_len);

				if (home_len == 0 || home_len >= sizeof(new_user.home)) {
					sprintf(new_user.home, DEFAULT_DATA_DIR "%s", new_user.name);
				} else {
					while (home_len > 1 && home[home_len - 1] == '/') home_len--;
					memcpy(new_user.home, home, home_len);
				}
				log_debug_conn("build new user (NAME:%s, HOME:%s)", new_user.name, new_user.home);

				// error check for home directory
				if (!stat(new_user.home, &st)) {
					// exists, but it is not a directory
					if (!S_ISDIR(st.st_mode)) {
						log_error_conn("invalid user home directory (HOME:%s)", new_user.home);
						return CONN_RES_ERR(INVALID_HOME);
					}
				} else if (mkdir(new_user.home, 0755) < 0) {
					// not exists, failed to create directory
					log_error_conn("failed to create user home (HOME:%s, ERROR:%s)", new_user.home, strerror(errno));
					return CONN_RES_ERR(CREATE_HOME);
				}

				// save the user to memory cache
				if ((user = xs_user_put(&new_user)) == NULL) {
					rc = CONN_RES_ERR(NOMEM);
				}
			}
		}
		if (user != NULL) {
			log_info_conn("project changed (NAME:%s)", user->name);
			conn->user = user;
			conn->wdb = NULL;
			rc = CONN_RES_OK(PROJECT);
		}
	}
	return rc;
}

/**
 * Global code of zcmd handler
 * @return CMD_RES_xxx 
 */
static int conn_zcmd_first(XS_CONN *conn)
{
	int rc = CMD_RES_NEXT;
	XS_CMD *cmd = conn->zcmd;

	// check project
	if (conn->user == NULL && cmd->cmd != CMD_USE && cmd->cmd != CMD_QUIT && cmd->cmd != CMD_TIMEOUT) {
		log_warning_conn("project not specified (CMD:%d)", cmd->cmd);
		return XS_CMD_DONT_ANS(cmd) ? CMD_RES_CONT : CONN_RES_ERR(NOPROJECT);
	}

	// parse global command type [first]
	if (cmd->cmd == CMD_QUIT) {
		rc = CMD_RES_QUIT;
	} else if (cmd->cmd == CMD_DEBUG) {
		// show debug info for some special usage (this may cause buffer overflow)
		char buf[4096];
		XS_CMDS *cmds;
		XS_DB *db;
		int len = 0;
		int uptime = time(NULL) - conn_server.uptime;

		// basic info
		len += sprintf(&buf[len], "{\n  id:\"%s\", uptime:%d, num_burst:%u, max_burst:%u,\n  "
				"num_accept:%u, aps:%.1f, num_task:%u, tps:%.1f,\n  "
				"sock:%d, name:\"%s\", home:\"%s\", rcv_size:%d,\n  "
				"flag:0x%04x, version:\"" PACKAGE_VERSION "\"\n}\n",
				log_ident(NULL), uptime, conn_server.num_burst, conn_server.max_burst,
				conn_server.num_accept, (float) conn_server.num_accept / uptime,
				conn_server.num_task, (float) conn_server.num_task / uptime,
				CONN_FD(), conn->user->name, conn->user->home, conn->rcv_size, conn->flag);
		// db list
		len += sprintf(&buf[len], "DBS:");
		for (db = conn->user->db; db != NULL; db = db->next) {
			len += sprintf(&buf[len], " [%s] ->", db->name);
		}
		len += sprintf(&buf[len], " [NULL]\nCMDS:\n");
		// cmd list
		for (cmds = conn->zhead; cmds != NULL && len < (sizeof(buf) - 256); cmds = cmds->next) {
			len += sprintf(&buf[len], "  -> {cmd:%d,arg1:%d,arg2:%d,blen1:%d,blen:%d}\n",
					cmds->cmd->cmd, cmds->cmd->arg1, cmds->cmd->arg2,
					cmds->cmd->blen1, cmds->cmd->blen);
		}
		if (cmds == NULL) {
			len += sprintf(&buf[len], "  -> {NULL}");
		} else {
			len += sprintf(&buf[len], "  -> ...");
		}
		rc = CONN_RES_OK3(INFO, buf, len);
	} else if (cmd->cmd == CMD_TIMEOUT) {
		// set timeout
		conn->tv.tv_sec = XS_CMD_ARG(cmd);
		rc = CONN_RES_OK(TIMEOUT_SET);
		log_debug_conn("adjust timeout (SEC:%d)", conn->tv.tv_sec);
	}
	return rc;
}

/**
 * Save the current zcmd into cmds list
 * @param conn
 * @return CMD_RES_CONT/CMD_RES_NOMEM
 */
static int conn_zcmd_save(XS_CONN *conn)
{
	XS_CMDS *cmds;

	// change RETURN value to CONT
	log_debug_conn("save command into CMDS (CMD:%d)", conn->zcmd->cmd);

	debug_malloc(cmds, sizeof(XS_CMDS), XS_CMDS);
	if (cmds == NULL) {
		log_error_conn("failed to allocate memory for CMDS (SIZE:%d)", sizeof(XS_CMDS));
		return CMD_RES_NOMEM;
	}

	cmds->next = NULL;
	if (conn->flag & CONN_FLAG_ZMALLOC) {
		// use zcmd directly
		conn->flag ^= CONN_FLAG_ZMALLOC;
		cmds->cmd = conn->zcmd;
	} else {
		// copy zcmd
		debug_malloc(cmds->cmd, XS_CMD_SIZE(conn->zcmd), XS_CMD);
		if (cmds->cmd != NULL) {
			memcpy(cmds->cmd, conn->zcmd, XS_CMD_SIZE(conn->zcmd));
		} else {
			log_error_conn("failed to allocate memory for CMDS->cmd (CMD:%d, SIZE:%d)", conn->zcmd->cmd, XS_CMD_SIZE(conn->zcmd));
			debug_free(cmds);
			return CMD_RES_NOMEM;
		}
	}

	// add the cmd to chain of cmds
	if (conn->zhead == NULL) {
		conn->ztail = conn->zhead = cmds;
	} else {
		conn->ztail->next = cmds;
		conn->ztail = cmds;
	}
	return CMD_RES_CONT;
}

/**
 * Execute zcmd of connection
 * @param conn connection pointer
 * @param func callback handler
 * @return CMD_RES_xxx
 */
int conn_zcmd_exec(XS_CONN *conn, zcmd_exec_t func)
{
	int i, rc = CMD_RES_CONT;
	zcmd_exec_t handlers[] = {conn_zcmd_first, func, conn_server.zcmd_handler, conn_zcmd_last};

	for (i = 0; i < sizeof(handlers) / sizeof(zcmd_exec_t); i++) {
		if (handlers[i] == NULL) {
			continue;
		}
		rc = (*handlers[i])(conn);
		log_debug_conn("execute zcmd (CMD:%d, FUNC[%d]:%p, RET:0x%04x)",
				conn->zcmd->cmd, i, handlers[i], rc);
		if (rc != CMD_RES_NEXT) {
			break;
		}
	}
#ifndef DEBUG
	log_info_conn("zcmd executed (CMD:%d, RET:0x%04x)", conn->zcmd->cmd, rc);
#endif

	// check special flag
	if (rc & CMD_RES_SAVE) // save zcmd into cmds chain
	{
		int rc2 = conn_zcmd_save(conn);
		if (rc2 != CMD_RES_CONT) {
			rc = rc2;
		}
	}
	rc &= CMD_RES_MASK;

	// handle special return value
	if (rc == CMD_RES_UNIMP) {
		// Not implemented
		log_warning_conn("command not implemented (CMD:%d)", conn->zcmd->cmd);
		rc = XS_CMD_DONT_ANS(conn->zcmd) ? CMD_RES_CONT : CONN_RES_ERR(UNIMP);
	}

	// check to free the zcmd (quit cmd)
	if (conn->flag & CONN_FLAG_ZMALLOC) {
		conn->flag ^= CONN_FLAG_ZMALLOC;
		debug_free(conn->zcmd);
	}
	conn->zcmd = NULL;

	// retrun the RC value
	return rc;
}

/**
 * Parse incoming data & execute commands
 * Called after data received.
 * @return CMD_RES_xxx (PAUSE|CONT|...quits...)
 */
int conn_cmds_parse(XS_CONN *conn, zcmd_exec_t func)
{
	int off = 0, rc = CMD_RES_CONT;

	// check zcmd
	if (conn->zcmd != NULL) {
		if (conn->zcmd_left > 0 && conn->rcv_size > 0) {
			char *buf = XS_CMD_BUFTAIL(conn->zcmd) - conn->zcmd_left;

			off = (conn->zcmd_left > conn->rcv_size ? conn->rcv_size : conn->zcmd_left);
			memcpy(buf, conn->rcv_buf, off);
			log_debug_conn("copy rcv_buf to zcmd (SIZE:%d, RCV_SIZE:%d, ZCMD_LEFT:%d)",
					off, conn->rcv_size, conn->zcmd_left);
			conn->zcmd_left -= off;
		}

		// execute zcmd (otherwise: rcv_size - off <= 0)
		if (conn->zcmd_left == 0) {
			rc = conn_zcmd_exec(conn, func);
		}
	}

	// parse the cmd from rcv_buf
	while (rc == CMD_RES_CONT
			&& (conn->rcv_size - off) >= sizeof(XS_CMD)) {
		conn->zcmd = (XS_CMD *) (conn->rcv_buf + off);
		off += sizeof(XS_CMD);

		log_debug_conn("get command {cmd:%d,arg1:%d,arg2:%d,blen1:%d,blen:%d}",
				conn->zcmd->cmd, conn->zcmd->arg1, conn->zcmd->arg2,
				conn->zcmd->blen1, conn->zcmd->blen);

		// check the zcmd is full or not
		if ((XS_CMD_BUFSIZE(conn->zcmd) + off) > conn->rcv_size) {
			XS_CMD *cmd;

			debug_malloc(cmd, XS_CMD_SIZE(conn->zcmd), XS_CMD);
			if (cmd == NULL) {
				log_error_conn("failed to allocate memory for ZCMD (CMD:%d, SIZE:%d)",
						conn->zcmd->cmd, XS_CMD_SIZE(conn->zcmd));

				conn->zcmd = NULL;
				off -= sizeof(XS_CMD); // reset offset to cmd header
				rc = CMD_RES_NOMEM;
			} else {
				memcpy(cmd, conn->zcmd, conn->rcv_size - off + sizeof(XS_CMD));
				conn->zcmd_left = XS_CMD_BUFSIZE(conn->zcmd) - (conn->rcv_size - off);
				conn->zcmd = cmd;
				conn->flag |= CONN_FLAG_ZMALLOC; // current zcmd must be free
				off = conn->rcv_size;
				log_debug_conn("wait left data of zcmd (CMD:%d, ZCMD_LEFT:%d)",
						cmd->cmd, conn->zcmd_left);
			}
			break;
		}

		// execute the zcmd (on rcv_buffer)
		off += XS_CMD_BUFSIZE(conn->zcmd);
		rc = conn_zcmd_exec(conn, func);
	}

	// flush the send buffer
	// **IMPORTANT** skip ioerr & pause
	// otherwise, if ioerr here will cause async task broken
	if (rc != CMD_RES_IOERR && rc != CMD_RES_PAUSE && CONN_FLUSH() != 0) {
		rc = CMD_RES_IOERR;
	}

	// move the buffer
	conn->rcv_size -= off;
	if (conn->rcv_size > 0 && off > 0) {
		memcpy(conn->rcv_buf, conn->rcv_buf + off, conn->rcv_size);
	}

	// return the rc
	return rc;
}

/**
 * Client event callback
 * Parse simple command & wait for a longer command
 */
static void client_ev_cb(int fd, short event, void *arg)
{
	XS_CONN *conn = (XS_CONN *) arg;
	log_debug_conn("run client event callback (EVENT:0x%04x)", event);

	// read event
	if (event & EV_READ) {
		int rc;
ev_try:
		rc = CONN_RECV();
		if (rc < 0) {
			if (errno == EAGAIN) {
				CONN_EVENT_ADD();
				return;
			}
			rc = CMD_RES_IOERR;
		} else if (rc == 0) {
			rc = CMD_RES_CLOSED;
		} else {
			rc = conn_cmds_parse(conn, NULL);
			log_debug_conn("parsed and executed incoming commands (RET:0x%04x)", rc);
		}
		switch (rc) {
			case CMD_RES_CONT:
				goto ev_try;
			case CMD_RES_PAUSE:
				// task should start safely from HERE
				log_debug_conn("connection paused to run other async task");
				(*conn_server.pause_handler)(conn);
				return;
			default:
				// CMD_RES_QUIT,CMD_RES_CLOSED, CMD_RES_IOERR
				// CMD_RES_NOMEM, CMD_RES_ERROR, CMD_RES_OTHER
				conn_quit(conn, rc);
				return;
		}
	}

#if 0
	// TODO: write event, write snd_buf until EAGAIN returned
	if (event & EV_WRITE) {

	}
#endif

	// timeout event
	if (event & EV_TIMEOUT) {
		CONN_QUIT(TIMEOUT);
	}
}

/**
 * Server event callback
 * acccept & create new connection
 */
static void server_ev_cb(int fd, short event, void *arg)
{
	log_debug("run server event callback (EVENT:0x%04x)", event);

	// read event
	if (event & EV_READ) {
		struct sockaddr_in sin;
		int sock, val = sizeof(sin);

		sock = accept(fd, (struct sockaddr *) &sin, (socklen_t *) & val);
		log_debug("accept new connection (FD:%d, RET:%d, ERRNO:%d)", fd, sock, errno);
		if (sock < 0) {
			if (errno != EINTR && errno != EWOULDBLOCK) {
				conn_server_shutdown();
				log_error("accept() failed, shutdown gracefully (ERROR:%s)", strerror(errno));
			}
		} else {
			conn_server.num_accept++;
			if (conn_new(sock) != NULL) {
				log_info("new connection (SOCK:%d, IP:%s, BURST:%d)",
						sock, inet_ntoa(sin.sin_addr), conn_server.num_burst);
			}
		}

		// add the listen event
		if ((conn_server.flag & (CONN_SERVER_STOPPED | CONN_SERVER_TIMEOUT)) == CONN_SERVER_TIMEOUT) {
			event_add(&conn_server.listen_ev, &conn_server.tv);
		}

		// check to stop
		if (conn_server.max_accept > 0 && conn_server.num_accept >= conn_server.max_accept) {
			log_notice("reach max accept number[%d], notify server stop", conn_server.max_accept);
			conn_server_push_back(NULL);
		}
	}

	// timeout event
	if (event & EV_TIMEOUT) {
		// add the listen event
		if (!(conn_server.flag & CONN_SERVER_STOPPED)) {
			event_add(&conn_server.listen_ev, &conn_server.tv);
		}

		log_debug("server loop timeout (CALLBACK:%p)", conn_server.timeout_handler);
		if (conn_server.timeout_handler != NULL) {
			(*conn_server.timeout_handler)();
		}
	}
}

/**
 * Pipe callback(get conn from sub-threads)
 * NOTE: NULL can be pushed to stop listen server
 */
static void pipe_ev_cb(int fd, short event, void *arg)
{
	log_debug("run pipe event callback (EVENT:0x%04x)", event);
	if (event & EV_READ) {
		XS_CONN *conn;

		while (read(fd, &conn, sizeof(XS_CONN *)) == sizeof(XS_CONN *)) {
			log_info("pull connection from pipe (CONN:%p)", conn);
			if (conn == NULL) {
				if (conn_server.flag & CONN_SERVER_STOPPED) {
					continue;
				}
				log_info("get NULL from pipe, shutdown gracefully");
				conn_server_shutdown();
			} else {
				if (conn_server.flag & CONN_SERVER_STOPPED) {
					CONN_QUIT(STOPPED);
				} else {
					log_info("revival paused connection (CONN:%p)", conn);
					CONN_EVENT_ADD();
				}
			}
		}
	}
}

/**
 * init the global conn server (called before starting server)
 */
void conn_server_init()
{
	memset(&conn_server, 0, sizeof(conn_server));
	conn_server.zcmd_handler = NULL;
	conn_server.tv.tv_sec = (CONN_TIMEOUT << 2);
	time(&conn_server.uptime);
}

/**
 * shutdown the listen server
 */
void conn_server_shutdown()
{
	log_info("shutdown the listen server");
	conn_server.flag |= CONN_SERVER_STOPPED;
	event_del(&conn_server.listen_ev);
	event_del(&conn_server.pipe_ev);
	close(event_get_fd(&conn_server.listen_ev));
}

/**
 * set default zcmd handler
 */
void conn_server_set_zcmd_handler(zcmd_exec_t func)
{
	conn_server.zcmd_handler = func;
}

/**
 * set pause handler, called when the rc of zcmd_exec is CMD_RES_PAUSE
 */
void conn_server_set_pause_handler(void (*func)(XS_CONN *))
{
	conn_server.pause_handler = func;
}

/**
 * set timeout handler
 */
void conn_server_set_timeout_handler(void (*func)())
{
	conn_server.timeout_handler = func;
}

/** 
 * set timeout in seconds
 */
void conn_server_set_timeout(int sec)
{
	conn_server.tv.tv_sec = sec;
}

/**
 * set conn server running as multi-threads supported
 */
void conn_server_set_multi_threads()
{
	conn_server.flag |= CONN_SERVER_THREADS;
}

/**
 * get number of all accepted request for the server
 */
int conn_server_get_num_accept()
{
	return conn_server.num_accept;
}

/**
 * set the max number of request to be processed
 */
void conn_server_set_max_accept(int max_accept)
{
	conn_server.max_accept = max_accept > 0 ? max_accept : 0;
}

/**
 * increase num task of server
 */
void conn_server_add_num_task(int num)
{
	//if (conn_server.flag & CONN_SERVER_THREADS)
	//	pthread_mutex_lock(&pipe_mutex);
	conn_server.num_task += num;
	//if (conn_server.flag & CONN_SERVER_THREADS)
	//	pthread_mutex_unlock(&pipe_mutex);
}

/**
 * Push back connection to listening thread
 * Called in sub-threads!
 * BUG: back_event occur between stop_accept & pipe_cb
 */
void conn_server_push_back(XS_CONN *conn)
{
	log_info("push connection back to pipe (CONN:%p, FLAG:0x%04x)", conn, conn_server.flag);

	if (conn_server.flag & CONN_SERVER_STOPPED) {
		if (conn != NULL) {
			CONN_QUIT(STOPPED);
		}
	} else if (!(conn_server.flag & CONN_SERVER_THREADS)) {
		write(pipe_fd[1], &conn, sizeof(XS_CONN *));
	} else {
		pthread_mutex_lock(&pipe_mutex);
		write(pipe_fd[1], &conn, sizeof(XS_CONN *));
		pthread_mutex_unlock(&pipe_mutex);
	}
}

/**
 * Socket address union
 */
typedef union _sa_t
{
	struct sockaddr sa;
	struct sockaddr_un sun;
	struct sockaddr_in sin;
} sa_t;

/**
 * Bind socket server & start to listen.
 * @param bind_path the address/port or socket path to bind
 *                  format: <port>|<addr:port>|<path>
 * @return integer listen socket fd or -1 on failure.
 */
int conn_server_listen(const char *bind_path)
{
	sa_t sa;
	struct linger ld;
	char host[128], *ptr;
	int sock, sock_len, val, port = 0;

	// parse host:port
	if (is_numeric((char *) bind_path)) {
		port = atoi(bind_path);
		if (port > 0) {
			host[0] = '\0';
		}
	} else if ((ptr = strchr(bind_path, ':')) != NULL) {
		port = atoi(ptr + 1);
		sock_len = ptr - bind_path;
		if (port <= 0 || sock_len >= sizeof(host)) {
			port = 0;
		} else {
			strncpy(host, bind_path, sock_len);
			host[sock_len] = '\0';
		}
	}

	log_info("bind listen server (BIND:%s, HOST:%s, PORT:%d)", bind_path, host, port);
	if (port <= 0) {
		// local unix domain
		int path_len = strlen(bind_path);
		if (path_len >= sizeof(sa.sun.sun_path)) { // path too long
			log_error("local path too long to bind (LEN:%d, MAXLEN:%d)",
					path_len, sizeof(sa.sun.sun_path));
			return -1;
		}

		memset(&sa.sun, 0, sizeof(sa.sun));
		sa.sun.sun_family = PF_LOCAL;
		memcpy(sa.sun.sun_path, bind_path, path_len + 1);
		sock_len = (size_t) (((struct sockaddr_un *) 0)->sun_path) + path_len;

#ifdef HAVE_SOCKADDR_UN_SUN_LEN
		sa.sun.sun_len = sock_len;
#endif
		unlink(bind_path);
	} else {
		// tcp socket
		memset(&sa.sin, 0, sizeof(sa.sin));
		sa.sin.sin_family = PF_INET;
		sa.sin.sin_port = htons(port);
		sock_len = sizeof(sa.sin);
		if (!host[0] || host[0] == '*' || host[0] == '\0') {
			sa.sin.sin_addr.s_addr = htonl(INADDR_ANY);
		} else {
			sa.sin.sin_addr.s_addr = inet_addr(host);
		}
	}

	// create socket
	sock = socket(sa.sa.sa_family, SOCK_STREAM, 0);
	log_info("create the listen socket (SOCK:%d)", sock);
	if (sock < 0) {
		log_error("socket() failed (ERROR:%s)", strerror(errno));
		return -1;
	}

	val = 1;
	setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, (void *) &val, sizeof(val));
	/*
	setsockopt(sock, SOL_SOCKET, SO_KEEPALIVE, (void *) &val, sizeof(val));
	setsockopt(sock, IPPROTO_TCP, TCP_NODELAY, (void *) &val, sizeof(val));
	 */
	ld.l_onoff = ld.l_linger = 0;
	setsockopt(sock, SOL_SOCKET, SO_LINGER, (void *) &ld, sizeof(ld));

	log_info("bind and listen the socket (SOCK:%d)", sock);
	if (bind(sock, (struct sockaddr *) &sa, sock_len) < 0 || listen(sock, DEFAULT_BACKLOG) < 0) {
		log_error("bind() or listen() failed (ERROR:%s)", strerror(errno));
		close(sock);
		sock = -1;
	}

	// set socket file permission
	if (sock >= 0 && port <= 0) {
		chmod(bind_path, 0666);
	}

	return sock;
}

/**
 * Start the accept server
 * @param listen_sock
 */
void conn_server_start(int listen_sock)
{
	short listen_event = (EV_READ | EV_PERSIST);
	struct event_base *base;

	// check socket
	if (listen_sock < 0) {
		log_error("invalid listen socket (SOCK:%d)", listen_sock);
		return;
	}

	// initlize the conn_server flag
	log_debug("init the listen event");
	base = event_base_new();
	if (conn_server.timeout_handler != NULL && conn_server.tv.tv_sec > 0) {
		conn_server.flag |= CONN_SERVER_TIMEOUT;
		listen_event ^= EV_PERSIST;
	}

	// listen event
	fcntl(listen_sock, F_SETFL, O_NONBLOCK);
	event_assign(&conn_server.listen_ev, base, listen_sock, listen_event, server_ev_cb, NULL);

	// pipe event
	log_debug("init the pipe event");
	if (pipe(pipe_fd) != 0) {
		log_error("pipe() failed (ERROR:%s)", strerror(errno));
		close(listen_sock);
		return;
	}
	fcntl(pipe_fd[0], F_SETFL, O_NONBLOCK);
	event_assign(&conn_server.pipe_ev, base, pipe_fd[0], EV_READ | EV_PERSIST, pipe_ev_cb, NULL);

	// thread mutex
	if (conn_server.flag & CONN_SERVER_THREADS) {
		pthread_mutex_init(&pipe_mutex, NULL);
	}

	// add events & start the loop
	log_notice("event loop start (EVENT:0x%04x, FLAG:0x%04x)", listen_event, conn_server.flag);
	event_add(&conn_server.listen_ev, (listen_event & EV_PERSIST) ? NULL : &conn_server.tv);
	event_add(&conn_server.pipe_ev, NULL);
    event_base_dispatch(base);
    event_base_free(base);

	// end the event loop
	log_notice("event loop end");
	if (conn_server.flag & CONN_SERVER_THREADS) {
		pthread_mutex_destroy(&pipe_mutex);
	}
	close(pipe_fd[0]);
	close(pipe_fd[1]);
}
