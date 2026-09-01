#include <blib/system/thread/rwlock.h>

#include <blib/inline.h>

#include <Windows.h>

void thread::RWLocker::readLock()
{
    AcquireSRWLockShared(&(this->rwsyncer));
}

void thread::RWLocker::readUnock()
{
    ReleaseSRWLockShared(&(this->rwsyncer));
}

void thread::RWLocker::writeLock()
{
    AcquireSRWLockExclusive(&(this->rwsyncer));
}

void thread::RWLocker::writeUnlock()
{
     ReleaseSRWLockExclusive(&(this->rwsyncer));
}

thread::RWLocker::RWLocker()
{
    InitializeSRWLock(&(this->rwsyncer));
}

thread::RWLocker::~RWLocker()
{
}
