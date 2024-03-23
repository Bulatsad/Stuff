#pragma once

#include <blib/align.h>
#include <blib/thread/mutexLocker.h>

#include <memory>
#include <mutex>

namespace blib
{
    template<class _Ty, class _Alloc = std::allocator<_Ty> >
    class CircleQueue
    {
    private:
        __blib_cache_aligned _Alloc allocator;
        __blib_cache_aligned _Ty* data;
        __blib_cache_aligned size_t reader;
        __blib_cache_aligned size_t writer;
        __blib_cache_aligned size_t size;
        __blib_cache_aligned size_t capacity;
        __blib_cache_aligned std::mutex m;
    public:
        template<class ...Args>
        CircleQueue(size_t capacity, Args&& ... args);
        ~CircleQueue();
        bool push(const _Ty& d);
        bool pop(_Ty& res);
    };
}



template<class _Ty, class _Alloc>
template<class ...Args>
__blib_inline blib::CircleQueue<_Ty, _Alloc>::CircleQueue(size_t capacity, Args&& ... args)
{
    this->data = this->allocator.allocate(capacity);
    this->capacity = capacity;
    this->reader = 0;
    this->writer = 0;
    this->size = 0;

    for (size_t i = 0; i < this->capacity; i++)
        this->allocator.construct(&(this->data[i]), std::forward<Args>(args)...);

}

template<class _Ty, class _Alloc>
__blib_inline blib::CircleQueue<_Ty, _Alloc> ::~CircleQueue()
{
    for (size_t i = 0; i < this->capacity; i++)
        this->allocator.destroy(&(this->data[i]));
    this->allocator.deallocate((this->data), this->capacity);
}

template<class _Ty, class _Alloc>
__blib_inline bool blib::CircleQueue<_Ty, _Alloc>::push(const _Ty& d)
{
    if (this->size == this->capacity)
    {
        return false;
    }
    MutexLocker<std::mutex> locker(&m);
    this->data[this->writer] = d;
    this->size++;
    if (this->writer + 1 >= this->capacity)
        this->writer = 0;
    else
        this->writer++;
    return true;
}

template<class _Ty, class _Alloc>
__blib_inline bool blib::CircleQueue<_Ty, _Alloc>::pop(_Ty& res)
{
    if (this->size > 0)
    {

        MutexLocker<std::mutex> locker(&m);
        this->size--;
        res = std::move(this->data[this->reader]);
        if (this->reader + 1 >= this->capacity)
            this->reader = 0;
        else
            this->reader++;
        return true;
    }
    return false;
}