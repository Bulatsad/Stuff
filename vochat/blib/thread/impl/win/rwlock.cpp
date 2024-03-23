#include <blib/thread/rwlock.h>

#include <blib/inline.h>

#include <Windows.h>

typedef SRWLOCK locker_t;

__blib_inline void thread::ReadWriteSyncronizer::readLock()
{
    AcquireSRWLockShared((locker_t*)this->rwsyncer);
}

__blib_inline void thread::ReadWriteSyncronizer::readUnock()
{
    ReleaseSRWLockShared((locker_t*)this->rwsyncer);
}

__blib_inline void thread::ReadWriteSyncronizer::writeLock()
{
    AcquireSRWLockExclusive((locker_t*)this->rwsyncer);
}

__blib_inline void thread::ReadWriteSyncronizer::writeUnlock()
{
     ReleaseSRWLockExclusive((locker_t*)this->rwsyncer);
}

__blib_inline thread::ReadWriteSyncronizer::ReadWriteSyncronizer()
{
    InitializeSRWLock((locker_t*)this->rwsyncer);
}

__blib_inline thread::ReadWriteSyncronizer::~ReadWriteSyncronizer()
{
    this->rwsyncer = nullptr;
}
