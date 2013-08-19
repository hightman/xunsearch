/**
 * Process control
 * 1. read/save running pid
 * 2. server process control by signal [-k option]
 * 3. become daemon
 * 4. install base signal handler
 * 5. output server usage
 * 6. setproctitle
 * $Id$
 */

#ifdef HAVE_CONFIG_H
#    include "config.h"
#endif

#include <sys/types.h>
#include <unistd.h>
#include <signal.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <fcntl.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <sys/wait.h>
#include <signal.h>
#include <errno.h>

#include "pcntl.h"
#include "global.h"

/**
 * Check the service is running or not, and record the current pid.
 * 
 * @param bind path/address:port to bind for the server
 * @param save whether to save pid of current process
 * @return >0:running pid, -1:failed to open pid file, 0:not running[save current pid]
 */
int pcntl_running(const char *bind, int save)
{
	int fd, pid = 0;
	char *ptr, pid_file[128];

	// get pid file
	ptr = (char *) bind;
	if (!strncmp(DEFAULT_TEMP_DIR, ptr, sizeof(DEFAULT_TEMP_DIR) - 1)) {
		ptr += sizeof(DEFAULT_TEMP_DIR) - 1;
	}
	strcpy(pid_file, DEFAULT_TEMP_DIR "pid.");
	fd = strlen(pid_file);
	while (*ptr && (fd < sizeof(pid_file) - 1)) {
		if (*ptr == '.' || *ptr == ':' || *ptr == '/') {
			pid_file[fd] = '_';
		} else {
			pid_file[fd] = *ptr;
		}
		ptr++;
		fd++;
	}
	pid_file[fd] = '\0';

	// get exists pid
	fd = save ? open(pid_file, O_RDWR | O_CREAT, 0600) : open(pid_file, O_RDONLY);
	if (fd < 0) {
		return -1;
	}

	// read exists pid
	memset(pid_file, 0, sizeof(pid_file));
	if (read(fd, pid_file, sizeof(pid_file) - 1) > 0) {
		pid = atoi(pid_file);
		if (kill(pid, 0) != 0) {
			pid = 0;
		}
	}

	// save new pid
	if (save && pid == 0) {
		ftruncate(fd, 0);
		lseek(fd, 0, SEEK_SET);
		sprintf(pid_file, "%d", getpid());
		write(fd, pid_file, strlen(pid_file) + 1);
	}
	close(fd);

	return pid;
}

#define	PCNTL_FLAG_FAST		0x01
#define	PCNTL_FLAG_ERROR	0x02
#define	PCNTL_FLAG_CONTINUE	0x04

/**
 * Server process control
 * @param bind path/address:port to bind for the server
 * @param op operator signal sent to the process, its value maybe: [fast]<start|stop|restart|reload>
 * @param name program name
 */
void pcntl_kill(const char *bind, char *op, char *name)
{
	int flag = 0;
	int pid = pcntl_running(bind, 0);

	// get real operator
	if (!strncasecmp(op, "fast", 4)) {
		flag |= PCNTL_FLAG_FAST;
		op += 4;
	}

	// start
	if (!strcasecmp(op, "start")) {
		if (pid > 0) {
			printf("WARNING: server[%s] is running (BIND:%s, PID:%d)\n", name, bind, pid);
		} else {
			printf("INFO: starting server[%s] ... (BIND:%s)\n", name, bind);
			flag |= PCNTL_FLAG_CONTINUE;
		}
	} else if (!strcasecmp(op, "restart") || !strcasecmp(op, "stop")) {
		if (pid <= 0) {
			printf("WARNING: no server[%s] is running (BIND:%s)\n", name, bind);
		} else if (kill(pid, (flag & PCNTL_FLAG_FAST) ? SIGTERM : SIGINT) != 0) {
			printf("ERROR: failed to send termination signal to the running server[%s] (PID:%d, ERROR:%s)\n",
					name, pid, strerror(errno));
			flag |= PCNTL_FLAG_ERROR;
		} else {
			int sec = 30;

			// wait the pid close in 30 seconds
			flag |= PCNTL_FLAG_ERROR;
			printf("INFO: stopping server[%s] (BIND:%s) ...", name, bind);
			while (sec--) {
				printf(".");
				fflush(stdout);
				sleep(1);
				if (kill(pid, 0) != 0) {
					flag ^= PCNTL_FLAG_ERROR;
					break;
				}
			}
			printf(" [%s]\n", (flag & PCNTL_FLAG_ERROR) ? "FAILED" : "OK");
		}
		if (!strcasecmp(op, "restart") && !(flag & PCNTL_FLAG_ERROR)) {
			printf("INFO: re-starting server[%s] ... (BIND:%s)\n", name, bind);
			flag |= PCNTL_FLAG_CONTINUE;
		}
	} else if (!strcasecmp(op, "reload")) {
		if (pid <= 0) {
			printf("WARNING: no server[%s] is running (BIND:%s)\n", name, bind);
		} else if (kill(pid, SIGHUP) == 0) {
			printf("INFO: reload signal has been sent to the running server[%s] (BIND:%s)\n", name, bind);
		} else {
			printf("ERROR: failed to send reload signal to the running server[%s] (PID:%d, ERROR:%s)\n",
					name, pid, strerror(errno));
			flag |= PCNTL_FLAG_ERROR;
		}
	} else {
		printf("ERROR: unknown operator: %s\n", op);
		flag |= PCNTL_FLAG_ERROR;
	}

	// result, exit when status is not: 0x100
	if (!(flag & PCNTL_FLAG_CONTINUE)) {
		exit(flag & PCNTL_FLAG_ERROR ? -1 : 0);
	}
}

/**
 * Become the current process as a daemon server
 */
void pcntl_daemon()
{
	close(2); // STDERR_FILENO
	close(1); // STDOUT_FILENO
	close(0); // STDIN_FILENO
	if (fork()) {
		exit(0);
	}
	setsid();
	if (fork()) {
		exit(0);
	}
}

/*
 * Scheme of signal handler:
 * 0. SIGTSTP = Ctrl-Z, SIGINT = Ctrl-C, SIGQUIT = Ctrl-\
 * 1. Wait the stopeed children process(restart a new worker for searchd): SIGCHLD
 *    <callback: sig_child>
 * 2. Reload config data: SIGTSTP, SIGHUP, SIGALRM
 *    <callback: sig_reload>
 * 3. Gracefully stop the server(quit after finishing all accepted request): SIGINT
 *    <callback: sig_int>
 * 4. Terminate immediately(Notify the child processes to quit, or notify its parent process):
 *    1) Normal terminated: SIGTERM, SIGQUIT
 *    <callback: sig_term>
 *    2) Except terminated: SIGXCPU, SIGBUS, SIGSEGV, SIGTRAP, SIGABRT, SIGFPE, SIGILL
 *    3) Should *NOT* block SIGFPE/SIGILL/SIGSEGV/SIGBUS in any threads.
 * 5. BLOCK all other signals
 */
extern int signal_term(int sig); // return exit_status, but 0x80 => not quit now
extern void signal_int();
extern void signal_child(pid_t pid, int status);
extern void signal_reload(int sig);
static struct sigaction act;

#define	_sig_int	signal_int
#define	_sig_reload	signal_reload

static void _sig_term(int sig)
{
	int rc = signal_term(sig);

#ifdef DEBUG
	// NOTE: generate core file in DEBUG mode
	if ((sig == SIGFPE || sig == SIGILL || sig == SIGBUS || sig == SIGSEGV || sig == SIGABRT)
			&& signal(sig, SIG_DFL) != SIG_ERR) {
		raise(sig);
		return;
	}
#endif

	// exit or not
	if (rc != SIGNAL_TERM_LATER) {
		_exit(rc);
	}
}

#ifndef WIFCONTINUED
#    define	WIFCONTINUED(x)		0
#endif

static void _sig_child()
{
	pid_t pid;
	int status, errno_save = errno;

	while ((pid = waitpid(-1, &status, WNOHANG | WUNTRACED)) > 0) {
		// just ignore stop/continue signal
		if (WIFSTOPPED(status) || WIFCONTINUED(status)) {
			continue;
		}
		// called only when child process exit() normal, -2 : signal
		status = WIFEXITED(status) ? (char) WEXITSTATUS(status) : -2;
		signal_child(pid, status);
	}
	errno = errno_save;
}

/**
 * Install base signal handlers
 */
void pcntl_base_signal()
{
	sigfillset(&act.sa_mask);
	sigprocmask(SIG_BLOCK, &act.sa_mask, NULL);

	act.sa_flags = 0;
	act.sa_handler = _sig_child;
	sigemptyset(&act.sa_mask);
	sigaction(SIGCHLD, &act, NULL);

	act.sa_handler = _sig_reload;
	sigemptyset(&act.sa_mask);
	sigaddset(&act.sa_mask, SIGHUP);
	sigaddset(&act.sa_mask, SIGTSTP);
	sigaddset(&act.sa_mask, SIGALRM);
	sigaction(SIGHUP, &act, NULL);
	sigaction(SIGTSTP, &act, NULL);
	sigaction(SIGALRM, &act, NULL);

	act.sa_handler = _sig_int;
	sigemptyset(&act.sa_mask);
	sigaction(SIGINT, &act, NULL);

	act.sa_handler = _sig_term;
	sigemptyset(&act.sa_mask);
	sigaddset(&act.sa_mask, SIGQUIT);
	sigaddset(&act.sa_mask, SIGTERM);
	sigaddset(&act.sa_mask, SIGABRT);
	sigaddset(&act.sa_mask, SIGXCPU);
	sigaddset(&act.sa_mask, SIGBUS);
	sigaddset(&act.sa_mask, SIGFPE);
	sigaddset(&act.sa_mask, SIGILL);
	sigaddset(&act.sa_mask, SIGSEGV);
	sigaddset(&act.sa_mask, SIGTRAP);
	sigaction(SIGQUIT, &act, NULL);
	sigaction(SIGTERM, &act, NULL);
	sigaction(SIGABRT, &act, NULL);
	sigaction(SIGXCPU, &act, NULL);
	sigaction(SIGBUS, &act, NULL);
	sigaction(SIGFPE, &act, NULL);
	sigaction(SIGILL, &act, NULL);
	sigaction(SIGSEGV, &act, NULL);
	sigaction(SIGTRAP, &act, NULL);

	// unblock these signals
	sigaddset(&act.sa_mask, SIGCHLD);
	sigaddset(&act.sa_mask, SIGHUP);
	sigaddset(&act.sa_mask, SIGTSTP);
	sigaddset(&act.sa_mask, SIGALRM);
	sigaddset(&act.sa_mask, SIGINT);
	sigprocmask(SIG_UNBLOCK, &act.sa_mask, NULL);
}

/**
 * Register signal handler
 * @param fpath
 */
void pcntl_register_signal(int sig, void (*func)(int))
{
	act.sa_flags = 0;
	act.sa_handler = func;
	sigemptyset(&act.sa_mask);
	sigaction(sig, &act, NULL);
	sigprocmask(SIG_UNBLOCK, &act.sa_mask, NULL);
}

/**
 * Export and save the server usage info.
 * 
 * @param fpath file path used to store data
 */
void pcntl_server_usage(const char *fpath)
{
	struct rusage ru;
	FILE *fp;

	if ((fp = fopen(fpath, "a")) == NULL) {
		return;
	}

	if (!getrusage(RUSAGE_CHILDREN, &ru)) {
		fprintf(fp, "\n[Server Usage] pid = %d \n\n"
				"user time: %.6f\n"
				"system time: %.6f\n"
				"maximum resident set size: %lu P\n"
				"integral resident set size: %lu\n"
				"page faults not requiring physical I/O: %ld\n"
				"page faults requiring physical I/O: %ld\n"
				"swaps: %ld\n"
				"block input operations: %ld\n"
				"block output operations: %ld\n"
				"messages sent: %ld\n"
				"messages received: %ld\n"
				"signals received: %ld\n"
				"voluntary context switches: %ld\n"
				"involuntary context switches: %ld\n\n",
				getpid(),
				(double) ru.ru_utime.tv_sec + (double) ru.ru_utime.tv_usec / 1000000.0,
				(double) ru.ru_stime.tv_sec + (double) ru.ru_stime.tv_usec / 1000000.0,
				ru.ru_maxrss, ru.ru_idrss, ru.ru_minflt, ru.ru_majflt, ru.ru_nswap,
				ru.ru_inblock, ru.ru_oublock, ru.ru_msgsnd, ru.ru_msgrcv,
				ru.ru_nsignals, ru.ru_nvcsw, ru.ru_nivcsw);
	}
	fclose(fp);
}

#ifndef HAVE_SETPROCTITLE
#    include <stdarg.h>

static int procttl_addlen, procttl_maxlen, orig_argc;
static char procttl_buf[256], *procttl_add, **orig_argv;

/**
 * Record the orginal argc, argv of main()
 * @param argc
 * @param argv
 */
void save_main_args(int argc, char **argv)
{
	orig_argc = argc;
	orig_argv = argv;
}

/**
 * Set the process title by updating argv[0] of main function
 * NOTE: please call save_main_args() in main() first!
 * 
 * @param fmt printf-style format string
 */
void setproctitle(const char *fmt, ...)
{
	int len;

	if (procttl_maxlen == 0 && orig_argc > 0) {
		char *eoa;

		eoa = orig_argv[orig_argc - 1] + strlen(orig_argv[orig_argc - 1]);
		procttl_maxlen = eoa - orig_argv[0];
		if (procttl_maxlen > sizeof(procttl_buf)) {
			procttl_maxlen = sizeof(procttl_buf);
		}
		if ((eoa = strrchr(orig_argv[0], '/')) == NULL) {
			eoa = orig_argv[0];
		} else {
			eoa++;
		}
		len = strlen(eoa);
		if (len > 32) {
			len = 32;
		}

		memcpy(procttl_buf, eoa, len);
		procttl_buf[len++] = ':';
		procttl_buf[len++] = ' ';
		procttl_add = procttl_buf + len;
		procttl_addlen = procttl_maxlen - len - 1;
	}

	if (procttl_addlen > 0) {
		va_list ap;

		memset(procttl_add, 0, procttl_addlen + 1);
		va_start(ap, fmt);
		vsnprintf(procttl_add, procttl_addlen, fmt, ap);
		va_end(ap);
		memcpy(orig_argv[0], procttl_buf, procttl_maxlen);
	}
}

#endif	/* HAVE_SETPROCTITLE */
