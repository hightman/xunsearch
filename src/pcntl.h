/**
 * Header file for controling process
 *
 * $Id: $
 */

#ifndef __XS_PCNTL_20110516_H__
#define	__XS_PCNTL_20110516_H__

#ifdef __cplusplus
extern "C" {
#endif

/* Used as return value of signal_term() to prevent calling exit() at once */
#define	SIGNAL_TERM_LATER	0x80

/* Check the service is running or not, and record the current pid. */
int pcntl_running(const char *bind, int save);

/* Server process control */
void pcntl_kill(const char *bind, char *op, char *name);

/* Become the current process as a daemon server */
void pcntl_daemon();

/* Install base signal handlers */
void pcntl_base_signal();

/* Register a signal handler */
void pcntl_register_signal(int sig, void (*func)(int));

/* Export and save the server usage info */
void pcntl_servo_usage(const char *fpath);

#ifndef HAVE_SETPROCTITLE
/* Save the arguments of main */
void save_main_args(int argc, char **argv);

/* Set the process title */
void setproctitle(const char *fmt, ...);
#endif

#ifdef __cplusplus
}
#endif

#endif	/* __XS_PCNTL_20110516_H__ */
