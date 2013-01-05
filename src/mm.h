/**
 Libmm replacement used by cache design of xunsearch
 Some source codes cut from eAccelerator/PHP

共享内存管理, 改自 eAccelerator 中的 mm.c, 采用 sem 信号量加锁, 线程安全!
使用时包含 mm.h 这个头文件即可, 数据类型 MM 就是这块共享内存的操作句柄类型.

常用 API 介绍:
1. MM *mm_create(size_t size);
   创建 size (单位bytes) 大小的共享内存空间(此为匿名mmap, 用于父子关系之间的进程)
   成功返回 MM 指针, 失败返回 NULL

2. void mm_destroy(MM *mm);
   销毁 mm, 它同时销毁所有的信号量锁, 多进程模型中只允许一次调用, 子进程退出时
   不必调用该函数, 以免破解整个全局的 mm 结构.

3. mm_lock(MM *mm); mm_unlock(MM *mm);
   对整个 mm 进行加锁或解锁.
   注意: 在 mm_malloc 和 mm_free 内部隐蔽地调用了 mm_lock/mm_unlock, 所以务必
		 不能已加锁代码段里使用 mm_malloc/mm_free, 否则会造成死锁, 应当改用
		 mm_malloc_nolock/mm_free_nolock

4. mm_lock1(MM *mm); mm_unlock1(MM *mm); ... mm_lock4(MM *mm); mm_unlock4(MM *mm);
   ... 这 4 组加锁/解锁之间互不影响, 可用于各类区间操作需加锁时使用.

5. void *mm_malloc(MM *mm, size_t size);
   void mm_free(MM *mm, void *p);

   申请和释放内存, 带全局锁

6. void *mm_malloc_nolock(MM *mm, size_t size);
   void  mm_malloc_free(MM *mm, void *p);
   同上, 但不上锁


罕用的 API:
1. size_t mm_size(MM *mm);
   获取 mm 在创建时的 size.

2. int mm_protect(MM *mm, int mode); 
   保护 mm , 内部调用 mprotect()
   mode 值为 MM_PROT_NONE, MM_PROT_READ, MM_PROT_WRITE, MM_PROT_EXEC 的组合

3. size_t mm_maxsize(MM *mm); size_t mm_avaiable(MM *mm);
   分别返回当前能申请到的最大内存长度和当前可用内存空间余额

4. size_t mm_sizeof(MM *mm, void *p);
   如果 p 为 mm_malloc 申请的内存, 则该调用可以返回申请时的长度

 $Id$
 */


#ifndef __XS_MM_20090527_H__
#define	__XS_MM_20090527_H__

#include <sys/types.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifndef MM
#define	MM void
#endif

#define	MM_SEM_NUM		5
#define	mm_lock(x)		_mm_lock(x,0)
#define	mm_unlock(x)	_mm_unlock(x,0)
#define	mm_lock1(x)		_mm_lock(x,1)
#define	mm_unlock1(x)	_mm_unlock(x,1)
#define	mm_lock2(x)		_mm_lock(x,2)
#define	mm_unlock2(x)	_mm_unlock(x,2)
#define	mm_lock3(x)		_mm_lock(x,3)
#define	mm_unlock3(x)	_mm_unlock(x,3)
#define	mm_lock4(x)		_mm_lock(x,4)
#define	mm_unlock4(x)	_mm_unlock(x,4)

#define MM_PROT_NONE  1
#define MM_PROT_READ  2
#define MM_PROT_WRITE 4
#define MM_PROT_EXEC  8

MM *mm_create(size_t size); // create mm by mmap
size_t mm_size(MM *mm);
void mm_destroy(MM *mm);
int _mm_lock(MM *mm, int num); // lock this mm
int _mm_unlock(MM *mm, int num);
int mm_protect(MM *mm, int mode); // protect the mm to avoid read|write?
size_t mm_available(MM *mm);
size_t mm_maxsize(MM *mm);
void *mm_malloc(MM *mm, size_t size);
void mm_free(MM *mm, void *p);
void *mm_malloc_nolock(MM *mm, size_t size);
void mm_free_nolock(MM *mm, void *p);
size_t mm_sizeof(MM *mm, void *x);

#ifdef __cplusplus
}
#endif

#endif	/* __XS_MM_20090527_H__ */
