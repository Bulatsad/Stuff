#include <blib/memory/allocator.h>
#include <blib/memory/defaultAllocator.h>
#include <blib/memory/allocatorTraits.h>

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
            IAllocatorImpl* sharedImpl = other.shareImpl();
            
            // Пытаемся разместить в SBO
            // TODO: Проверка размера и размещение
            // Пока просто сохраняем указатель (будет в heap из share())
            impl = sharedImpl;
        }
    }

    Allocator::Allocator(Allocator&& other) noexcept
        : storage() // SBO конструируется пустым
        , impl(other.impl)
    {
        // Забираем impl у other
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

            // Забираем impl у other
            impl = other.impl;
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
        if (impl)
        {
            // Вызываем деструктор
            impl->~IAllocatorImpl();

            // Освобождаем память если была в heap
            // TODO: Правильное определение где impl - в SBO или в heap
            // Пока упрощённая версия - считаем что всегда в SBO для дефолта
            
            impl = nullptr;
        }
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
