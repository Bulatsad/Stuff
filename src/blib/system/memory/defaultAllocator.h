#pragma once

#include <blib/system/memory/allocatorTraits.h>
#include <blib/system/memory/globalAllocator.h>

// Опциональное включение DebugAllocator в debug builds
#if defined(_DEBUG) && !defined(BLIB_DEBUG_ALLOCATOR_DISABLED)
    #define BLIB_DEBUG_ALLOCATOR_ENABLED
    #include <blib/system/memory/allocators/debugAllocator.h>
#endif

namespace blib
{
namespace memory
{
    /**
     * DefaultAllocatorImpl - внутренняя реализация дефолтного аллокатора.
     * 
     * Назначение:
     * - Простой прокси к GlobalAllocator::instance()
     * - Stateless - не имеет состояния, все экземпляры идентичны
     * - Thread-safe (через GlobalAllocator)
     * - Собирает статистику через GlobalAllocator
     * 
     * Это внутренний класс, пользователи должны использовать DefaultAllocator typedef.
     */
    class DefaultAllocatorImpl
    {
    public:
        /**
         * Выделить блок памяти через GlobalAllocator.
         * 
         * @param size Размер блока в байтах
         * @return Указатель на выделенный блок или nullptr при ошибке
         */
        void* allocate(size_t size)
        {
            return GlobalAllocator::instance().allocate(size);
        }

        /**
         * Освободить блок памяти через GlobalAllocator.
         * 
         * @param ptr Указатель на блок (должен быть получен из allocate)
         * @param size Размер блока в байтах (должен совпадать с allocate)
         */
        void deallocate(void* ptr, size_t size)
        {
            GlobalAllocator::instance().deallocate(ptr, size);
        }
    };

    /**
     * DefaultAllocator - публичный typedef дефолтного аллокатора.
     * 
     * Поведение зависит от build mode:
     * 
     * Debug builds (BLIB_DEBUG_ALLOCATOR_ENABLED):
     *   - Автоматически оборачивается в DebugAllocator<DefaultAllocatorImpl>
     *   - Guard bytes для overflow/underflow detection
     *   - Poison memory для use-after-free detection
     *   - Double-free detection
     *   - Header validation с checksum
     *   - Overhead: +40 байт на каждую аллокацию
     *   - Ошибки вызывают abort() с сообщением
     * 
     * Release builds:
     *   - Прямой DefaultAllocatorImpl (прокси к GlobalAllocator)
     *   - Минимальный overhead
     *   - Максимальная производительность
     * 
     * Использование:
     *   DefaultAllocator alloc;
     *   void* ptr = alloc.allocate(100);
     *   alloc.deallocate(ptr, 100);
     * 
     * Управление debug режимом:
     *   - По умолчанию: _DEBUG определяет поведение
     *   - Отключить в debug: #define BLIB_DEBUG_ALLOCATOR_DISABLED
     *   - Включить в release: #define BLIB_DEBUG_ALLOCATOR_ENABLED
     * 
     * Характеристики:
     * - Debug: Stateful (DebugAllocator хранит underlying allocator)
     * - Release: Stateless (простой прокси)
     * - Thread-safe через GlobalAllocator
     */
#ifdef BLIB_DEBUG_ALLOCATOR_ENABLED
    using DefaultAllocator = DebugAllocator<DefaultAllocatorImpl>;
#else
    using DefaultAllocator = DefaultAllocatorImpl;
#endif

    /**
     * Специализация AllocatorTraits для DefaultAllocatorImpl.
     * Помечаем как stateless - не имеет состояния.
     */
    template<>
    struct AllocatorTraits<DefaultAllocatorImpl>
    {
        static constexpr bool isStateless = true;
    };

    /**
     * Специализация AllocatorTraits для DefaultAllocator в debug режиме.
     * В debug DefaultAllocator = DebugAllocator<...> - stateful!
     */
#ifdef BLIB_DEBUG_ALLOCATOR_ENABLED
    template<>
    struct AllocatorTraits<DefaultAllocator>
    {
        static constexpr bool isStateless = false; // DebugAllocator stateful
    };
#else
    // В release DefaultAllocator == DefaultAllocatorImpl
    // Trait уже определён выше для DefaultAllocatorImpl
#endif

} // namespace memory
} // namespace blib
