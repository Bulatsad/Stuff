// allocator.inl - реализация шаблонного конструктора Allocator
// Этот файл включается в конец allocator.h

#pragma once

#include <blib/system/memory/allocatorTraits.h>
#include <utility>

namespace blib
{
namespace memory
{
    // Forward declaration внутренних классов (определены в allocator.cpp)
    template<typename AllocatorType, bool IsStateless>
    class AllocatorImplWrapper;

    /**
     * Шаблонный конструктор Allocator от конкретного аллокатора.
     * 
     * Реализует type erasure:
     * 1. Проверяет AllocatorTraits<AllocatorImpl>::isStateless
     * 2. Создаёт соответствующую обёртку (stateless или stateful)
     * 3. Размещает обёртку в SBO (если влезает) или в heap
     * 
     * @tparam AllocatorImpl Тип конкретного аллокатора
     * @param alloc Экземпляр аллокатора (будет перемещён в обёртку)
     */
    template<typename AllocatorImpl, typename>
    Allocator::Allocator(AllocatorImpl&& alloc)
        : impl(nullptr)
    {
        // Определяем stateless или stateful через traits
        constexpr bool isStateless = AllocatorTraits<AllocatorImpl>::isStateless;

        // Создаём соответствующую обёртку
        using WrapperType = AllocatorImplWrapper<AllocatorImpl, isStateless>;

        // Размещаем обёртку в SBO или heap
        // Для stateless: WrapperType не хранит состояние, маленький размер
        // Для stateful: WrapperType хранит AllocatorImpl, может быть больше
        
        constexpr bool fitsInSBO = sizeof(WrapperType) <= SBO_SIZE;
        
        if constexpr (isStateless)
        {
            // Stateless - создаём пустую обёртку
            if constexpr (fitsInSBO)
            {
                auto* wrapper = storage.construct<WrapperType>();
                impl = reinterpret_cast<IAllocatorImpl*>(wrapper);
            }
            else
            {
                auto* wrapper = storage.constructInHeap<WrapperType>();
                impl = reinterpret_cast<IAllocatorImpl*>(wrapper);
            }
        }
        else
        {
            // Stateful - создаём обёртку с перемещением аллокатора
            if constexpr (fitsInSBO)
            {
                auto* wrapper = storage.construct<WrapperType>(std::forward<AllocatorImpl>(alloc));
                impl = reinterpret_cast<IAllocatorImpl*>(wrapper);
            }
            else
            {
                auto* wrapper = storage.constructInHeap<WrapperType>(std::forward<AllocatorImpl>(alloc));
                impl = reinterpret_cast<IAllocatorImpl*>(wrapper);
            }
        }
    }

} // namespace memory
} // namespace blib
