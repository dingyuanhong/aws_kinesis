#ifndef THREAD_WIN_H
#define THREAD_WIN_H

#ifdef _WIN32

#include <stdint.h>
#include <stdlib.h>
#include <winsock2.h>
#include <windows.h>

/* Windows - set up dll import/export decorators. */
# if defined(BUILDING_UV_SHARED)
/* Building shared library. */
#   define UV_EXTERN __declspec(dllexport)
# elif defined(USING_UV_SHARED)
/* Using shared library. */
#   define UV_EXTERN __declspec(dllimport)
# else
/* Building static library. */
#   define UV_EXTERN /* nothing */
# endif

typedef HANDLE uv_thread_t;

typedef HANDLE uv_sem_t;

typedef CRITICAL_SECTION uv_mutex_t;

/* This condition variable implementation is based on the SetEvent solution
 * (section 3.2) at http://www.cs.wustl.edu/~schmidt/win32-cv-1.html
 * We could not use the SignalObjectAndWait solution (section 3.4) because
 * it want the 2nd argument (type uv_mutex_t) of uv_cond_wait() and
 * uv_cond_timedwait() to be HANDLEs, but we use CRITICAL_SECTIONs.
 */

typedef union {
  CONDITION_VARIABLE cond_var;
  struct {
    unsigned int waiters_count;
    CRITICAL_SECTION waiters_count_lock;
    HANDLE signal_event;
    HANDLE broadcast_event;
  } fallback;
} uv_cond_t;

typedef union {
  struct {
    unsigned int num_readers_;
    CRITICAL_SECTION num_readers_lock_;
    HANDLE write_semaphore_;
  } state_;
  /* TODO: remove me in v2.x. */
  struct {
    SRWLOCK unused_;
  } unused1_;
  /* TODO: remove me in v2.x. */
  struct {
    uv_mutex_t unused1_;
    uv_mutex_t unused2_;
  } unused2_;
} uv_rwlock_t;

typedef struct {
  unsigned int n;
  unsigned int count;
  uv_mutex_t mutex;
  uv_sem_t turnstile1;
  uv_sem_t turnstile2;
} uv_barrier_t;

typedef struct {
  DWORD tls_index;
} uv_key_t;

typedef struct uv_once_s {
  unsigned char ran;
  HANDLE event;
} uv_once_t;

#define UV_ONCE_INIT { 0, NULL }

#include <errno.h>
#define UV_EINVAL EINVAL
#define UV_EBUSY EBUSY
#define UV_ENOMEM ENOMEM
#define UV_EACCES EACCES
#define UV_EAGAIN EAGAIN
#define UV_EIO EIO
#define UV_ETIMEDOUT ETIMEDOUT

#endif

#endif