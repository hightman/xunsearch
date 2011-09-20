/**
 * 线程安全的文件锁设计 (Thread-Safe file locking)
 *
 * 由于某些平台 phtread_rwlock_ 系列是不支持进程间共享的属性,
 * 而 fcntl() 的文件记录锁又不能安全用于多线程. 所以在多进程和
 * 多线程混合混合的服务器模型中, 互斥锁一般只能用信号灯或信号量
 * 机制, 但这都只有一种状态而没有共享锁(读)和独占锁(写)的区分,
 * 对于多读少写的情况不太舒服.
 *
 * 下面的代码是的实测平台为: FreeBSD-6.2,
 * (正是freebsd不支持pthread_rwlockattr_setpshared(attr, PTHREAD_PROCESS_SHARED ..)
 *
 * flock_*** 系列的说明, 它可以类似 pthread_rwlock_t 那样的使用, 但因为某些平台不支持
 * PTHREAD_PROCESS_SHARED 的属性, 所以才特意写了我这段代码通过 fcntl 记录锁来协调.
 *
 * 1) 数据类型: flock_t
 *
 * 2) 函数接口: (对于 int 型的函数成功统一返回 1, 失败返回 0)
 *    int flock_init(flock_t *fl, const char *fpath);
 *    初始化 fl 指向的 flock_t 数据结构, fpath 指定要锁定的文件路径, 传入 NULL 则由系统自动创建临时文件
 * 
 *    int flock_set_thread_safe(fockt_t *fl);
 *    将 fl 设置为线程安全 (内部初始化类型为 pthread_rwlock_t 的 fl->plock)
 *
 *    int flock_wrlock(flock_t *fl);
 *    取独占锁, 一般用于写
 *
 *    int flock_rdlock(flock_t *fl);
 *    取共享锁
 * 
 *    int flock_unlock(flock_t *fl);
 *    解锁
 *
 *    void flock_destroy(flock_t *fl);
 *    释放解毁锁
 *
 * 3) 根据文件描述进行文件加锁/解锁的通用宏(这些宏不具备线程安全):
 *    FLOCK_WR_NB(fd);       // 对 fd取独占锁(无阻塞,成功返回1,失败返回0)
 *    FLOCK_RD_NB(fd);       // 对 fd取共享锁(无阻塞,成功返回1,失败返回0)
 *    FLOCK_WR(fd);          // 对 fd取独占锁(若已被占则自动阻塞等待)
 *    FLOCK_RD(fd);          // 对 fd取共享锁(若已被独占则自动等待)
 *    FLOCK_UN(fd);          // 对 fd释放锁
 * 
 * $Id: flock.h,v 1.1.1.1 2009/07/24 08:15:50 hightman Exp $
 */

#ifndef __XS_FLOCK_20110520_H__
#define	__XS_FLOCK_20110520_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <unistd.h>
#define __USE_UNIX98	1

#include <fcntl.h>
#include <sys/types.h>
#include <pthread.h>

/* The structure for locking */
typedef struct
{
	int fd;
	int flag;
	pthread_rwlock_t plock;
} flock_t;

/* Initializing file lock structure */
int flock_init(flock_t *fl, const char *fpath); // 1->ok, 0->failed

/* Enable thread-safe */
int flock_set_thread_safe(flock_t *fl); // set to thread safe, 1->ok, 0->failed

/* Exclusive lock (read & write protected) */
int flock_wrlock(flock_t *fl); // 1->ok, 0->failed

/* Shared lock (read protected) */
int flock_rdlock(flock_t *fl); // 1->ok, 0->failed

/* Releasing lock */
int flock_unlock(flock_t *fl);

/* Destroy all of the tempory lock resources */
void flock_destroy(flock_t *fl); // destroy flock

/* Lock or unlock operation */
int flock_exec(int fd, int type, off_t offset, int whence, off_t len, int nonblock); // error: 0, succ: 1

#define	FLOCK_WR_NB(fd)	flock_exec(fd, F_WRLCK, 0, SEEK_SET, 0, 1)
#define	FLOCK_RD_NB(fd)	flock_exec(fd, F_RDLCK, 0, SEEK_SET, 0, 1)
#define	FLOCK_WR(fd)	flock_exec(fd, F_WRLCK, 0, SEEK_SET, 0, 0)
#define	FLOCK_RD(fd)	flock_exec(fd, F_RDLCK, 0, SEEK_SET, 0, 0)
#define	FLOCK_UN(fd)	flock_exec(fd, F_UNLCK, 0, SEEK_SET, 0, 0)

#ifdef __cplusplus
}
#endif

#endif	/* __XS_FLOCK_20110520_H__ */
