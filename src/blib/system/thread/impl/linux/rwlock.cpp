#include <blib/system/thread/rwlock.h>

#include <blib/inline.h>

void thread::RWLocker::readLock()
{
    pthread_rwlock_rdlock(&(this->rwsyncer));
}

void thread::RWLocker::readUnock()
{
    pthread_rwlock_unlock(&(this->rwsyncer));
}

void thread::RWLocker::writeLock()
{
    pthread_rwlock_wrlock(&(this->rwsyncer));
}

void thread::RWLocker::writeUnlock()
{
    pthread_rwlock_unlock(&(this->rwsyncer));
}

thread::RWLocker::RWLocker()
{
    pthread_rwlock_init(&(this->rwsyncer), nullptr);
}

thread::RWLocker::~RWLocker()
{
    pthread_rwlock_destroy(&(this->rwsyncer));
}
