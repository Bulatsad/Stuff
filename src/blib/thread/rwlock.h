#pragma once

#ifdef WIN32
#include <Windows.h>
typedef SRWLOCK locker_t;
#else
#error "Define rwlock type"
#endif // !WIN32


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