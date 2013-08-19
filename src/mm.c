/**
 * Libmm replacement used by cache design of FTPHP-searchd
 * Some source codes cut from eAccelerator/PHP
 * 
 * $Id$
 */

#ifdef HAVE_CONFIG_H
#    include "config.h"
#endif

#include <sys/types.h>
#include <sys/stat.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/sem.h>
#include <sys/mman.h>
#include <errno.h>

typedef struct mm_mutex
{
	int semid;
} mm_mutex;

typedef struct mm_free_bucket
{
	size_t size;
	struct mm_free_bucket *next;
} mm_free_bucket;

typedef struct mm_core
{
	size_t size;
	void *start;
	size_t available;
	mm_mutex *lock;
	mm_free_bucket *free_list;
} mm_core;

typedef union mm_mem_head
{
	size_t size;
	double a1;
	int (*a2)(int);
	void *a3;
} mm_mem_head;

#define MM_SIZE(sz)		(sizeof(mm_mem_head)+(sz))
#define PTR_TO_HEAD(p)	(((mm_mem_head *)(p)) - 1)
#define HEAD_TO_PTR(p)	((void *)(((mm_mem_head *)(p)) + 1))
#define MM mm_core
#define	MM_WORD	mm_mem_head

#if (defined (__GNUC__) && __GNUC__ >= 2)
#    define MM_PLATFORM_ALIGNMENT (__alignof__ (MM_WORD))
#else
#    define MM_PLATFORM_ALIGNMENT (sizeof(MM_WORD))
#endif
#define MM_ALIGN(n) (void*)((((size_t)(n)-1) & ~(MM_PLATFORM_ALIGNMENT-1)) + MM_PLATFORM_ALIGNMENT)

#ifndef HAVE_UNION_SEMUN 

union semun
{
	int val;
	struct semid_ds *buf;
	unsigned short *array;
	struct seminfo *__buf;
};
#endif

#include "mm.h"

/**
 * MM-lock implement
 * Semaphore
 */
static int mm_init_lock(mm_mutex *lock)
{
	union semun arg;
	int n = MM_SEM_NUM;

	if ((lock->semid = semget(IPC_PRIVATE, n, IPC_CREAT | IPC_EXCL | S_IRUSR | S_IWUSR)) < 0) {
		return 0;
	}

	arg.val = 1;
	while (n--) {
		if (semctl(lock->semid, n, SETVAL, arg) < 0) {
			semctl(lock->semid, n, IPC_RMID, 0);
		}
	}
	return 1;
}

static int mm_do_lock(mm_mutex *lock, int num, int val)
{
	struct sembuf op;
	int rc;

	op.sem_num = (unsigned short) num;
	op.sem_op = (short) val;
	op.sem_flg = SEM_UNDO;

	do {
		rc = semop(lock->semid, &op, 1);
	} while (rc < 0 && errno == EINTR);

	return(rc == 0);
}

static void mm_destroy_lock(mm_mutex *lock)
{
	int n = MM_SEM_NUM;
	while (n--) {
		semctl(lock->semid, n, IPC_RMID, 0);
	}
}

int _mm_lock(MM *mm, int num)
{
	return mm_do_lock(mm->lock, num, -1);
}

int _mm_unlock(MM *mm, int num)
{
	return mm_do_lock(mm->lock, num, 1);
}

/**
 * Shared memory implement
 */
static MM *mm_create_shm(size_t size)
{
	MM *p;

	p = (MM *) mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANON, -1, 0);
	if (p != (MM *) MAP_FAILED) {
		p->size = size;
		p->start = (char *) p + sizeof(MM);
	}
	return p;
}

static void mm_destroy_shm(MM *mm)
{
	if (mm != NULL && mm != (MM *) MAP_FAILED) {
		munmap(mm, mm->size);
	}
}

static void mm_init(MM *mm)
{
	mm->start = MM_ALIGN(mm->start);
	mm->lock = mm->start;
	mm->start = MM_ALIGN((void *) (((char *) (mm->start)) + sizeof(mm_mutex)));
	mm->available = mm->size - (((char *) (mm->start))-(char *) mm);
	mm->free_list = (mm_free_bucket *) mm->start;
	mm->free_list->size = mm->available;
	mm->free_list->next = NULL;
}

void *mm_malloc_nolock(MM *mm, size_t size)
{
	if (size > 0) {
		mm_mem_head *x = NULL;
		size_t realsize = (size_t) MM_ALIGN(MM_SIZE(size));

		if (realsize <= mm->available) {
			/* Search for free bucket */
			mm_free_bucket *p = mm->free_list;
			mm_free_bucket *q = NULL;
			mm_free_bucket *best = NULL;
			mm_free_bucket *best_prev = NULL;
			while (p != NULL) {
				if (p->size == realsize) {
					/* Found free bucket with the same size */
					if (q == NULL) {
						mm->free_list = p->next;
						x = (mm_mem_head *) p;
					} else {
						q->next = p->next;
						x = (mm_mem_head *) p;
					}
					break;
				} else if (p->size > realsize && (best == NULL || best->size > p->size)) {
					/* Found best bucket (smallest bucket with the grater size) */
					best = p;
					best_prev = q;
				}
				q = p;
				p = p->next;
			}
			if (x == NULL && best != NULL) {
				if (best->size - realsize < sizeof(mm_free_bucket)) {
					realsize = best->size;
					x = (mm_mem_head *) best;
					if (best_prev == NULL) {
						mm->free_list = best->next;
					} else {
						best_prev->next = best->next;
					}
				} else {
					if (best_prev == NULL) {
						mm->free_list = (mm_free_bucket *) ((char *) best + realsize);
						mm->free_list->next = best->next;
						mm->free_list->size = best->size - realsize;
					} else {
						best_prev->next = (mm_free_bucket *) ((char *) best + realsize);
						best_prev->next->next = best->next;
						best_prev->next->size = best->size - realsize;
					}
					best->size = realsize;
					x = (mm_mem_head *) best;
				}
			}
			if (x != NULL) {
				mm->available -= realsize;
			}
		}
		if (x != NULL) {
			return HEAD_TO_PTR(x);
		}
	}
	return NULL;
}

void mm_free_nolock(MM *mm, void *x)
{
	if (x != NULL) {
		if (x >= mm->start && x < (void *) ((char *) mm + mm->size)) {
			mm_mem_head *p = PTR_TO_HEAD(x);
			size_t size = p->size;

			if ((char *) p + size <= (char *) mm + mm->size) {
				mm_free_bucket *b = (mm_free_bucket *) p;
				b->next = NULL;
				if (mm->free_list == NULL) {
					mm->free_list = b;
				} else {
					mm_free_bucket *q = mm->free_list;
					mm_free_bucket *prev = NULL;
					mm_free_bucket *next = NULL;
					while (q != NULL) {
						if (b < q) {
							next = q;
							break;
						}
						prev = q;
						q = q->next;
					}
					if (prev != NULL && (char *) prev + prev->size == (char *) b) {
						if ((char *) next == (char *) b + size) {
							/* merging with prev and next */
							prev->size += size + next->size;
							prev->next = next->next;
						} else {
							/* merging with prev */
							prev->size += size;
						}
					} else {
						if ((char *) next == (char *) b + size) {
							/* merging with next */
							b->size += next->size;
							b->next = next->next;
						} else {
							/* don't merge */
							b->next = next;
						}
						if (prev != NULL) {
							prev->next = b;
						} else {
							mm->free_list = b;
						}
					}
				}
				mm->available += size;
			}
		}
	}
}

size_t mm_maxsize(MM *mm)
{
	size_t ret = MM_SIZE(0);
	mm_free_bucket *p;

	if (!mm_lock(mm))
		return 0;

	p = mm->free_list;
	while (p != NULL) {
		if (p->size > ret) {
			ret = p->size;
		}
		p = p->next;
	}
	mm_unlock(mm);
	return ret - MM_SIZE(0);
}

void *mm_malloc(MM *mm, size_t size)
{
	void *ret;

	if (!mm_lock(mm)) {
		return NULL;
	}
	ret = mm_malloc_nolock(mm, size);
	mm_unlock(mm);
	return ret;
}

void mm_free(MM *mm, void *x)
{
	mm_lock(mm);
	mm_free_nolock(mm, x);
	mm_unlock(mm);
}

MM *mm_create(size_t size)
{
	MM *p;

	if (size == 0)
		size = 1 << 25;
	p = mm_create_shm(size);
	if (p == (MM *) MAP_FAILED) {
		return NULL;
	}

	mm_init(p);
	if (!mm_init_lock(p->lock)) {
		mm_destroy_shm(p);
		return NULL;
	}
	return p;
}

void mm_destroy(MM *mm)
{
	if (mm != NULL) {
		mm_destroy_lock(mm->lock);
		mm_destroy_shm(mm);
	}
}

size_t mm_size(MM *mm)
{
	if (mm != NULL) {
		return mm->size;
	}
	return 0;
}

size_t mm_sizeof(MM *mm, void *x)
{
	mm_mem_head *p;
	size_t ret;

	if (mm == NULL || x == NULL || !mm_lock(mm)) {
		return 0;
	}
	p = PTR_TO_HEAD(x);
	ret = p->size;
	mm_unlock(mm);
	return ret;
}

size_t mm_available(MM *mm)
{
	size_t available;

	if (mm != NULL && mm_lock(mm)) {
		available = mm->available;
		mm_unlock(mm);
		return available;
	}
	return 0;
}

int mm_protect(MM *mm, int mode)
{
	int pmode = 0;

	if (mode & MM_PROT_NONE) {
		pmode |= PROT_NONE;
	}
	if (mode & MM_PROT_READ) {
		pmode |= PROT_READ;
	}
	if (mode & MM_PROT_WRITE) {
		pmode |= PROT_WRITE;
	}
	if (mode & MM_PROT_EXEC) {
		pmode |= PROT_EXEC;
	}

	return(mprotect(mm, mm->size, pmode) == 0);
}
