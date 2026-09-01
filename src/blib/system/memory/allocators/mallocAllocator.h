#pragma once

#include <cstdlib>
#include <cstddef>

#include <blib/system/memory/allocatorTraits.h>
#include <blib/system/memory/globalAllocator.h>

// Опциональное включение DebugAllocator в debug builds
#if defined(BLIB_DEBUG) && !defined(BLIB_DEBUG_ALLOCATOR_DISABLED)
    #define BLIB_DEBUG_ALLOCATOR_ENABLED
    #include <blib/system/memory/allocators/debugAllocator.h>
#endif

namespace blib
{
namespace memory
{
    /**
     * MallocAllocatorImpl - внутренняя реализация, простая stateless обёртка над malloc/free.
     * 
     * Назначение:
     * - Прямой вызов системных malloc/free без дополнительной обработки
     * - Stateless - не имеет состояния, все экземпляры идентичны
     * - НЕ использует GlobalAllocator (прямой вызов libc)
     * - Полезен когда нужна совместимость с C API или минимальный overhead
     * 
     * Это внутренний класс, пользователи должны использовать MallocAllocator typedef.
     */
    class MallocAllocatorImpl
    {
    public:
        /**
         * Выделить блок памяти через malloc.
         * 
         * @param size Размер блока в байтах (должен быть > 0)
         * @return Указатель на выделенный блок или nullptr при ошибке
         */
        void* allocate(size_t size)
        {
            if (size == 0)
            {
                return nullptr;
            }
            return std::malloc(size);
        }

        /**
         * Освободить блок памяти через free.
         * 
         * @param ptr Указатель на блок (должен быть получен из allocate)
         * @param size Размер блока (игнорируется, free не требует размера)
         */
        void deallocate(void* ptr, size_t size)
        {
            (void)size;
            if (!ptr)
            {
                return;
            }
            std::free(ptr);
        }
    };

    /**
     * MallocAllocator - публичный typedef аллокатора malloc/free.
     * 
     * Поведение зависит от build mode:
     * 
     * Debug builds (BLIB_DEBUG_ALLOCATOR_ENABLED):
     *   - Автоматически оборачивается в DebugAllocator<MallocAllocatorImpl>
     *   - Guard bytes для overflow/underflow detection
     *   - Poison memory для use-after-free detection
     *   - Double-free detection
     *   - Overhead: +40 байт на каждую аллокацию
     * 
     * Release builds:
     *   - Прямой MallocAllocatorImpl (прямой malloc/free)
     *   - Минимальный overhead
     *   - Максимальная производительность
     * 
     * Использование:
     *   MallocAllocator alloc;
     *   void* ptr = alloc.allocate(1024);
     *   alloc.deallocate(ptr, 1024);
     * 
     * Характеристики:
     * - Debug: Stateful (DebugAllocator хранит underlying allocator)
     * - Release: Stateless (простая обёртка malloc/free)
     * - Thread-safe (malloc/free thread-safe в современных libc)
     * - НЕ собирает статистику через GlobalAllocator
     * 
     * Отличия от DefaultAllocator:
     * - DefaultAllocator: new/delete через GlobalAllocator (статистика)
     * - MallocAllocator: прямой malloc/free (без статистики)
     * 
     * Когда использовать:
     * - Взаимодействие с C API
     * - Минимальный overhead в release
     * - Когда статистика не нужна
     */
#ifdef BLIB_DEBUG_ALLOCATOR_ENABLED
    using MallocAllocator = DebugAllocator<MallocAllocatorImpl>;
#else
    using MallocAllocator = MallocAllocatorImpl;
#endif

    /**
     * Специализация AllocatorTraits для MallocAllocatorImpl.
     * Помечаем как stateless - не имеет состояния.
     */
    template<>
    struct AllocatorTraits<MallocAllocatorImpl>
    {
        static constexpr bool isStateless = true;
    };

    /**
     * Специализация AllocatorTraits для MallocAllocator в debug режиме.
     */
#ifdef BLIB_DEBUG_ALLOCATOR_ENABLED
    template<>
    struct AllocatorTraits<MallocAllocator>
    {
        static constexpr bool isStateless = false; // DebugAllocator stateful
    };
#endif

} // namespace memory
} // namespace blib
