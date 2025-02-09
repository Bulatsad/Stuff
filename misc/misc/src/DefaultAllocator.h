#pragma once

#include <utility>
#include "Types.h"
#include "Macroses.h"

namespace blib
{
    class DefaultAllocator
    {
    public:
        template<typename T, typename ...Args>
        static T* create(Args&&... args);
        template<typename T>
        static void destroy(T*p);
        template<typename T>
        static T* alloc();
        template<typename T>
        static void free(T*p);

    };

    template<typename T, typename ...Args>
    INLINE T* DefaultAllocator::create(Args&&... args)
    {
        return new T(std::forward<Args>(args)...);
    }

    template<typename T>
    INLINE void DefaultAllocator::destroy(T*p)
    {
        delete p;
    }

    template<typename T>
    INLINE T* DefaultAllocator::alloc()
    {
        new uint8_t[sizeof(T)];
    }

    template<typename T>
    INLINE void DefaultAllocator::free(T* p)
    {
        delete static_cast<uint8_t*>(p);
    }

}
