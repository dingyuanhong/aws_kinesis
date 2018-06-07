#include "core/thread.h"

class ReadLock {
public:
	ReadLock(uv_rwlock_t* rw)
		:rwlock(rw)
	{
		uv_rwlock_rdlock(rwlock);
	}
	~ReadLock() {
		uv_rwlock_rdunlock(rwlock);
	}
private:
	uv_rwlock_t* rwlock;
};

class WriteLock {
public:
	WriteLock(uv_rwlock_t* rw)
		:rwlock(rw)
	{
		uv_rwlock_rdlock(rwlock);
	}
	~WriteLock() {
		uv_rwlock_rdunlock(rwlock);
	}
private:
	uv_rwlock_t* rwlock;
};