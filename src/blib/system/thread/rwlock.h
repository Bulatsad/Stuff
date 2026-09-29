#pragma once

#include <blib/config.h>

#ifdef __blib_compile_platform_windows
#include <Windows.h>

// Windows.h (через wingdi.h) определяет устаревшие макросы rad1/rad2/rad3.
// Они протекают в любой код, включающий rwlock.h (например, console.h),
// и ломают пользовательские идентификаторы с такими же именами.
// Снимаем их — сами макросы давно не используются даже в WinAPI.
#undef rad1
#undef rad2
#undef rad3

typedef SRWLOCK locker_t;
#elif defined(__blib_compile_platform_linux)
#include <pthread.h>
typedef pthread_rwlock_t locker_t;
#else
#error "Define rwlock type"
#endif // платформа


namespace thread
{
    // __blib_system_api — экспорт в shared-сборке blib-system
    // (blib_build_dynamic): потребители (blib-core и выше) импортируют
    class __blib_system_api RWLocker
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