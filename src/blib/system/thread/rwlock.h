#pragma once

#include <blib/config.h>

#ifdef __blib_compile_platform_windows
#include <Windows.h>
typedef SRWLOCK locker_t;
#elif defined(__blib_compile_platform_linux)
#include <pthread.h>
typedef pthread_rwlock_t locker_t;
#else
#error "Define rwlock type"
#endif // платформа


namespace thread
{
    class RWLocker
    {
    private:
        locker_t rwsyncer;
    public:
        void readLock();
        void readUnock();

        void writeLock();
        void writeUnlock();

        RWLocker();
        ~RWLocker();
        RWLocker(const RWLocker&) = delete;
        RWLocker(RWLocker&&) = delete;
    };
}