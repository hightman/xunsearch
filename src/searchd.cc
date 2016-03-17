/**
 * XunSearch Search Daemon server
 * Contains: 1*master + N*worker
 * $Id$
 */

#ifdef HAVE_CONFIG_H
#    include "config.h"
#endif

#ifndef HAVE_MM
#    define HAVE_MM	1
#endif

#ifndef	PREFIX
#    define	PREFIX	"."
#endif

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <fcntl.h>
#include <signal.h>
#include <string.h>
#include <limits.h>
#include <sys/param.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>
#include <xapian.h>

#include "conn.h"
#include "log.h"
#include "pcntl.h"
#include "global.h"
#include "pinyin.h"
#include "tpool.h"
#include "task.h"
#include "searchd.h"
#ifdef HAVE_MEMORY_CACHE
#    include "mcache.h"
#endif

/**
 * Flags for main
 */
#define	FLAG_FOREGROUND			0x0001
#define	FLAG_NO_ERROR			0x0002
#define	FLAG_G_INITED			0x0004
#define	FLAG_HAS_CHILD			0x0008
#define	FLAG_MASTER				0x0010
#define	FLAG_KEEPALIVE			0x0020
#define	FLAG_ON_EXIT			0x0040

#define	FLAG_SIG_MASK			0xff00
#define	FLAG_SIG_EXIT			0x0700
#define	FLAG_SIG_EXIT_NORMAL	0x0100
#define	FLAG_SIG_EXIT_GRACEFUL	0x0200
#define	FLAG_SIG_EXIT_EXCEPTION	0x0400
#define	FLAG_SIG_CHILD			0x1000
#define	FLAG_SIG_RELOAD			0x2000
#define	FLAG_SIG_ALARM			0x4000
#define	FLAG_SIG_TSTP			0x8000

#define	RESET_FLAG_SIG()		main_flag &= ~FLAG_SIG_MASK
#define	CHECK_FLAG_SIG(x)		main_flag & FLAG_SIG_##x

/**
 * Global variables
 */
G_DECL();
G_VAR_DECL(user_base, void *);

#ifdef HAVE_MEMORY_CACHE
MC *mc;
#endif

Xapian::Stem stemmer;
Xapian::SimpleStopper *stopper;

/**
 * Local static variables
 */
typedef struct
{
	pid_t pid;
	time_t chrono; // born time
} worker_t;

static worker_t * volatile worker_pids;
static volatile int main_flag;
static char *prog_name;
static int worker_num, listen_sock;

/**
 * Thread pool
 */
static tpool_t thr_pool;

#define	TPOOL_INIT()			tpool_init(&thr_pool, MAX_THREAD_NUM, 0, 0)
#define	TPOOL_DEINIT()			tpool_destroy(&thr_pool)
#define	TPOOL_CANCEL()			tpool_cancel(&thr_pool)
#define	TPOOL_ADD_TASK()		tpool_exec(&thr_pool, task_exec, task_cancel, conn)
#define	TPOOL_KILL_TIMEOUT()	tpool_cancel_timeout(&thr_pool, MAX_WORKER_TIME)
#define	TPOOL_LOG_STATUS()		log_info_conn("new task (USER:%s, SPARE:%d, TOTAL:%d)", \
	conn->user->name, thr_pool.cur_spare, thr_pool.cur_total)

/**
 * Macros define
 */
#define	IS_MASTER()				(main_flag & FLAG_MASTER)

/**
 * Worker basic zcmd handler (trigger task & save commands)
 * @param conn
 * @return CMD_RES_xxx
 */
static int worker_zcmd_exec(XS_CONN *conn)
{
	switch (conn->zcmd->cmd) {
		case CMD_SEARCH_DB_TOTAL:
		case CMD_SEARCH_GET_TOTAL:
		case CMD_SEARCH_GET_RESULT:
		case CMD_SEARCH_GET_SYNONYMS:
		case CMD_QUERY_GET_STRING:
		case CMD_QUERY_GET_TERMS:
		case CMD_QUERY_GET_CORRECTED:
		case CMD_QUERY_GET_EXPANDED:
		case CMD_SEARCH_SET_DB:
		case CMD_SEARCH_ADD_DB:
		case CMD_SEARCH_GET_DB:
		case CMD_SEARCH_SCWS_GET:
			if (conn->zcmd->cmd == CMD_SEARCH_SCWS_GET)
				conn->flag |= CONN_FLAG_ON_SCWS;
			// paused in event server, submit task to thread pool
			return CMD_RES_PAUSE | CMD_RES_SAVE;
		case CMD_SEARCH_SET_SORT:
		case CMD_SEARCH_SET_CUT:
		case CMD_SEARCH_SET_NUMERIC:
		case CMD_SEARCH_SET_COLLAPSE:
		case CMD_SEARCH_SET_FACETS:
		case CMD_SEARCH_SET_CUTOFF:
		case CMD_SEARCH_SET_MISC:
		case CMD_QUERY_INIT:
		case CMD_QUERY_PARSE:
		case CMD_QUERY_TERM:
		case CMD_QUERY_TERMS:
		case CMD_QUERY_RANGEPROC:
		case CMD_QUERY_RANGE:
		case CMD_QUERY_VALCMP:
		case CMD_QUERY_PREFIX:
		case CMD_QUERY_PARSEFLAG:
		case CMD_SEARCH_SCWS_SET:
			// save the command
			return CMD_RES_CONT | CMD_RES_SAVE;
		case CMD_SEARCH_ADD_LOG:
			return task_add_search_log(conn);
		case CMD_SEARCH_FINISH:
			return CONN_RES_ERR(WRONGPLACE);
		case CMD_SEARCH_KEEPALIVE:
			if (conn->zcmd->arg1 == 0) {
				main_flag &= ~FLAG_KEEPALIVE;
			} else {
				main_flag |= FLAG_KEEPALIVE;
			}
			return CMD_RES_CONT;
		case CMD_SEARCH_DRAW_TPOOL:
		{
			// draw thread pool for debugging
			char *buf = tpool_draw(&thr_pool);
			int rc = CONN_RES_OK2(INFO, buf);
			free(buf);
			return rc;
		}
	}
	// others, passed to next handler
	return CMD_RES_NEXT;
}

/**
 * Worker pause handler, submit task to thread pool
 * @param conn
 */
static void worker_submit_task(XS_CONN *conn)
{
	if (main_flag & FLAG_ON_EXIT) {
		log_notice("ignore task on terminating");
		CONN_QUIT(STOPPED);
	} else {
		TPOOL_LOG_STATUS();
		TPOOL_ADD_TASK();
	}
}

/**
 * Worker server timeout
 */
static void worker_server_timeout()
{
	if (!(main_flag & FLAG_KEEPALIVE)) {
		int num = TPOOL_KILL_TIMEOUT();
		log_debug("check to kill timed out threads (NUM:%d)", num);
	}
}

/**
 * Worker cleanup on exit
 */
static void worker_cleanup()
{
	// wait tpool end without error
	if (main_flag & FLAG_NO_ERROR) {
		log_info("cancel and wait thread pool");
		TPOOL_CANCEL();
	}
	// destroy the tpool
	log_info("deinit thread pool");
	TPOOL_DEINIT();
}

/**
 * Worker shutdown gracefully
 */
static void worker_shutdown()
{
	conn_server_push_back(NULL);
}

/**
 * Worker start point
 * @param listen_sock
 */
static void worker_start()
{
	// init the thread pool
	log_info("init thread pool");
	TPOOL_INIT();

	// start the listen server
	conn_server_init();
	conn_server_set_zcmd_handler(worker_zcmd_exec);
	conn_server_set_pause_handler(worker_submit_task);
	conn_server_set_timeout_handler(worker_server_timeout);
#ifdef MAX_WORKER_ACCEPT
	conn_server_set_max_accept(MAX_WORKER_ACCEPT);
#endif
	conn_server_start(listen_sock);
}

/**
 * Show version information
 */
static void show_version()
{
	printf("%s: %s/%s (search server)\n", prog_name, PACKAGE_NAME, PACKAGE_VERSION);
	exit(0);
}

/**
 * Usage help
 */
static void show_usage()
{
	printf("%s (%s/%s) - Search Response Server\n", prog_name, PACKAGE_NAME, PACKAGE_VERSION);
	printf("Copyright (C)2007-2011 hightman, HangZhou YunSheng Network Co., Ltd.\n\n");

	printf("Usage: %s [options]\n", prog_name);
	printf("  -F               Run the server on foreground (non-daemon)\n");
	printf("  -H <home>        Specify the working directory\n");
	printf("                   Default: " PREFIX "\n");
	printf("  -L <log_level>   Larger the value to get more information, (1-7, default: %d)\n", LOG_NOTICE);
	printf("  -b <port>|<address:port>|<path>\n");
	printf("                   Bind the server to adddress/port or path, (default: " DEFAULT_BIND_PATH ")\n");
	printf("  -l <log_file>    Specify the log output file, (default: none)\n");
	printf("                   E.g: " DEFAULT_TEMP_DIR "%s.log, stderr\n", prog_name);
	printf("  -m <size>MB      Set the size of global shared memory, (default: %dMB)\n", DEFAULT_MM_SIZE);
	printf("  -n <num>         Set the number of worker processes to spawn, (default: %d)\n", DEFAULT_WORKER_NUM);
	printf("  -s <stopfile>    Specify the path to stop words list\n");
	printf("                   Default: none, refer to etc/stopwords.txt under install directory\n");
	printf("  -t <stemmer>     Specify the stemmer language, (default: " DEFAULT_STEMMER ")\n");
	printf("  -k [fast]<stop|start|restart|reload> Server process running control\n");
	printf("  -v               Show version information\n");
	printf("  -h               Display this help page\n\n");
	printf("Compiled with xapian-core-scws-" XAPIAN_VERSION "\n");
	printf("Report bugs to " PACKAGE_BUGREPORT "\n");
	exit(0);
}

/**
 * Cleanup then exit
 */
static inline void main_cleanup()
{
	// add exit flag for other signal handler (child)
	if (main_flag & FLAG_ON_EXIT) {
		return;
	}
	main_flag |= FLAG_ON_EXIT;
	conn_server_shutdown();

	if (IS_MASTER()) {
		// master
		if (worker_pids != NULL) {
			free(worker_pids);
		}
#ifdef HAVE_MEMORY_CACHE
		if (mc != NULL) {
			log_debug("deinit memory cache");
			mc_destroy(mc);
		}
#endif
		if (main_flag & FLAG_G_INITED) {
			log_debug("deinit global states");
			xs_user_deinit();
			G_VAR_FREE(user_base);
			G_DEINIT();
		}
	} else {
		// worker
		worker_cleanup();
	}

	// deinit task env
	log_debug("deinit task env");
	task_deinit();

	// others
	log_debug("unload the pinyin dict");
	py_dict_unload();
	log_close();
}

/**
 * Signal handlers should be compiled in C-style
 */
#ifdef __cplusplus
extern "C" {
#endif

/**
 * Termination handler
 */
int signal_term(int sig)
{
	log_alert("caught %ssignal[%d], terminate immediately",
			(sig == SIGTERM ? "" : "exceptional "), sig);
	if (IS_MASTER()) {
		main_flag |= (sig == SIGTERM ? FLAG_SIG_EXIT_NORMAL : FLAG_SIG_EXIT_EXCEPTION);
		return SIGNAL_TERM_LATER;
	} else {
		if (sig == SIGTERM) {
			main_cleanup();
		}
		return main_flag & FLAG_NO_ERROR ? 0 : -1;
	}
}

/**
 * Child process reaper
 */
void signal_child(pid_t pid, int status)
{
	if (!IS_MASTER()) {
		log_notice("unknown child process exit (PID:%d, STATUS:%d)", pid, status);
	} else {
		int i;
		for (i = 1; i <= worker_num; i++) {
			if (worker_pids[i].pid == pid) {
				worker_pids[i].pid = 0;
				break;
			}
		}
		main_flag |= FLAG_SIG_CHILD;
		log_alert("child process worker[%d] exit (PID:%d, STATUS:%d)",
				i > worker_num ? -1 : i, pid, status);
	}
}

/**
 * Shutdown gracefully
 */
void signal_int()
{
	log_alert("caught SIGINT, shutdown gracefully");
	if (IS_MASTER()) {
		main_flag |= FLAG_SIG_EXIT_GRACEFUL;
	} else {
		// FIXME: signal received before conn server become ready
		worker_shutdown();
	}
}

/**
 * Reload handler
 */
void signal_reload(int sig)
{
	if (sig == SIGHUP) {
		log_notice("caught reload SIGHUP, but nothing to do");
	}
	if (IS_MASTER()) {
		if (sig == SIGTSTP) {
			main_flag |= FLAG_SIG_TSTP;
		} else if (sig == SIGALRM) {
			main_flag |= FLAG_SIG_ALARM;
		} else {
			main_flag |= FLAG_SIG_RELOAD;
		}
	} else if (sig == SIGTSTP) {
		log_printf("accepted requests (NUM:%d)", conn_server_get_num_accept());
	}
}

#ifdef __cplusplus
}
#endif

/**
 * Become worker
 */
static void become_worker(int idx, sigset_t *sigmask)
{
	// child worker
	char ident[32];

	usleep(5000);
	sprintf(ident, "worker%d", idx);
	log_ident(ident);
	setproctitle("worker[%d]", idx);

	// worker, restore signal mask
	sigprocmask(SIG_SETMASK, sigmask, NULL);
	main_flag ^= FLAG_MASTER;

	// TODO: release more unused resources
	free(worker_pids);

	// increase scheduling priority
	nice(3);

	// start the worker
	log_alert("worker server start");
	worker_start();

	// end the worker
	main_flag |= FLAG_NO_ERROR;
	main_cleanup();
	_exit(0);
}

/**
 * Spawning worker process
 * @param idx
 * @param sigmask
 */
static void spawn_worker(int idx, sigset_t *sigmask)
{
	pid_t pid = fork();

	if (pid < 0) {
		log_error("failed to spawn worker[%d] (ERROR:%s)", idx, strerror(errno));
	} else if (pid == 0) {
		// child worker
		become_worker(idx, sigmask);
	} else {
		// parent master
		worker_pids[idx].pid = pid;
		time(&worker_pids[idx].chrono);
		log_alert("spawned child worker[%d] (PID:%d)", idx, pid);
	}
}

/**
 * Main function(entrance)
 * @param argc
 * @param argv
 */
int main(int argc, char *argv[])
{
	int cc, msize;
	char prog_path[PATH_MAX], *ctrl = NULL;
	const char *bind, *home;
	sigset_t oldmask, tmpmask;

	// init the global value
	home = PREFIX;
	bind = DEFAULT_BIND_PATH;
	msize = DEFAULT_MM_SIZE;
	worker_num = DEFAULT_WORKER_NUM;
	stemmer = Xapian::Stem(DEFAULT_STEMMER);
	main_flag = FLAG_MASTER;
	stopper = NULL;

#ifndef HAVE_SETPROCTITLE
	save_main_args(argc, argv);
#endif

	// get prog_name & prog_path
	realpath(argv[0], prog_path);
	if ((prog_name = strrchr(argv[0], '/')) != NULL) prog_name++;
	else prog_name = argv[0];

	log_debug("parse arguments");
	// parse arguments, NOTE: optarg maybe changed by setproctitle()
	while ((cc = getopt(argc, argv, "FvhH:L:b:l:m:n:s:t:k:?")) != -1) {
		switch (cc) {
			case 'F': main_flag |= FLAG_FOREGROUND;
				break;
			case 'H': home = optarg;
				break;
			case 'L': log_level(atoi(optarg));
				break;
			case 'b': bind = optarg;
				break;
			case 'k': ctrl = optarg;
				break;
			case 'l':
				if (log_open(optarg, NULL, -1) < 0) {
					fprintf(stderr, "WARNING: failed to open log file (FILE:%s)\n", optarg);
				}
				break;
			case 'm':
				msize = atoi(optarg);
				if (msize < 1 || msize > 128) {
					fprintf(stderr, "ERROR: invalid global shared memory size (VALID:1~128)\n");
					goto main_end;
				}
				break;
			case 'n':
				worker_num = atoi(optarg);
				if (worker_num < 1) {
					worker_num = DEFAULT_WORKER_NUM;
				}
				break;
			case 's':
			{
				FILE *fp = fopen(optarg, "r");
				if (fp == NULL) {
					fprintf(stderr, "WARNING: stopwords file not found (FILE:%s)\n", optarg);
				} else {
					char buf[64], *ptr;
					stopper = new Xapian::SimpleStopper();
					buf[sizeof(buf) - 1] = '\0';
					while (fgets(buf, sizeof(buf) - 1, fp) != NULL) {
						if (buf[0] == ';' || buf[0] == '#' || buf[0] == '\r' || buf[0] == '\n') {
							continue;
						}
						ptr = buf + strlen(buf) - 1;
						while (ptr > buf && (*ptr == ' ' || *ptr == '\t' || *ptr == '\r' || *ptr == '\n')) {
							ptr--;
						}
						ptr[1] = '\0';
						ptr = buf;
						if (*ptr == '\0') {
							continue;
						}
						while (*ptr == ' ' || *ptr == '\t') {
							ptr++;
						}
						stopper->add(ptr);
					}
					fclose(fp);
				}
			}
				break;
			case 't':
				try {
					stemmer = Xapian::Stem(optarg);
				} catch (...) {
					fprintf(stderr, "ERROR: invalid stemmer language (LANG:%s)\n", optarg);
					goto main_end;
				}
				break;
			case 'v':
				show_version();
				break;
			case 'h':
				show_usage();
				break;
			case '?':
			default:
				fprintf(stderr, "Use `-h' option to get more help messages\n");
				goto main_end;
		}
	}

	// check home directory
	if (chdir(home) < 0) {
		fprintf(stderr, "ERROR: failed to change work directory (DIR:%s, ERROR:%s)\n",
				home, strerror(errno));
		goto main_end;
	}
	// Check the temporary directory: tmp/ (writable required)
	if (access(DEFAULT_TEMP_DIR, W_OK) < 0) {
		fprintf(stderr, "ERROR: temp directory not exists or not writable (DIR:" DEFAULT_TEMP_DIR ")\n");
		goto main_end;
	}

	// Just run the control signal `-k'
	if (ctrl != NULL) {
		pcntl_kill(bind, ctrl, prog_name);
	}

	// basic setup: mask, signal, log_id
	umask(022);
	log_ident("searchd");

	// become daemon or not?
	log_debug("main start (FLAG: 0x%04x)", main_flag);
	if (!(main_flag & FLAG_FOREGROUND)) {
		pcntl_daemon();
	} else {
		log_open("stderr", NULL, -1);
		fprintf(stderr, "WARNING: run on foreground, logs are redirected to <stderr>\n");
	}

	// check running & save the pid
	log_debug("check running and save pid");
	if ((cc = pcntl_running(bind, 1)) != 0) {
		if (cc > 0) {
			log_error("server is running (BIND:%s, PID:%d)", bind, cc);
		} else {
			log_error("failed to save the pid (ERROR:%s)", strerror(errno));
		}
		goto main_end;
	}

	// install signal handlers
	log_debug("install base signal handler");
	pcntl_base_signal();

	// init global variables
	// TODO: check malloc return value & mm_global?
	log_debug("init global states");
	cc = sizeof(worker_t) * (worker_num + 1);
	worker_pids = (worker_t *) malloc(cc);
	memset(worker_pids, 0, cc);

	G_INIT(msize);
	G_VAR_ALLOC(user_base, void *);
	xs_user_init();
	main_flag |= FLAG_G_INITED;

	// init the py_dict
	log_debug("load pinyin dict");
	py_dict_load("etc/py.xdb");

	// init the default scws
	log_debug("init task env");
	task_init();

	// init the memory cache
#ifdef HAVE_MEMORY_CACHE
	log_debug("init memory cache");
	if ((mc = mc_create(mm_global)) == NULL) {
		log_error("failed to create memory cache");
		goto main_end;
	}
	mc_set_max_memory(mc, ((msize - 1) << 20));
	mc_set_hash_size(mc, (msize - 1) * 1000);
	mc_set_copy_flag(mc, MC_FLAG_COPY);
	mc_set_dash_type(mc, MC_DASH_CHAIN);
#endif	/* HAVE_MEMORY_CACHE */

	// create tcp server & listen (should before setproctitle)
	if ((listen_sock = conn_server_listen(bind)) < 0) {
		log_error("socket server listen/bind failure");
		goto main_end;
	}
	log_alert("server start (BIND:%s)", bind);

	// spawn workers (first to block all signal)
	sigfillset(&tmpmask);
	sigprocmask(SIG_BLOCK, &tmpmask, &oldmask);

	// special foreground mode
	if (main_flag & FLAG_FOREGROUND) {
		log_alert("works as foregound worker mode");
		become_worker(1, &oldmask);
	}

	// spawn the childs
	for (cc = 1; cc <= worker_num; cc++) {
		spawn_worker(cc, &oldmask);
	}

	// only use tmpmask to caught sigchld on exit
	sigemptyset(&tmpmask);
	sigaddset(&tmpmask, SIGCHLD);

	// master ready
	log_ident("~master");
	setproctitle("master");
	log_notice("ready for system signal (WORKER_NUM:%d)", worker_num);

	// loop to wait signals
	alarm(300); // first alarm timer
	while (1) {
		RESET_FLAG_SIG();
		sigsuspend(&oldmask);

		// check signal flag
		if (CHECK_FLAG_SIG(CHILD)) {
			// respawn dead worker
			for (cc = 1; cc <= worker_num; cc++) {
				if (worker_pids[cc].pid == 0) {
					spawn_worker(cc, &oldmask);
				}
			}
		} else if (CHECK_FLAG_SIG(TSTP)) {
			// log worker info
#if defined(MAX_WORKER_LIFE) && MAX_WORKER_LIFE > 0
			for (cc = 1; cc <= worker_num; cc++) {
				if (worker_pids[cc].pid == 0) {
					msize = 0;
				} else {
					msize = MAX_WORKER_LIFE - (int) (time(NULL) - worker_pids[cc].chrono);
					kill(worker_pids[cc].pid, SIGTSTP);
				}
				log_printf("worker[%d] info (PID:%d, LIFE:%d'%d\")",
						cc, worker_pids[cc].pid, msize / 60, msize % 60);
			}
#endif
		} else if (CHECK_FLAG_SIG(ALARM)) {
			// kill some workers
#if defined(MAX_WORKER_LIFE) && MAX_WORKER_LIFE > 0
			for (cc = 1; cc <= worker_num; cc++) {
				if (worker_pids[cc].pid == 0) {
					continue;
				}
				if (worker_pids[cc].chrono == 0) {
					kill(worker_pids[cc].pid, SIGKILL);
					log_notice("worker[%d] early reach the maximum lifetime, force to kill", cc);
				} else {
					msize = (int) (time(NULL) - worker_pids[cc].chrono);
					if (msize > MAX_WORKER_LIFE) {
						kill(worker_pids[cc].pid, SIGINT);
						worker_pids[cc].chrono = 0;
						log_notice("worker[%d] reach the maximum lifetime, notify to shutdown", cc);
					}
				}
			}
			// set next timer
			alarm(300);
#endif
		} else if (CHECK_FLAG_SIG(EXIT)) {
			// exit by signal
			int sig = (CHECK_FLAG_SIG(EXIT_GRACEFUL)) ? SIGINT : SIGTERM;

			// now should allow child reaper signal (blocked when sigsuspend() return)
			sigprocmask(SIG_UNBLOCK, &tmpmask, NULL);
			log_notice("send exit signal[%d] to all worker", sig);
			for (cc = 1; cc <= worker_num; cc++) {
				if (worker_pids[cc].pid <= 0) {
					continue;
				}
				if (kill(worker_pids[cc].pid, sig) == 0) {
					main_flag |= FLAG_HAS_CHILD;
				} else {
					worker_pids[cc].pid = 0;
					log_error("failed send signal[%d] to worker[%d] (ERROR:%s)",
							sig, cc, strerror(errno));
				}
			}

			// wait for all child processes to exit (30seconds)
			for (msize = 0; msize < 15 && (main_flag & FLAG_HAS_CHILD); msize++) {
				sleep(2);
				main_flag ^= FLAG_HAS_CHILD;
				for (cc = 1; cc <= worker_num; cc++) {
					if (worker_pids[cc].pid > 0) {
						main_flag |= FLAG_HAS_CHILD;
						break;
					}
				}
			}

			// waiting timeout, re-check
			if (main_flag & FLAG_HAS_CHILD) {
				log_warning("timeout to wait, send SIGKILL to lived workers");
				for (cc = 1; cc <= worker_num; cc++) {
					if (worker_pids[cc].pid > 0) {
						kill(worker_pids[cc].pid, SIGKILL);
					}
				}
			}

			// ok real end
			log_alert("server stop");
			break;
		}
	}

	// exit normally or gracefully
	if (!CHECK_FLAG_SIG(EXIT_EXCEPTION)) {
		main_flag |= FLAG_NO_ERROR;
	}

main_end:
	log_debug("main end");
	main_cleanup();
	exit(main_flag & FLAG_NO_ERROR ? 0 : -1);
}
