#ifndef THREAD_H
#define THREAD_H

#ifdef _WIN32
#include "thread_win.h"
#else

#if __GNUC__ >= 4
# define UV_EXTERN __attribute__((visibility("default")))
#else
# define UV_EXTERN
#endif

#include <stdint.h>
#include <stdlib.h>
#include <pthread.h>
#include <sys/sem.h>
#include <semaphore.h>

#if defined(__APPLE__) && defined(__MACH__)
	#include "TargetConditionals.h"
	#ifdef TARGET_OS_MAC
		typedef struct sem_mac_s{
		  sem_t *sem;
		  char * name;
		}sem_mac_t;

		#define UV_PLATFORM_SEM_T sem_mac_t
		#define clock_t clockid_t
		#define CLOCK_MONOTONIC_COARSE CLOCK_MONOTONIC
		#define USE_USERDEFINED_BARRIER
	#endif
#endif

#ifndef UV_PLATFORM_SEM_T
# define UV_PLATFORM_SEM_T sem_t
#endif

#define UV_ONCE_INIT PTHREAD_ONCE_INIT

typedef pthread_once_t uv_once_t;
typedef pthread_t uv_thread_t;
typedef pthread_mutex_t uv_mutex_t;
typedef pthread_rwlock_t uv_rwlock_t;
typedef UV_PLATFORM_SEM_T uv_sem_t;
typedef pthread_cond_t uv_cond_t;
typedef pthread_key_t uv_key_t;
#ifndef USE_USERDEFINED_BARRIER
typedef pthread_barrier_t uv_barrier_t;
#else
typedef struct {
  unsigned int n;
  unsigned int count;
  uv_mutex_t mutex;
  uv_sem_t turnstile1;
  uv_sem_t turnstile2;
} uv_barrier_t;
#endif

#endif

UV_EXTERN int uv_mutex_init(uv_mutex_t* handle);
UV_EXTERN void uv_mutex_destroy(uv_mutex_t* handle);
UV_EXTERN void uv_mutex_lock(uv_mutex_t* handle);
UV_EXTERN int uv_mutex_trylock(uv_mutex_t* handle);
UV_EXTERN void uv_mutex_unlock(uv_mutex_t* handle);

UV_EXTERN int uv_rwlock_init(uv_rwlock_t* rwlock);
UV_EXTERN void uv_rwlock_destroy(uv_rwlock_t* rwlock);
UV_EXTERN void uv_rwlock_rdlock(uv_rwlock_t* rwlock);
UV_EXTERN int uv_rwlock_tryrdlock(uv_rwlock_t* rwlock);
UV_EXTERN void uv_rwlock_rdunlock(uv_rwlock_t* rwlock);
UV_EXTERN void uv_rwlock_wrlock(uv_rwlock_t* rwlock);
UV_EXTERN int uv_rwlock_trywrlock(uv_rwlock_t* rwlock);
UV_EXTERN void uv_rwlock_wrunlock(uv_rwlock_t* rwlock);

UV_EXTERN int uv_sem_init(uv_sem_t* sem, unsigned int value);
UV_EXTERN void uv_sem_destroy(uv_sem_t* sem);
UV_EXTERN void uv_sem_post(uv_sem_t* sem);
UV_EXTERN void uv_sem_wait(uv_sem_t* sem);
UV_EXTERN int uv_sem_trywait(uv_sem_t* sem);

UV_EXTERN int uv_cond_init(uv_cond_t* cond);
UV_EXTERN void uv_cond_destroy(uv_cond_t* cond);
UV_EXTERN void uv_cond_signal(uv_cond_t* cond);
UV_EXTERN void uv_cond_broadcast(uv_cond_t* cond);

UV_EXTERN int uv_barrier_init(uv_barrier_t* barrier, unsigned int count);
UV_EXTERN void uv_barrier_destroy(uv_barrier_t* barrier);
UV_EXTERN int uv_barrier_wait(uv_barrier_t* barrier);

UV_EXTERN void uv_cond_wait(uv_cond_t* cond, uv_mutex_t* mutex);
UV_EXTERN int uv_cond_timedwait(uv_cond_t* cond,
                                uv_mutex_t* mutex,
                                uint64_t timeout);

UV_EXTERN void uv_once(uv_once_t* guard, void (*callback)(void));

UV_EXTERN int uv_key_create(uv_key_t* key);
UV_EXTERN void uv_key_delete(uv_key_t* key);
UV_EXTERN void* uv_key_get(uv_key_t* key);
UV_EXTERN void uv_key_set(uv_key_t* key, void* value);

typedef void (*uv_thread_cb)(void* arg);

UV_EXTERN int uv_thread_create(uv_thread_t* tid, uv_thread_cb entry, void* arg);
UV_EXTERN uv_thread_t uv_thread_self(void);
UV_EXTERN int uv_thread_join(uv_thread_t *tid);
UV_EXTERN int uv_thread_equal(const uv_thread_t* t1, const uv_thread_t* t2);

#endif
