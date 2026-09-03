/*
Multi producer - single consumer thread safe queue.
В отличие от LocklessProducerConcumerCircleQueue синхронизация строится
на RWLocker (SRWLOCK на Windows), а не на атомиках:
    - продюсеры сериализуются через writeLock (push безопасен из любого потока);
    - консюмер читает под readLock (pop/isEmpty).
Контракт: консюмер должен быть ровно один — два параллельных pop нарушат
целостность индекса чтения (readLock допускает одновременных читателей).
Семантика переполнения: при заполненном буфере push роняет САМОЕ СТАРОЕ
сообщение (в отличие от SPSC-версии, которая возвращает false), поэтому
push всегда успешен и ничего не возвращает. Продвигать reader под
writeLock безопасно: консюмер в этот момент исключён.
Тип не lockless: за синхронизацию отвечают локи ОС. False sharing между
reader/writer не критичен, т.к. синхронизация идёт через лок, поэтому
__blib_cache_aligned не используется.
*/

#pragma once

#include <blib/system/thread/rwlock.h>
#include <blib/inline.h>

#include <memory>
#include <utility>

namespace blib
{
    template<class _Ty, class _Alloc = std::allocator<_Ty> >
    class MultiProducerSingleConsumerCircleQueue
    {
    private:
        _Ty* data;
        mutable thread::RWLocker syncer; // mutable: блокировка — не логическое состояние
        size_t reader;
        size_t writer;
        size_t capacity;
        _Alloc allocator; // Allocator MUST be last cause alignment. If not +cache_line to structure size!
    public:
        MultiProducerSingleConsumerCircleQueue();

        template<class ...Args>
        MultiProducerSingleConsumerCircleQueue(size_t _capacity, Args&& ... args);
        ~MultiProducerSingleConsumerCircleQueue();

        template<class ...Args>
        void reset(size_t _capacity, Args&& ... args);

        // Всегда успешен: при переполнении роняет самое старое
        void push(const _Ty& d);
        void push(_Ty&& d);

        bool pop(_Ty& res);

        bool isEmpty() const;
    };
}

template<class _Ty, class _Alloc>
__blib_inline blib::MultiProducerSingleConsumerCircleQueue<_Ty, _Alloc>::MultiProducerSingleConsumerCircleQueue()
    : data(nullptr)
    , reader(0)
    , writer(0)
    , capacity(0)
{
    this->reset(0);
}

template<class _Ty, class _Alloc>
template<class ...Args>
__blib_inline blib::MultiProducerSingleConsumerCircleQueue<_Ty, _Alloc>::MultiProducerSingleConsumerCircleQueue(size_t _capacity, Args&& ... args)
    : data(nullptr)
    , reader(0)
    , writer(0)
    , capacity(0)
{
    this->reset(_capacity, std::forward<Args>(args)...);
}

template<class _Ty, class _Alloc>
__blib_inline blib::MultiProducerSingleConsumerCircleQueue<_Ty, _Alloc>::~MultiProducerSingleConsumerCircleQueue()
{
    for (size_t i = 0; i < this->capacity; i++)
        this->allocator.destroy(&(this->data[i]));
    this->allocator.deallocate(this->data, this->capacity);
}

template<class _Ty, class _Alloc>
template<class ...Args>
__blib_inline void blib::MultiProducerSingleConsumerCircleQueue<_Ty, _Alloc>::reset(size_t _capacity, Args&& ... args)
{
    // В отличие от LocklessProducerConcumerCircleQueue корректно
    // освобождаем старый буфер перед выделением нового
    if (this->data != nullptr)
    {
        for (size_t i = 0; i < this->capacity; i++)
            this->allocator.destroy(&(this->data[i]));
        this->allocator.deallocate(this->data, this->capacity);
    }

    // +1 sentinel-слот: при reader == writer буфер считается пустым,
    // поэтому реально хранится не более _capacity элементов
    _capacity++;
    this->data = this->allocator.allocate(_capacity);
    this->capacity = _capacity;
    this->reader = 0;
    this->writer = 0;

    for (size_t i = 0; i < this->capacity; i++)
        this->allocator.construct(&(this->data[i]), std::forward<Args>(args)...);
}

template<class _Ty, class _Alloc>
__blib_inline void blib::MultiProducerSingleConsumerCircleQueue<_Ty, _Alloc>::push(const _Ty& d)
{
    this->syncer.writeLock();

    const size_t next = (this->writer + 1 == this->capacity) ? 0 : this->writer + 1;

    // Переполнение: продвигаем reader — роняем самое старое.
    // Безопасно: writeLock исключает консюмера, который мог бы
    // читать этот же слот под readLock.
    if (next == this->reader)
        this->reader = (this->reader + 1 == this->capacity) ? 0 : this->reader + 1;

    this->data[this->writer] = d;
    this->writer = next;

    this->syncer.writeUnlock();
}

template<class _Ty, class _Alloc>
__blib_inline void blib::MultiProducerSingleConsumerCircleQueue<_Ty, _Alloc>::push(_Ty&& d)
{
    this->syncer.writeLock();

    const size_t next = (this->writer + 1 == this->capacity) ? 0 : this->writer + 1;

    // Переполнение: продвигаем reader — роняем самое старое.
    // Безопасно: writeLock исключает консюмера, который мог бы
    // читать этот же слот под readLock.
    if (next == this->reader)
        this->reader = (this->reader + 1 == this->capacity) ? 0 : this->reader + 1;

    this->data[this->writer] = std::move(d);
    this->writer = next;

    this->syncer.writeUnlock();
}

template<class _Ty, class _Alloc>
__blib_inline bool blib::MultiProducerSingleConsumerCircleQueue<_Ty, _Alloc>::pop(_Ty& res)
{
    this->syncer.readLock();

    if (this->writer == this->reader)
    {
        this->syncer.readUnock();
        return false;
    }

    res = std::move(this->data[this->reader]);
    this->reader = (this->reader + 1 == this->capacity) ? 0 : this->reader + 1;

    this->syncer.readUnock();
    return true;
}

template<class _Ty, class _Alloc>
__blib_inline bool blib::MultiProducerSingleConsumerCircleQueue<_Ty, _Alloc>::isEmpty() const
{
    this->syncer.readLock();
    const bool res = (this->writer == this->reader);
    this->syncer.readUnock();
    return res;
}
