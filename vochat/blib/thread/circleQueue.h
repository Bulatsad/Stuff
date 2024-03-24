/*
Single producer - single consumer thread safe queue
*/

#pragma once

#include <blib/align.h>
#include <blib/thread/mutexLocker.h>

#include <memory>
#include <mutex>
#include <atomic>

namespace blib
{
    template<class _Ty, class _Alloc = std::allocator<_Ty> >
    class __blib_cache_aligned LocklessProducerConcumerCircleQueue
    {
    private:
        __blib_cache_aligned _Ty* data;
        __blib_cache_aligned std::atomic<size_t> reader;
        __blib_cache_aligned std::atomic<size_t> writer;
        __blib_cache_aligned size_t capacity;
        _Alloc allocator; // Allocator MUST be last cause alignment. If not +cache_line to structure size!
    public:
        LocklessProducerConcumerCircleQueue();

        template<class ...Args>
        LocklessProducerConcumerCircleQueue(size_t _capacity, Args&& ... args);
        ~LocklessProducerConcumerCircleQueue();

        template<class ...Args>
        void reset(size_t _capacity, Args&& ... args);

        bool push(const _Ty& d);
        bool push(_Ty&& d);
        bool pop(_Ty& res);

        bool isEmpty() const;
    };
}

template<class _Ty, class _Alloc>
__blib_inline blib::LocklessProducerConcumerCircleQueue<_Ty, _Alloc>::LocklessProducerConcumerCircleQueue()
{
    reset(0);
}

template<class _Ty, class _Alloc>
template<class ...Args>
__blib_inline blib::LocklessProducerConcumerCircleQueue<_Ty, _Alloc>::LocklessProducerConcumerCircleQueue(size_t _capacity, Args&& ... args)
{
    this->~LocklessProducerConcumerCircleQueue();
    this->reset(_capacity, std::forward<Args>(args)...);
}

template<class _Ty, class _Alloc>
__blib_inline blib::LocklessProducerConcumerCircleQueue<_Ty, _Alloc> ::~LocklessProducerConcumerCircleQueue()
{
    for (size_t i = 0; i < this->capacity; i++)
        this->allocator.destroy(&(this->data[i]));
    this->allocator.deallocate((this->data), this->capacity);
}

template<class _Ty, class _Alloc>
template<class ...Args>
__blib_inline void blib::LocklessProducerConcumerCircleQueue<_Ty, _Alloc>::reset(size_t _capacity, Args && ...args)
{
    _capacity++;
    this->data = this->allocator.allocate(_capacity);
    this->capacity = _capacity;
    this->writer.store(0, std::memory_order::memory_order_release);
    this->reader.store(0, std::memory_order::memory_order_release);

    for (size_t i = 0; i < this->capacity; i++)
        this->allocator.construct(&(this->data[i]), std::forward<Args>(args)...);
}

template<class _Ty, class _Alloc>
__blib_inline bool blib::LocklessProducerConcumerCircleQueue<_Ty, _Alloc>::push(const _Ty& d)
{
    const size_t w = this->writer.load(std::memory_order::memory_order_relaxed);
    const size_t r = this->reader.load(std::memory_order::memory_order_acquire);
    bool writable;

    if (r > w)
        writable = w + 1 != r;
    else if (r < w)
        writable = w + 1 == this->capacity ? r != 0 : true;
    else
        writable = true;

    if (writable == 0)
        return false;

    this->data[w] = d;
    if (w + 1 == this->capacity)
        this->writer.store(0, std::memory_order::memory_order_release);
    else
        this->writer.store(w + 1, std::memory_order::memory_order_release);
    return true;
}

template<class _Ty, class _Alloc>
__blib_inline bool blib::LocklessProducerConcumerCircleQueue<_Ty, _Alloc>::push(_Ty&& d)
{
    const size_t w = this->writer.load(std::memory_order::memory_order_relaxed);
    const size_t r = this->reader.load(std::memory_order::memory_order_acquire);
    bool writable;

    if (r > w)
        writable = w + 1 != r;
    else if (r < w)
        writable = w + 1 == this->capacity ? r != 0 : true;
    else
        writable = true;

    if (writable == 0)
        return false;

    this->data[w] = std::move(d);
    if (w + 1 == this->capacity)
        this->writer.store(0, std::memory_order::memory_order_release);
    else
        this->writer.store(w + 1, std::memory_order::memory_order_release);
    return true;
}

template<class _Ty, class _Alloc>
__blib_inline bool blib::LocklessProducerConcumerCircleQueue<_Ty, _Alloc>::pop(_Ty& res)
{
    const std::size_t w = this->writer.load(std::memory_order::memory_order_relaxed);
    const std::size_t r = this->reader.load(std::memory_order::memory_order_acquire);
    if (w == r)
        return false;

    res = std::move(this->data[this->reader]);
    if (r + 1 == this->capacity)
        this->reader.store(0, std::memory_order::memory_order_release);
    else
        this->reader.store(r + 1, std::memory_order::memory_order_release);
    return true;
}

template<class _Ty, class _Alloc>
inline bool blib::LocklessProducerConcumerCircleQueue<_Ty, _Alloc>::isEmpty() const
{
    const std::size_t w = this->writer.load(std::memory_order::memory_order_relaxed);
    const std::size_t r = this->reader.load(std::memory_order::memory_order_acquire);
    if (w == r)
        return true;
    return false;
}
