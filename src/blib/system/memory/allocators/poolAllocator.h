#pragma once

#include <cstddef>
#include <cstdint>
#include <iterator>
#include <type_traits>
#include <vector>

#include <blib/blibint.h>
#include <blib/utilmacro.h>
#include <blib/system/memory/allocator.h>
#include <blib/system/memory/allocatorTraits.h>
#include <blib/system/memory/globalAllocator.h>
#include <blib/system/memory/stdAllocatorAdapter.h>

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
     * PoolAllocatorImpl - внутренняя реализация аллокатора с пулами фиксированного размера.
     * 
     * Назначение:
     * - Быстрое выделение/освобождение блоков фиксированного размера
     * - Минимизация фрагментации памяти
     * - Идеален для частых аллокаций объектов одного размера (entity pooling, particle systems)
     * 
     * Архитектура:
     * ┌─────────────────────────────────────────────┐
     * │ Chunk (bitmap + blockSize * blocksPerChunk) │
     * │  [Bitmap: 1 бит на блок][Block][Block]...   │
     * ├─────────────────────────────────────────────┤
     * │ Chunk 2                                     │
     * │  [Bitmap][Block][Block]...[Block]           │
     * └─────────────────────────────────────────────┘
     *   Free list (для O(1) allocate): Block₃ → Block₇ → Block₁₂ → ...
     *   Bitmap (для O(1) проверки "блок свободен?" в итераторе):
     *   бит = 1 → свободен, 0 → занят
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
     * - Stateful (хранит chunks, free list и битмапы внутри чанков)
     * - Не thread-safe (требуется внешняя синхронизация)
     * - allocate/deallocate: O(1 + C), C - число чанков (поиск чанка скан с конца)
     * - Аллокации разных размеров не поддерживаются (только blockSize)
     * 
     * Оптимизации:
     * - Free list — связный список свободных блоков (intrusive, для O(1) allocate)
     * - Инлайн-битмап "свободен/занят" в начале каждого чанка (для O(1) isFree
     *   в итераторе): ceil(blocksPerChunk / 64) слов buint64 на чанк
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
     * Итерация:
     * - begin()/end() (+ const/cbegin/cend) — итерация по ЗАНЯТЫМ блокам,
     *   свободные пропускаются; пустой/полностью свободный пул даёт begin() == end()
     * - Итераторы инвалидируются allocate() (возможен push_back в chunks),
     *   деструктором и перемещением пула; инкремент end() — UB
     * - Стоимость итерации O(N) (битовый тест за O(1))
     * 
     * Защита deallocate (release-диагностика в stderr, см. SYSTEM.md):
     * - Чужой указатель и double-free — warning + no-op (раньше было UB)
     * 
     * TODO (будущие улучшения):
     * - Поддержка кастомного alignment
     * - Shrink механизм (освобождение пустых chunks)
     * - Thread-safe версия с per-thread pools
     * - Статистика (используемые/свободные блоки)
     * - Конвертация голых size_t в buint64 (правило типов; затронет сигнатуры
     *   публичных методов и вызовы из ComponentPool — отдельный рефакторинг)
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
         *   (по умолчанию — defaultBlocksPerChunk)
         */

        // Дефолтное количество блоков в чанке (баланс память/overhead).
        // Вшитые литералы запрещены, поэтому дефолтный аргумент конструктора
        // ссылается на эту именованную константу.
        static constexpr size_t defaultBlocksPerChunk = 128;

        PoolAllocatorImpl(size_t blockSize, size_t blocksPerChunk = defaultBlocksPerChunk);

        /**
         * IteratorBase<PoolT> - forward-итератор по занятым блокам пула.
         * 
         * PoolT = PoolAllocatorImpl        → Iterator (возвращает void*)
         * PoolT = const PoolAllocatorImpl  → ConstIterator (возвращает const void*)
         * 
         * Семантика:
         * - Итерирует ТОЛЬКО занятые (allocated) блоки, свободные пропускаются
         *   (isFree() - O(1) проверка бита в инлайн-битмапе чанка)
         * - Порядок: чанки по порядку выделения, внутри чанка по возрастанию адресов
         * - Пустой или полностью свободный пул: begin() == end()
         * 
         * Инвалидация (как у контейнеров):
         * - allocate() может добавить чанк (push_back в chunks) - живые
         *   итераторы инвалидируются (end() пересчитывать заново)
         * - Деструктор и перемещение пула оставляют итераторы висячими (palloc)
         * - Инкремент на end() - UB (стандартная конвенция итераторов)
         * 
         * Стоимость: полная итерация O(N) (битовая проверка за O(1)).
         */
        template<typename PoolT>
        class IteratorBase
        {
        public:
            // std-совместимые typedef'ы (по образцу LinkedList::Iterator).
            // value_type намеренно НЕ const: итератор пула бестиповый, выдаёт
            // сырые адреса блоков; const-ность выражается через pointer/reference.
            typedef std::forward_iterator_tag iterator_category;
            typedef void* value_type;
            typedef std::ptrdiff_t difference_type;
            typedef std::conditional_t<std::is_const_v<PoolT>, const void*, void*> pointer;
            typedef std::conditional_t<std::is_const_v<PoolT>, const void*, void*> reference;

            enum class IteratorOrigin {
                Begin,
                End
            };

            IteratorBase(_In PoolT* a_palloc, IteratorOrigin itOrigin)
                : palloc(a_palloc)
                , chunkI(0)
                , blockJ(0)
            {
                if (itOrigin == IteratorOrigin::Begin)
                {
                    // Нормализация: free list не упорядочен, поэтому первый
                    // блок пула может быть свободным - сразу продвигаемся
                    // к первому занятому (полностью свободный пул даёт end()).
                    this->skipFree();
                }
                else if (itOrigin == IteratorOrigin::End)
                {
                    this->chunkI = this->palloc->chunks.size();
                }
            }

            reference operator*() const
            {
                return this->getIterAddr();
            }
            pointer operator->() const
            {
                return this->getIterAddr();
            }

            IteratorBase& operator++()
            {
                this->next();
                this->skipFree();
                return *this;
            }
            IteratorBase operator++(int)
            {
                IteratorBase tmp(*this);
                ++(*this);
                return tmp;
            }

            bool operator==(const IteratorBase& rhs) const
            {
                return (this->palloc == rhs.palloc) && (this->chunkI == rhs.chunkI) && (this->blockJ == rhs.blockJ);
            }
            bool operator!=(const IteratorBase& rhs) const
            {
                return !(*this == rhs);
            }

        private:
            PoolT* palloc = nullptr;
            size_t chunkI = 0;
            size_t blockJ = 0;

            // Позиция end: chunkI указывает за последний чанк (blockJ сброшен в 0).
            bool isAtEnd() const
            {
                return this->chunkI == this->palloc->chunks.size();
            }

            // Переход к следующему блоку с переносом через границу чанка.
            void next()
            {
                this->blockJ++;
                if (this->blockJ == this->palloc->blocksPerChunk)
                {
                    this->chunkI++;
                    this->blockJ = 0;
                }
            }

            // Пропуск свободных блоков. Проверка isAtEnd() ИДЁТ ДО isFree():
            // next() может увести за последний чанк, а getIterAddr() тогда
            // читает chunks[size] (out-of-bounds).
            void skipFree()
            {
                while (!this->isAtEnd() && this->isFree())
                {
                    this->next();
                }
            }

            // Адрес текущего блока: chunks[chunkI] + bitmapBytes + blockSize * blockJ
            // (битмап лежит инлайн в начале буфера чанка). Для const-пула
            // возвращает const void* (conditional_t).
            pointer getIterAddr() const
            {
                typedef std::conditional_t<std::is_const_v<PoolT>, const char*, char*> CharPtr;
                return static_cast<pointer>(static_cast<CharPtr>(this->palloc->chunks[this->chunkI])
                    + this->palloc->getBitmapBytes()
                    + (this->palloc->blockSize * this->blockJ));
            }

            // Свободен ли текущий блок: O(1) проверка бита в инлайн-битмапе
            // чанка (1 = свободен). Бит индексируется по blockJ.
            bool isFree() const
            {
                const buint64* bitmap = static_cast<const buint64*>(this->palloc->chunks[this->chunkI]);
                const size_t wordIndex = this->blockJ / PoolAllocatorImpl::bitsPerWord;
                const buint64 bitMask = static_cast<buint64>(1) << (this->blockJ % PoolAllocatorImpl::bitsPerWord);
                return (bitmap[wordIndex] & bitMask) != 0;
            }
        };

        typedef IteratorBase<PoolAllocatorImpl> Iterator;
        typedef IteratorBase<const PoolAllocatorImpl> ConstIterator;

        Iterator begin() noexcept { return Iterator(this, Iterator::IteratorOrigin::Begin); }
        Iterator end() noexcept { return Iterator(this, Iterator::IteratorOrigin::End); }

        ConstIterator begin() const noexcept { return ConstIterator(this, ConstIterator::IteratorOrigin::Begin); }
        ConstIterator end() const noexcept { return ConstIterator(this, ConstIterator::IteratorOrigin::End); }

        ConstIterator cbegin() const noexcept { return ConstIterator(this, ConstIterator::IteratorOrigin::Begin); }
        ConstIterator cend() const noexcept { return ConstIterator(this, ConstIterator::IteratorOrigin::End); }

        /**
         * Деструктор - освобождает все chunks.
         */
        ~PoolAllocatorImpl();

        // Запрет копирования (stateful - имеет владение chunks)
        PoolAllocatorImpl(const PoolAllocatorImpl&) = delete;
        PoolAllocatorImpl& operator=(const PoolAllocatorImpl&) = delete;

        // Перемещение разрешено
        PoolAllocatorImpl(_In _Out PoolAllocatorImpl&& other) noexcept;
        PoolAllocatorImpl& operator=(_In _Out PoolAllocatorImpl&& other) noexcept;

        /**
         * Выделить блок памяти размером blockSize.
         * 
         * @param size Размер блока (должен быть == blockSize)
         * @return Указатель на блок или nullptr при ошибке
         * 
         * Алгоритм:
         * 1. Если free list не пуст - берём первый блок (O(1))
         * 2. Иначе выделяем новый chunk и берём из него (O(1) amortized)
         * 3. Снимаем бит "свободен" в битмапе чанка (поиск чанка - O(C))
         * 
         * Важно:
         * - size ДОЛЖЕН быть равен blockSize (иначе вернёт nullptr)
         * - Не thread-safe
         * 
         * Сложность: O(1 + C) amortized, C - число чанков (скан с конца)
         */
        void* allocate(size_t size);

        /**
         * Освободить ранее выделенный блок.
         * 
         * @param ptr Указатель на блок (должен быть из этого пула)
         * @param size Размер блока (должен быть == blockSize)
         * 
         * Алгоритм:
         * - Взводит бит "свободен" в битмапе чанка и добавляет блок
         *   в начало free list
         * - НЕ освобождает chunk даже если он полностью свободен
         * 
         * Защита (в отличие от прежнего UB):
         * - size != blockSize - тихий no-op
         * - ptr не принадлежит ни одному чанку - warning в stderr + no-op
         * - Бит уже взведён (double-free) - warning в stderr + no-op
         *   (в debug это ловит DebugAllocator ещё до нас - abort)
         * 
         * Диагностика идёт в stderr: blib-system не может использовать
         * Console (core выше по слою), см. SYSTEM.md.
         * 
         * Важно:
         * - ptr должен быть получен из allocate этого же пула
         * - Не thread-safe
         * 
         * Сложность: O(C), C - число чанков (поиск чанка сканом с конца)
         */
        void deallocate(_In void* ptr, size_t size);

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

        // Битов в одном слове инлайн-битмапа чанка (слово - buint64).
        static constexpr size_t bitsPerWord = 64;

        // Число buint64-слов битмапа чанка (1 бит на блок, округление вверх).
        size_t getBitmapWords() const
        {
            return (this->blocksPerChunk + bitsPerWord - 1) / bitsPerWord;
        }

        // Байты битмапа, лежащего инлайн в начале буфера чанка.
        // Кратны sizeof(buint64), поэтому блоки после него остаются 8-выровненными.
        size_t getBitmapBytes() const
        {
            return this->getBitmapWords() * sizeof(buint64);
        }

        // Полный размер буфера чанка: битмап + блоки.
        size_t getChunkTotalBytes() const
        {
            return this->getBitmapBytes() + this->blockSize * this->blocksPerChunk;
        }

        /**
         * Найти индекс чанка, которому принадлежит указатель (по диапазону
         * адресов). Скан с конца: блоки LIFO чаще лежат в свежих чанках.
         * @param ptr Указатель (начало блока)
         * @return Индекс чанка или chunks.size(), если не найден
         */
        size_t findChunkIndex(_In const void* ptr) const;

        /**
         * Выделить новый chunk и добавить все его блоки в free list.
         * @return true если успешно, false при ошибке аллокации
         */
        bool allocateNewChunk();

        size_t blockSize;          // Размер одного блока в байтах
        size_t blocksPerChunk;     // Количество блоков в одном чанке

        // Аллокатор служебного контейнера chunks. Объявлен ПЕРЕД контейнером:
        // StdAllocatorAdapter хранит указатель и не владеет аллокатором,
        // поэтому chunkAllocator обязан жить дольше chunks (порядок объявления
        // гарантирует это: разрушение членов идёт в обратном порядке).
        // По умолчанию — DefaultAllocator (прокси к GlobalAllocator).
        blib::memory::Allocator chunkAllocator;

        // Выделенные чанки памяти (для освобождения в деструкторе).
        // STL-контейнер обязан указывать blib-аллокатор (правило проекта).
        std::vector<void*, blib::memory::StdAllocatorAdapter<void*>> chunks{
            blib::memory::StdAllocatorAdapter<void*>(&chunkAllocator) };
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
     *   - Минимальный overhead (только intrusive free list + инлайн-битмап чанка)
     *   - allocate/deallocate O(1 + C), итерация O(N)
     *   - Собственная диагностика: double-free и чужой ptr - warning в stderr + no-op
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

    /**
     * Специализация AllocatorTraits для PoolAllocator в debug режиме.
     * В обоих случаях stateful, но в debug дополнительный wrapper.
     */
    template<>
    struct AllocatorTraits<PoolAllocator>
    {
        static constexpr bool isStateless = false;
    };
#else
    using PoolAllocator = PoolAllocatorImpl;
#endif

} // namespace memory
} // namespace blib

