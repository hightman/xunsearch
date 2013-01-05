/**
 * Thread pool header file
 * 
 * $Id$
 */

#ifndef __XS_TPOOL_20090531_H__
#define	__XS_TPOOL_20090531_H__

#include <pthread.h>
#include <time.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Limitation settings
 */
#define	TPOOL_MIN_SPARE_THREADS	3		// minimum number of spare threads (added when threads not enough)
#define	TPOOL_MAX_SPARE_THREADS	6		// maximum number of spare threads (created at start)
#define	TPOOL_MAX_LIMIT_THREADS	100		// total threads number limit < 32768 [hard limit]
#define	TPOOL_TASK_ARG(t)		((struct tpool_thread *) t)->task->arg
	
/**
 * Thread job prototype
 */
typedef struct tpool_thread tpool_thr;
typedef void (*tpool_func_t)(void *);

/**
 * Task queue (keep pending-jobs to process)
 */
struct tpool_task
{
	tpool_func_t task_func; // start point of job
	tpool_func_t cancel_func; // called if the thread cancled before finishing job
	void *arg; // argument for exec/cancel func
	time_t begin; // task begin time
	struct tpool_task *next;
};

/**
 * Thread bits setting
 */
#define	TPOOL_THREAD_NONE			0
#define	TPOOL_THREAD_ACTIVED		0x01	// ready for accepting new task
#define	TPOOL_THREAD_BUSY			0x02	// running/sleeping
#define	TPOOL_THREAD_TASK			0x04	// with task

#define	TPOOL_STATUS_ONHEAP			0x01
#define	TPOOL_STATUS_INITED			0x02
#define	TPOOL_STATUS_CANCELED		0x04

/**
 * Data structure of thread pool
 */
typedef struct thread_pool tpool_t;

struct tpool_thread
{
	pthread_t tid; // thread id
	short status; // TPOOL_THREAD_xxx
	short index; // index
	int calls; // number of call times
	struct tpool_task *task; // running task
	tpool_t *tp; // pointer to self
};

struct thread_pool
{
	short min_spare; // min spare thread in the pool, for ceate thread when not-enough
	short max_spare; // max spare thread in the pool, for auto kill thread & init thread pool
	short cur_total; // current thread num
	short cur_spare; // current spare thread num
	short max_total; // soft limit for threads num
	short status; // status of thread pool
	
	pthread_mutex_t mutex; // global thread_pool mutex
	pthread_cond_t cond; // global thread_cond signal

	struct tpool_thread threads[TPOOL_MAX_LIMIT_THREADS];
	struct tpool_task *task_list; // task list
};

/* Initlize thread pool */
tpool_t *tpool_init(tpool_t *tp, int max_total, int min_spare, int max_spare);

/* Cancel all spare threads & notify working threads to quit */
void tpool_do_cancel(tpool_t *tp, int wait);

/* Destroy the thread pool & cleanup all related resources */
void tpool_destroy(tpool_t *tp);

/* Execute a new job */
void tpool_exec(tpool_t *tp, tpool_func_t func, tpool_func_t cancel, void *arg);

/* Walk threads to cancel timeoutd, return the number of threads were canceld */
int tpool_cancel_timeout(tpool_t *tp, int sec);

/* Draw thread pool (free return value is required) */
char *tpool_draw(tpool_t *tp);

/* Macros to cancel tpool */
#define	tpool_cancel(tp)		tpool_do_cancel(tp, 1)
#define	tpool_cancel_nowait(tp)	tpool_do_cancel(tp, 0)

// 注意:
// 1.子线程屏蔽除以下列表外的所有信号(信号处理函数只能共享主线程的)
//   SIGFPE, SIGILL, SIGBUS, SIGSEGV [, SIGKILL, SIGSTOP]
//   产生这些信号时可能会导致整个进程退出.
// 2.子线程工作超时被取消时, 应通过 pthread_cleanup_push/pop 登记清理函数
//   以免造成死锁或资源长期不能释放. 是否考虑在主线程记录异常线程的数量?
//

#ifdef __cplusplus
}
#endif

#endif	/* __XS_TPOOL_20090531_H__ */
