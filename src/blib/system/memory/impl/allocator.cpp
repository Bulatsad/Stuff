#include <blib/system/memory/allocator.h>
#include <blib/system/memory/defaultAllocator.h>
#include <blib/system/memory/allocatorTraits.h>

namespace blib
{
namespace memory
{
    // ============================================================================
    // Реализация Allocator
    // ============================================================================

    Allocator::Allocator()
        : impl(nullptr)
    {
        // Создаём дефолтный аллокатор
        // В debug режиме это DebugAllocator<DefaultAllocatorImpl> (stateful)
        // В release это DefaultAllocatorImpl (stateless)
        constexpr bool isStateless = AllocatorTraits<DefaultAllocator>::isStateless;
        using DefaultImpl = AllocatorImplWrapper<DefaultAllocator, isStateless>;
        
        // Размещаем в SBO (если влезает) или в heap
        if constexpr (isStateless)
        {
            // Stateless - не требует аргументов
            impl = storage.construct<DefaultImpl>();
        }
        else
        {
            // Stateful - создаём экземпляр DefaultAllocator и передаём в wrapper
            DefaultAllocator defaultAlloc(DefaultAllocatorImpl{});
            impl = storage.construct<DefaultImpl>(std::move(defaultAlloc));
        }
    }

    Allocator::~Allocator()
    {
        destroyImpl();
    }

    Allocator::Allocator(const Allocator& other)
        : impl(nullptr)
    {
        if (other.impl)
        {
            // Создаём shared копию через share()
            // share() выделяет heap-объект через GlobalAllocator,
            // освобождение происходит в destroyImpl() (путь 3)
            impl = other.shareImpl();
        }
    }

    Allocator::Allocator(Allocator&& other) noexcept
        : storage(std::move(other.storage)) // Переносим SBO (inline-байты или heapPtr)
        , impl(other.impl)
    {
        // Если impl жил в inline-буфере other - он "переехал" в наш буфер,
        // поэтому указатель нужно пересчитать на свой storage
        if (impl == other.storage.get())
        {
            impl = static_cast<IAllocatorImpl*>(storage.get());
        }

        // Исходник больше не владеет impl
        other.impl = nullptr;
    }

    Allocator& Allocator::operator=(const Allocator& other)
    {
        if (this != &other)
        {
            // Уничтожаем старый impl
            destroyImpl();

            if (other.impl)
            {
                // Создаём shared копию
                impl = other.shareImpl();
            }
        }
        return *this;
    }

    Allocator& Allocator::operator=(Allocator&& other) noexcept
    {
        if (this != &other)
        {
            // Уничтожаем старый impl
            destroyImpl();

            // Переносим SBO (inline-байты или heapPtr)
            storage = std::move(other.storage);

            // Забираем impl у other
            impl = other.impl;

            // Если impl жил в inline-буфере other - пересчитываем на свой буфер
            if (impl == other.storage.get())
            {
                impl = static_cast<IAllocatorImpl*>(storage.get());
            }

            other.impl = nullptr;
        }
        return *this;
    }

    void* Allocator::allocate(size_t size)
    {
        if (!impl)
        {
            return nullptr;
        }
        return impl->allocate(size);
    }

    void Allocator::deallocate(_In void* ptr, size_t size)
    {
        if (!impl)
        {
            return;
        }
        impl->deallocate(ptr, size);
    }

    Allocator Allocator::clone() const
    {
        Allocator result;
        
        // Уничтожаем дефолтный impl у result
        result.destroyImpl();

        if (impl)
        {
            // Создаём глубокую копию через deepCopy()
            result.impl = cloneImpl();
        }

        return result;
    }

    void Allocator::destroyImpl()
    {
        if (!impl)
        {
            return;
        }

        // Возможны три способа хранения impl:
        // 1. Inline в SBO-буфере (impl == storage.get() и !storage.isHeap())
        // 2. В heap через SBO::constructInHeap (impl == storage.get() и storage.isHeap())
        // 3. В heap через share()/deepCopy() (storage пуст, impl != storage.get())
        //
        // ВАЖНО: implSize() должен вызываться ДО виртуального деструктора,
        // т.к. после деструктора вызывать виртуальные методы нельзя.

        if (storage.isHeap() && impl == storage.get())
        {
            // Путь 2: объект в heap, память выделял SBO через GlobalAllocator
            size_t size = impl->implSize();

            // Вызываем виртуальный деструктор
            impl->~IAllocatorImpl();

            // Освобождаем память через GlobalAllocator (::delete запрещён)
            GlobalAllocator::instance().deallocate(impl, size);

            // Сообщаем SBO что объект больше не хранится
            storage.releaseHeap();
        }
        else if (impl != storage.get())
        {
            // Путь 3: объект в heap, память выделял share()/deepCopy() через GlobalAllocator
            size_t size = impl->implSize();

            // Вызываем виртуальный деструктор
            impl->~IAllocatorImpl();

            // Освобождаем память через GlobalAllocator (::delete запрещён)
            GlobalAllocator::instance().deallocate(impl, size);
        }
        else
        {
            // Путь 1: объект в inline-буфере - только деструктор, память не освобождается
            impl->~IAllocatorImpl();
        }

        impl = nullptr;
    }

    IAllocatorImpl* Allocator::shareImpl() const
    {
        if (!impl)
        {
            return nullptr;
        }
        return impl->share();
    }

    IAllocatorImpl* Allocator::cloneImpl() const
    {
        if (!impl)
        {
            return nullptr;
        }
        return impl->deepCopy();
    }

} // namespace memory
} // namespace blib
