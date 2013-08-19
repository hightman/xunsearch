/**
 * Thread pool implemention
 * $Id$
 */

#ifdef HAVE_CONFIG_H
#    include "config.h"
#endif

#include <unistd.h>
#include <stdlib.h>
#include <signal.h>
#include <string.h>
#include <stdio.h>
#include "tpool.h"

#if defined(TP_TEST) && !defined(TP_DEBUG)
#    define	TP_DEBUG
#endif
#ifdef TP_DEBUG
#    define debug_printf(fmt, ...)	printf("[%s:%d] " fmt "\n", __FILE__, __LINE__, ##__VA_ARGS__)
#else
#    define	debug_printf(...)		(void)0
#endif

#ifdef DEBUG
#    undef	debug_printf
#    include "log.h"
#    define	debug_printf	log_debug
#endif	/* DEBUG */

/**
 * Macros to access locks quickly
 */
#define	TP_LOCK()		pthread_mutex_lock(&tp->mutex)
#define	TP_UNLOCK()		pthread_mutex_unlock(&tp->mutex)
#define	TP_WAIT()		pthread_cond_wait(&tp->cond, &tp->mutex)
#define	TP_CANCELED()	(tp->status & TPOOL_STATUS_CANCELED)

/**
 * Get task & remove it from the queue (Called under tpool_lock zone)
 * Notice: The task should be free after job finishing
 */
static struct tpool_task *tpool_get_task(tpool_t *tp)
{
	struct tpool_task *task = tp->task_list;
	if (task != NULL) {
		tp->task_list = task->next;
	}
	return task;
}

/**
 * Cleanup function called when the thread was canceld during task execution
 * @param arg struct tpool_thread
 */
static void tpool_thread_cleanup(void *arg)
{
	struct tpool_thread *me = (struct tpool_thread *) arg;
	tpool_t *tp = me->tp;

	debug_printf("thread[%d] is canceled, run cleanup function (TID:%p, TOTAL:%d)",
			me->index, me->tid, tp->cur_total - 1);

	// call cancel handler of task
	if (me->task->cancel_func != NULL) {
		(*me->task->cancel_func)(me->task->arg);
	}

	// free task
	free(me->task);
	me->status = TPOOL_THREAD_NONE;

	TP_LOCK();
	tp->cur_total--;
	if (me->tp->cur_total == 0) {
		pthread_cond_signal(&tp->cond);
	}
	TP_UNLOCK();
}

/**
 * Initlize thread (+sigmask)
 */
static inline void tpool_thread_init(struct tpool_thread *t)
{
	sigset_t set;

	// detach first
	pthread_detach(t->tid);

	// block all signal except for: SIGFPE/SIGILL/SIGBUS/SIGSEGV
	sigfillset(&set);
	sigdelset(&set, SIGFPE);
	sigdelset(&set, SIGILL);
	sigdelset(&set, SIGBUS);
	sigdelset(&set, SIGSEGV);
	pthread_sigmask(SIG_SETMASK, &set, NULL);

	// only can be canceled on executing task
	pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, NULL);
	pthread_setcanceltype(PTHREAD_CANCEL_DEFERRED, NULL);

	// init some struct members
	t->status |= TPOOL_THREAD_BUSY;
	t->calls = 0;
}

/**
 * Thread start point
 */
static void *tpool_thread_start(void *arg)
{
	struct tpool_thread *me = (struct tpool_thread *) arg;
	tpool_t *tp = me->tp;

	// init the thread
	tpool_thread_init(me);

	// loop to execute task
	while (1) {
		// waiting for task
		TP_LOCK();
		tp->cur_spare++;
		me->status ^= TPOOL_THREAD_BUSY;
		while ((me->task = tpool_get_task(tp)) == NULL && !TP_CANCELED()) {
			TP_WAIT();
		}
		me->status |= TPOOL_THREAD_BUSY;
		tp->cur_spare--;
		TP_UNLOCK();

		// empty task (cancled)
		if (me->task == NULL) {
			TP_LOCK();
			me->status = TPOOL_THREAD_NONE;
			tp->cur_total--;
			TP_UNLOCK();

			debug_printf("thread[%d] get empty task(NULL), forced to cancel (TID:%p, CALLS:%d, TOTAL:%d)",
					me->index, me->tid, me->calls, tp->cur_total);

			break;
		}

		// task accepted
		debug_printf("thread[%d] accept new task (TID:%p, FUNC:%p, ARG:%p)",
				me->index, me->tid, me->task->task_func, me->task->arg);

		time(&me->task->begin);
		me->calls++;
		me->status |= TPOOL_THREAD_TASK;

		// call task function with cleanup function & cancelstate
		pthread_cleanup_push(tpool_thread_cleanup, me);
		pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);
		(*me->task->task_func)(me->task->arg);
		pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, NULL);
		pthread_cleanup_pop(0);

		me->status ^= TPOOL_THREAD_TASK;
		free(me->task);
		debug_printf("thread[%d] finished the task (TID:%p, CALLS:%d)", me->index, me->tid, me->calls);

		// check the number of spare threads
		if (tp->cur_spare >= tp->max_spare) {
			TP_LOCK();
			me->status = TPOOL_THREAD_NONE;
			tp->cur_total--;
			TP_UNLOCK();

			debug_printf("thread[%d] suicided due to too many spare threads (TID:%p, SPARE:%d, TOTAL:%d)",
					me->index, me->tid, tp->cur_spare, tp->cur_total);
			break;
		}
	}

	// notify the thread that is waiting for canceling
	if (tp->cur_total == 0) {
		pthread_cond_signal(&tp->cond);
	}

	return NULL;
}

/**
 * Add spare threads (called by main thread)
 * @return added threads number
 */
static int tpool_add_thread(tpool_t *tp, int num)
{
	TP_LOCK();
	if (tp->cur_spare > 0) {
		num = 0;
	} else if (num > (tp->max_total - tp->cur_total)) {
		num = tp->max_total - tp->cur_total;
	}
	TP_UNLOCK();

	// create new threads
	if (num > 0) {
		int i, j = 0;

		// find deactived threads & create them
		for (i = 0; num > 0 && i < tp->max_total; i++) {
			if (tp->threads[i].status & TPOOL_THREAD_ACTIVED) {
				continue;
			}
			if (pthread_create(&tp->threads[i].tid, NULL, tpool_thread_start, &tp->threads[i]) != 0) {
				break;
			}
			tp->threads[i].status |= TPOOL_THREAD_ACTIVED;

			j++;
			num--;
			debug_printf("thread[%d] is created (TID:%p)", i, tp->threads[i].tid);
		}

		// save new number
		TP_LOCK();
		tp->cur_total += j;
		TP_UNLOCK();

		debug_printf("thread pool status (TOTAL:%d, SPARE:%d)", tp->cur_total, tp->cur_spare);
		return j;
	}

	// none of threads created
	return 0;
}

/**
 * Thead pool initlizing
 * @param tp if NULL passed in, thread_pool created on heap
 * @return the thread pool pointer on success or NULL on failure
 */
tpool_t *tpool_init(tpool_t *tp, int max_total, int min_spare, int max_spare)
{
	if (tp != NULL) {
		tp->status = 0;
	} else {
		tp = (tpool_t *) malloc(sizeof(tpool_t));
		if (tp == NULL) {
			return NULL;
		}
		tp->status = TPOOL_STATUS_ONHEAP;
	}

	// set default parameters
	if (max_total <= 0 || max_total > TPOOL_MAX_LIMIT_THREADS) {
		max_total = TPOOL_MAX_LIMIT_THREADS;
	}
	if (max_spare <= 0) {
		max_spare = TPOOL_MAX_SPARE_THREADS;
	}
	if (min_spare <= 0) {
		min_spare = TPOOL_MIN_SPARE_THREADS;
	}
	if (max_spare > max_total) {
		max_spare = max_total;
	}
	if (min_spare > max_spare) {
		min_spare = max_spare;
	}

	// init the value
	tp->status |= TPOOL_STATUS_INITED;
	tp->min_spare = min_spare;
	tp->max_spare = max_spare;
	tp->cur_total = tp->cur_spare = 0;
	tp->max_total = max_total;
	tp->task_list = NULL;

	pthread_mutex_init(&tp->mutex, NULL);
	pthread_cond_init(&tp->cond, NULL);

	memset(tp->threads, 0, sizeof(tp->threads));
	for (max_total = 0; max_total < TPOOL_MAX_LIMIT_THREADS; max_total++) {
		tp->threads[max_total].status = TPOOL_THREAD_NONE;
		tp->threads[max_total].index = max_total;
		tp->threads[max_total].tp = tp;
	}

	// create the first spare threads
	tpool_add_thread(tp, tp->max_spare);

	debug_printf("thread pool initialized (TOTAL:%d, SPARE:%d, MAX:%d)",
			tp->cur_total, tp->cur_spare, tp->max_total);
	return tp;
}

/**
 * Cancel all idle thread & notify working thread quit after finishing job
 */
void tpool_do_cancel(tpool_t *tp, int wait)
{
	if (!tp || !(tp->status & TPOOL_STATUS_INITED)) {
		return;
	}

	debug_printf("send cancel signal to all threads");
	tp->status |= TPOOL_STATUS_CANCELED;
	pthread_cond_broadcast(&tp->cond);

	if (wait == 1) {
		debug_printf("waiting for the end of all threads ...");
		TP_LOCK();
		while (tp->cur_total > 0)
			TP_WAIT();
		TP_UNLOCK();
		debug_printf("ok, all threads exited");
	}
}

/**
 * Destroy thread pool (called by main thread)
 */
void tpool_destroy(tpool_t *tp)
{
	if (!tp) {
		return;
	}
	if (!(tp->status & TPOOL_STATUS_INITED)) {
		goto destroy_end;
	}

	// forced to cancel all actived threads
	if (tp->cur_total > 0) {
		int i = 0;
		struct tpool_thread *t;

		// force to cancel threads that is executing task function
		while (i < tp->max_total) {
			t = &tp->threads[i++];
			if (t->status & TPOOL_THREAD_TASK) {
				// NOTE: If it is me, I should wait other threads, so can not die first.
				// But forced to call cleanup is required.
				if (pthread_equal(t->tid, pthread_self())) {
					tpool_thread_cleanup((void *) t);
					debug_printf("run cleanup for thread[%d] that is myself (TID:%p)", t->index, t->tid);
				} else {
					pthread_cancel(t->tid);
					debug_printf("cancel thread[%d] that is executing task (TID:%p)", t->index, t->tid);
				}
			}
		}

		// cancel & wait
		tpool_do_cancel(tp, 1);
	}
	pthread_mutex_destroy(&tp->mutex);

	// clean inited status
	tp->status ^= TPOOL_STATUS_INITED;

destroy_end:
	if (tp->status & TPOOL_STATUS_ONHEAP) {
		free(tp);
	}
}

/**
 * Submit task to thread pool
 */
void tpool_exec(tpool_t *tp, tpool_func_t func, tpool_func_t cancel, void *arg)
{
	struct tpool_task *task;

	task = (struct tpool_task *) malloc(sizeof(struct tpool_task));
	if (task == NULL) {
		debug_printf("failed to allocate memory for new task (SIZE:%d)", (int) sizeof(struct tpool_task));
		return;
	}

	memset(task, 0, sizeof(struct tpool_task));
	task->task_func = func;
	task->cancel_func = cancel;
	task->arg = arg;

	// save to task list
	TP_LOCK();
	if (tp->task_list == NULL) {
		tp->task_list = task;
	} else {
		struct tpool_task *tail = tp->task_list;

		while (tail->next != NULL) {
			tail = tail->next;
		}
		tail->next = task;
	}
	TP_UNLOCK();
	debug_printf("add new task to thread pool (SPARE:%d, TOTAL:%d)", tp->cur_spare, tp->cur_total);

	// check spare threads
	if (tp->cur_spare == 0) {
		debug_printf("try to add some new threads (NUM:%d)", tp->min_spare);
		tpool_add_thread(tp, tp->min_spare);
	}

	// notify
	pthread_cond_signal(&tp->cond);
}

/**
 * Walk threads to cancel timeoutd
 * @return number of canceled threads
 */
int tpool_cancel_timeout(tpool_t *tp, int sec)
{
	int i, cost, num = 0;
	time_t now;

	time(&now);
	for (i = 0; i < tp->max_total; i++) {
		if (!(tp->threads[i].status & TPOOL_THREAD_TASK)) {
			continue;
		}

		cost = now - tp->threads[i].task->begin;
		debug_printf("thread[%d] current task has been running for [%d] seconds", i, cost);
		if (cost > sec) {
			debug_printf("cancel thread[%d] because it timed out", i);
			pthread_cancel(tp->threads[i].tid);
			num++;
		}
	}
	return num;
}

/**
 * Draw thread pool (free return value is required)
 * Note: allocate enough memory to save data (256bytes per thread)
 */
char *tpool_draw(tpool_t *tp)
{
	struct tpool_thread *t;
	struct tpool_task *tsk;
	char *buf;
	int i, len;
	time_t now;

	if (!tp || !(tp->status & TPOOL_STATUS_INITED)) {
		return NULL;
	}

	len = (tp->max_total + 1) << 8; // * 256
	buf = (char *) malloc(len);
	if (buf == NULL) {
		return NULL;
	}

	// print struct
	len = sprintf(buf, "TPOOL[%p] { status:'%c%c%c', total:%d, spare:%d, max:%d }\n", tp,
			tp->status & TPOOL_STATUS_ONHEAP ? 'H' : '-',
			tp->status & TPOOL_STATUS_INITED ? 'I' : '-',
			tp->status & TPOOL_STATUS_CANCELED ? 'C' : '-',
			tp->cur_total, tp->cur_spare, tp->max_total);

	// print task list
	len += sprintf(buf + len, " = TODO task queue: %s\n", tp->task_list == NULL ? "NULL" : "");
	for (i = 0, tsk = tp->task_list; tsk != NULL; tsk = tsk->next, i++) {
		len += sprintf(buf + len, " = task[%d] {func:%p, cancel:%p, arg:%p}\n",
				i, tsk->task_func, tsk->cancel_func, tsk->arg);
	}

	// print threads
	time(&now);
	for (i = 0; i < tp->max_total; i++) {
		t = &tp->threads[i];
		if (!(t->status & TPOOL_THREAD_ACTIVED)) {
			continue;
		}
		len += sprintf(buf + len, " - thread[%d] {status:'%c%c%c', calls:%d, tid:%p, task:{", i,
				t->status & TPOOL_THREAD_ACTIVED ? 'A' : '-',
				t->status & TPOOL_THREAD_BUSY ? 'B' : '-',
				t->status & TPOOL_THREAD_TASK ? 'T' : '-', t->calls,
				t->status & TPOOL_THREAD_ACTIVED ? (void *) t->tid : NULL);
		if (t->status & TPOOL_THREAD_TASK) {
			len += sprintf(buf + len, "func:%p, cancel:%p, arg:%p, timed:%d",
					t->task->task_func, t->task->cancel_func, t->task->arg, (int) (now - t->task->begin));
		}
		len += sprintf(buf + len, "}}\n");
	}

	return buf;
}

/**
 * Test CODE
 * gcc -DTP_TEST tpool.c -lpthread
 */
#ifdef TP_TEST

static void test_cancel(void *arg)
{
	printf("free(%p)\n", arg);
	free(arg);
}

static void test_task(void *arg)
{
	int i;
	char *p = (char *) arg;

	for (i = 0; i < 10; i++) {
		printf("JOB[%s]: running %02d\n", p, i);
		sleep(1);
	}
	printf("JOB[%s] finished!\n", p);
	printf("free(%p)\n", p);
	if (!strcmp(p, "segv")) {
		p[-1] = '\0';
	}
	if (!strcmp(p, "fpe")) {
		i /= 0;
	}
	if (!strcmp(p, "bus")) {
		char *s = "hello";
		*s = '\0';
	}
	free(p);
}

int main()
{
	tpool_t *t;
	char buf[256], *ptr;
	int len, i = 0;

#    ifdef DEBUG
	log_open("stderr", "tp_test", -1);
#    endif

	t = tpool_init(NULL, 3, 2, 2);

	printf("Thread Pool[%d] initlized OK, try to write your message after >\n", getpid());
	printf("Main thread: %p\n", pthread_self());
	do {
		printf(">");
		fflush(stdout);

		if (!fgets(buf, sizeof(buf) - 1, stdin)) {
			puts("Aborted!\n");
			break;
		}
		if (buf[0] == '\r' || buf[0] == '\n') {
			continue;
		}
		len = strlen(buf);
		buf[len - 1] = '\0';
		if (!strcasecmp(buf, "quit") || !strcasecmp(buf, "exit")) {
			puts("Quit!\n");
			break;
		} else if (!strcasecmp(buf, "cancel")) {
			for (i = 0; i < t->max_total; i++) {
				int cc;

				if (!(t->threads[i].status & TPOOL_THREAD_ACTIVED)) {
					continue;
				}
				pthread_cancel(t->threads[i].tid);
			}
			continue;
		} else if (!strcasecmp(buf, "cancel2")) {
			tpool_cancel(t);
			tpool_destroy(t);
			exit(0);
		} else if (!strcasecmp(buf, "timeout")) {
			tpool_cancel_timeout(t, 5);
			continue;
		} else if (!strcasecmp(buf, "display")) {
			ptr = tpool_draw(t);
			puts(ptr);
			free(ptr);
			continue;
		}
		ptr = strdup(buf);
		printf("strdup(%p)\n", ptr);
		tpool_exec(t, test_task, test_cancel, ptr);
	} while (1);

	//tpool_cancel(t);
	tpool_destroy(t);
	exit(0);
}

#endif	/* TEST */
