#pragma once

#include <blib/inline.h>

template<class Mutex_t>
class MutexLocker
{
private:
    Mutex_t* pm;
public:
    MutexLocker() = delete;
    MutexLocker(const MutexLocker&) = delete;
    MutexLocker(MutexLocker&&) = delete;
    MutexLocker(Mutex_t* m);
    ~MutexLocker();
};

template<class Mutex_t>
__blib_inline MutexLocker<Mutex_t>::MutexLocker(Mutex_t* m)
{
    m->lock();
    this->pm = m;
}

template<class Mutex_t>
__blib_inline MutexLocker<Mutex_t>::~MutexLocker()
{
    this->pm->unlock();
    this->pm = nullptr;
}
