#pragma once

#include <cstddef>
#include <cstdint>

#include <blib/align.h>
#include <blib/system/memory/allocatorTraits.h>
#include <blib/system/memory/globalAllocator.h>

namespace blib
{
namespace memory
{
    /**
     * AlignedAllocatorImpl - stateless аллокатор с гарантированным выравниванием.
     * 
     * Назначение:
     * - Выделение блоков, выровненных по произвольной границе (степень двойки)
     * - Вся память идёт ЧЕРЕЗ GlobalAllocator (статистика, leak tracking работают)
     * - Кроссплатформенная замена legacy `blib::core::AlignedAllocator`
     *   (тот жил на _aligned_malloc - MSVC-only, в обход GlobalAllocator)
     * - Stateless - не имеет состояния, все экземпляры идентичны
     * 
     * Архитектура блока памяти (overallocation + ручное выравнивание):
     * ┌────────────────────────────────────────────────────────┐
     * │ GlobalAllocator-блок (size + Alignment + sizeof(Header))│
     * │ ... паддинг ...                                        │
     * │ Header (16 байт): originalPtr, originalSize            │
     * ├────────────────────────────────────────────────────────┤
     * │ User Data (size байт, выровнено по Alignment)          │ ← возвращается пользователю
     * └────────────────────────────────────────────────────────┘
     * Header лежит СРАЗУ ПЕРЕД выровненным адресом - deallocate()
     * восстанавливает исходный указатель и точный исходный размер
     * (GlobalAllocator::deallocate требует размер аллокации для статистики).
     * 
     * Использование:
     *   AlignedAllocator<64> alloc;
     *   void* ptr = alloc.allocate(100); // адрес кратен 64
     *   alloc.deallocate(ptr, 100);
     * 
     * Характеристики:
     * - Stateless, thread-safe (через GlobalAllocator)
     * - Overhead: sizeof(Header) + (Alignment - 1) байт на аллокацию в худшем случае
     * - НЕ оборачивается в DebugAllocator сознательно: его front-guard сдвигает
     *   пользовательский адрес на 32 байта и ломает гарантию выравнивания
     * - Требования к Alignment: степень двойки, не меньше alignof(Header)
     * 
     * @tparam Alignment Выравнивание в байтах (например, __blib_cache_size = 64)
     */
    template<size_t Alignment>
    class AlignedAllocatorImpl
    {
    private:
        /**
         * Header блока - служебная информация для deallocate.
         * Лежит сразу перед выровненным пользовательским адресом.
         */
        struct Header
        {
            void* originalPtr;   // Исходный указатель GlobalAllocator
            size_t originalSize; // Исходный размер аллокации (totalSize)
        };

    public:
        // Header объявлен выше: alignof(Header) видим в compile-time
        static_assert(Alignment >= alignof(Header), "Alignment must be at least alignof(Header)");
        static_assert((Alignment & (Alignment - 1)) == 0, "Alignment must be a power of two");

        /**
         * Выделить блок памяти, выровненный по Alignment.
         * 
         * @param size Размер пользовательских данных в байтах (должен быть > 0)
         * @return Указатель на выровненный блок или nullptr при ошибке
         * 
         * Алгоритм:
         * 1. Запрашиваем у GlobalAllocator size + Alignment + sizeof(Header) байт
         *    (запас Alignment гарантирует место для Header + выравнивание вверх)
         * 2. Выравниваем адрес (raw + sizeof(Header)) вверх до кратного Alignment
         * 3. Записываем Header {originalPtr, originalSize} перед выровненным адресом
         */
        void* allocate(size_t size)
        {
            const size_t totalSize = size + Alignment + sizeof(Header);
            void* raw = GlobalAllocator::instance().allocate(totalSize);
            if (!raw)
            {
                return nullptr;
            }

            // Выравнивание вверх: (x + Alignment - 1) & ~(Alignment - 1),
            // корректно только для степеней двойки (проверено static_assert выше)
            const uintptr_t alignedAddress =
                (reinterpret_cast<uintptr_t>(raw) + sizeof(Header) + Alignment - 1) &
                ~(static_cast<uintptr_t>(Alignment) - 1);

            void* aligned = reinterpret_cast<void*>(alignedAddress);

            // Header лежит сразу перед пользовательским блоком и гарантированно
            // внутри GlobalAllocator-блока (добавили sizeof(Header) до выравнивания)
            Header* header = getHeader(aligned);
            header->originalPtr = raw;
            header->originalSize = totalSize;

            return aligned;
        }

        /**
         * Освободить блок, ранее выделенный через allocate().
         * 
         * @param ptr Указатель на пользовательскую область (из allocate), nullptr игнорируется
         * @param size Размер пользовательских данных (для единообразия интерфейса;
         *              фактический размер блока хранится в Header)
         * 
         * Восстанавливает исходный указатель и размер из Header и возвращает
         * блок GlobalAllocator'у с ТОЧНЫМ исходным размером аллокации.
         */
        void deallocate(_In void* ptr, size_t size)
        {
            (void)size;
            if (!ptr)
            {
                return;
            }

            Header* header = getHeader(ptr);
            GlobalAllocator::instance().deallocate(header->originalPtr, header->originalSize);
        }

    private:
        /**
         * Получить Header из пользовательского указателя.
         * 
         * @param ptr Выровненный указатель, выданный allocate()
         * @return Указатель на Header (сразу перед ptr)
         */
        static Header* getHeader(_In void* ptr)
        {
            return reinterpret_cast<Header*>(static_cast<char*>(ptr) - sizeof(Header));
        }
    };

    /**
     * AlignedAllocator - публичный alias stateless аллокатора с выравниванием.
     * 
     * Использование:
     *   AlignedAllocator<64> alloc;
     *   void* ptr = alloc.allocate(1024);
     *   alloc.deallocate(ptr, 1024);
     * 
     * Подходит для передачи в blib::memory::Allocator (type-erased):
     *   Allocator erased(AlignedAllocator<64>{});
     */
    template<size_t Alignment>
    using AlignedAllocator = AlignedAllocatorImpl<Alignment>;

    /**
     * CacheLineAlignedAllocator - аллокатор с выравниванием по кеш-линии процессора.
     * Преемник legacy `blib::CacheAlignedAllocator` (core/alignedAllocator.h).
     * Дефолтный аллокатор SoundBufferTemplate (звуковые сэмплы).
     */
    using CacheLineAlignedAllocator = AlignedAllocatorImpl<__blib_cache_size>;

    /**
     * Специализация AllocatorTraits для AlignedAllocatorImpl.
     * Помечаем как stateless - не имеет состояния.
     */
    template<size_t Alignment>
    struct AllocatorTraits<AlignedAllocatorImpl<Alignment>>
    {
        static constexpr bool isStateless = true;
    };

} // namespace memory
} // namespace blib
