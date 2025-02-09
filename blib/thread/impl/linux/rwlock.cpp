#include <rwlock.h>

#include <inline.h>

#include <pthread.h>

typedef pthread_rwlock_t locker_t;

__blib_inline void thread::ReadWriteSyncronizer::readLock()
{
    pthread_rwlock_rdlock((locker_t*)this->rwsyncer);
}

__blib_inline void thread::ReadWriteSyncronizer::readUnock()
{
    pthread_rwlock_unlock((locker_t*)this->rwsyncer);
}

__blib_inline void thread::ReadWriteSyncronizer::writeLock()
{
    pthread_rwlock_wrlock((locker_t*)this->rwsyncer);
}

__blib_inline void thread::ReadWriteSyncronizer::writeUnlock()
{
    pthread_rwlock_unlock((locker_t*)this->rwsyncer);
}

__blib_inline thread::ReadWriteSyncronizer::ReadWriteSyncronizer()
{
    this->rwsyncer = new pthread_rwlock_t;
    pthread_rwlock_init(this->rwsyncer, nullptr);
}

__blib_inline thread::ReadWriteSyncronizer::~ReadWriteSyncronizer()
{
    pthread_rwlock_destroy(this->rwsyncer);
    delete this->rwsyncer;
}
