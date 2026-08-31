#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

#include <blib/blibint.h>
#include <blib/memory/allocatorTraits.h>
#include <blib/memory/globalAllocator.h>

// Опциональное включение DebugAllocator в debug builds
#if defined(_DEBUG) && !defined(BLIB_DEBUG_ALLOCATOR_DISABLED)
    #define BLIB_DEBUG_ALLOCATOR_ENABLED
    #include <blib/memory/allocators/debugAllocator.h>
#endif

namespace blib
{
namespace memory
{
    /**
     * PoolAllocatorImpl - внутренняя реализация аллокатора с пулами фиксированного размера.
     * 
     * Назначение:
     * - Быстрое выделение/освобождение блоков фиксированного размера
     * - Минимизация фрагментации памяти
     * - Идеален для частых аллокаций объектов одного размера (entity pooling, particle systems)
     * 
     * Архитектура:
     * ┌─────────────────────────────────────────────┐
     * │ Chunk 1 (blockSize * blocksPerChunk)        │
     * │  [Block][Block][Block]...[Block]            │
     * ├─────────────────────────────────────────────┤
     * │ Chunk 2                                     │
     * │  [Block][Block][Block]...[Block]            │
     * └─────────────────────────────────────────────┘
     *   Free list: Block₃ → Block₇ → Block₁₂ → ...
     * 
     * Использование:
     *   // Пул для объектов размером 64 байта, по 128 блоков в чанке
     *   PoolAllocator pool(64, 128);
     *   
     *   void* obj1 = pool.allocate(64); // быстро - из free list
     *   void* obj2 = pool.allocate(64);
     *   pool.deallocate(obj1, 64);      // возврат в free list
     *   
     *   void* obj3 = pool.allocate(64); // переиспользование obj1
     * 
     * Характеристики:
     * - Stateful (хранит chunks и free list)
     * - Не thread-safe (требуется внешняя синхронизация)
     * - O(1) allocate/deallocate в среднем случае
     * - Аллокации разных размеров не поддерживаются (только blockSize)
     * 
     * Оптимизации:
     * - Free list — связный список свободных блоков (intrusive)
     * - Chunks выделяются динамически по мере необходимости
     * - Минимальный overhead на блок (только указатель next для free blocks)
     * 
     * Ограничения:
     * - Размер аллокации должен быть == blockSize
     * - Не shrink (chunks не освобождаются даже если пусты, пока ~PoolAllocator)
     * - Alignment должен быть <= alignof(void*) или передан явно
     * 
     * Debug режим (BLIB_DEBUG_ALLOCATOR_ENABLED):
     * - blockSize автоматически увеличивается на размер debug overhead (40 байт)
     * - Это компенсирует метаданные DebugAllocator (header + guard bytes)
     * - Пользователь передаёт обычный размер объекта, корректировка происходит прозрачно
     * - Размер overhead вычисляется через DebugAllocator::getDebugOverhead()
     * 
     * TODO (будущие улучшения):
     * - Поддержка кастомного alignment
     * - Shrink механизм (освобождение пустых chunks)
     * - Thread-safe версия с per-thread pools
     * - Статистика (используемые/свободные блоки)
     * Это внутренний класс, пользователи должны использовать PoolAllocator typedef.
     */
    class PoolAllocatorImpl
    {
    public:
        /**
         * Конструктор - создаёт пул с заданным размером блока.
         * 
         * @param blockSize Размер одного блока в байтах (должен быть >= sizeof(void*))
         * @param blocksPerChunk Количество блоков в одном чанке (чем больше, тем меньше overhead)
         * 
         * Инварианты:
         * - blockSize должен быть достаточным для хранения указателя (для free list)
         * - blocksPerChunk влияет на частоту аллокаций больших кусков памяти
         * 
         * Debug режим (BLIB_DEBUG_ALLOCATOR_ENABLED):
         * - blockSize автоматически увеличивается на DebugAllocator::getDebugOverhead()
         * - Это происходит прозрачно — пользователь передаёт обычный размер объекта
         * - Например: PoolAllocator(64, 128) → внутренний blockSize = 64 + 40 = 104
         * 
         * Рекомендации:
         * - blockSize: размер типичного объекта (например, sizeof(Entity))
         * - blocksPerChunk: 64-256 для balance между памятью и overhead
         */
        PoolAllocatorImpl(size_t blockSize, size_t blocksPerChunk = 128);

        /**
         * Деструктор - освобождает все chunks.
         */
        ~PoolAllocatorImpl();

        // Запрет копирования (stateful - имеет владение chunks)
        PoolAllocatorImpl(const PoolAllocatorImpl&) = delete;
        PoolAllocatorImpl& operator=(const PoolAllocatorImpl&) = delete;

        // Перемещение разрешено
        PoolAllocatorImpl(PoolAllocatorImpl&& other) noexcept;
        PoolAllocatorImpl& operator=(PoolAllocatorImpl&& other) noexcept;

        /**
         * Выделить блок памяти размером blockSize.
         * 
         * @param size Размер блока (должен быть == blockSize)
         * @return Указатель на блок или nullptr при ошибке
         * 
         * Алгоритм:
         * 1. Если free list не пуст - берём первый блок (O(1))
         * 2. Иначе выделяем новый chunk и берём из него (O(1) amortized)
         * 
         * Важно:
         * - size ДОЛЖЕН быть равен blockSize (иначе вернёт nullptr)
         * - Не thread-safe
         * 
         * Сложность: O(1) amortized
         */
        void* allocate(size_t size);

        /**
         * Освободить ранее выделенный блок.
         * 
         * @param ptr Указатель на блок (должен быть из этого пула)
         * @param size Размер блока (должен быть == blockSize)
         * 
         * Алгоритм:
         * - Добавляет блок в начало free list (O(1))
         * - НЕ освобождает chunk даже если он полностью свободен
         * 
         * Важно:
         * - ptr должен быть получен из allocate этого же пула
         * - size должен быть == blockSize
         * - Не проверяет валидность ptr (UB если указатель некорректен)
         * - Не thread-safe
         * 
         * Сложность: O(1)
         */
        void deallocate(void* ptr, size_t size);

        /**
         * Получить размер блока пула.
         * @return Размер блока в байтах
         */
        size_t getBlockSize() const { return blockSize; }

        /**
         * Получить общее количество блоков (выделенных chunks * blocksPerChunk).
         * @return Общее количество блоков
         */
        size_t getTotalBlocks() const { return chunks.size() * blocksPerChunk; }

        /**
         * Получить примерное количество свободных блоков (не точное, требует обхода free list).
         * TODO: Добавить точный подсчёт с кешированием.
         * @return Примерное количество свободных блоков
         */
        size_t getApproximateFreeBlocks() const;

    private:
        /**
         * FreeBlock - узел intrusive связного списка свободных блоков.
         * Хранится прямо в памяти блока (не требует дополнительной памяти).
         */
        struct FreeBlock
        {
            FreeBlock* next; // Указатель на следующий свободный блок
        };

        /**
         * Выделить новый chunk и добавить все его блоки в free list.
         * @return true если успешно, false при ошибке аллокации
         */
        bool allocateNewChunk();

        size_t blockSize;          // Размер одного блока в байтах
        size_t blocksPerChunk;     // Количество блоков в одном чанке
        
        std::vector<void*> chunks; // Выделенные чанки памяти (для освобождения в деструкторе)
        FreeBlock* freeList;       // Голова связного списка свободных блоков
    };

    /**
     * Специализация AllocatorTraits для PoolAllocatorImpl.
     * Помечаем как stateful - имеет внутреннее состояние (chunks, free list).
     */
    template<>
    struct AllocatorTraits<PoolAllocatorImpl>
    {
        static constexpr bool isStateless = false;
    };

    /**
     * PoolAllocator - публичный typedef pool аллокатора.
     * 
     * Поведение зависит от build mode:
     * 
     * Debug builds (BLIB_DEBUG_ALLOCATOR_ENABLED):
     *   - Автоматически оборачивается в DebugAllocator<PoolAllocatorImpl>
     *   - Guard bytes для overflow/underflow detection
     *   - Poison memory для use-after-free detection
     *   - Double-free detection
     *   - Overhead: +40 байт на каждую аллокацию (вычисляется через getDebugOverhead())
     *   - blockSize автоматически увеличивается в конструкторе для размещения метаданных
     *   - Пользователь продолжает использовать обычный размер объекта
     * 
     * Release builds:
     *   - Прямой PoolAllocatorImpl
     *   - Минимальный overhead (только intrusive free list)
     *   - O(1) allocate/deallocate
     * 
     * Использование:
     *   PoolAllocator pool(64, 128);  // blockSize=64, blocksPerChunk=128
     *   void* ptr = pool.allocate(64);
     *   pool.deallocate(ptr, 64);
     * 
     * Характеристики:
     * - Stateful в обоих режимах (хранит chunks и free list)
     * - Debug: Дополнительная валидация каждой аллокации
     * - Release: Максимальная производительность
     */
#ifdef BLIB_DEBUG_ALLOCATOR_ENABLED
    using PoolAllocator = DebugAllocator<PoolAllocatorImpl>;
#else
    using PoolAllocator = PoolAllocatorImpl;
#endif

    /**
     * Специализация AllocatorTraits для PoolAllocator в debug режиме.
     * В обоих случаях stateful, но в debug дополнительный wrapper.
     */
#ifdef BLIB_DEBUG_ALLOCATOR_ENABLED
    template<>
    struct AllocatorTraits<PoolAllocator>
    {
        static constexpr bool isStateless = false;
    };
#endif

} // namespace memory
} // namespace blib

