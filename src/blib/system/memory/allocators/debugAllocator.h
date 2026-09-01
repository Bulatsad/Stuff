#pragma once

#include <cstddef>
#include <cstdint>
#include <cstring>

#include <blib/blibint.h>
#include <blib/system/memory/allocatorTraits.h>

namespace blib
{
namespace memory
{
    /**
     * DebugAllocator - аллокатор с инструментами отладки для детекции ошибок работы с памятью.
     * 
     * Назначение:
     * - Детекция buffer overflow через guard bytes (канарейки)
     * - Детекция use-after-free через poison memory
     * - Детекция double-free через tracking выделенных блоков
     * - Помощь в отладке memory corruption проблем
     * 
     * Архитектура блока памяти:
     * ┌──────────────────────────────────────────────────────────┐
     * │ Header (24 bytes)                                        │
     * │  - size (8 bytes)                                        │
     * │  - magic (4 bytes) - для валидации                       │
     * │  - padding (4 bytes)                                     │
     * │  - checksum (8 bytes) - для детекции corruption          │
     * ├──────────────────────────────────────────────────────────┤
     * │ Front Guard (8 bytes) - 0xDEADBEEFDEADBEEF               │
     * ├──────────────────────────────────────────────────────────┤
     * │ User Data (size bytes)                                   │ ← возвращается пользователю
     * ├──────────────────────────────────────────────────────────┤
     * │ Back Guard (8 bytes) - 0xDEADBEEFDEADBEEF                │
     * └──────────────────────────────────────────────────────────┘
     * 
     * Использование:
     *   DebugAllocator debugAlloc(underlyingAlloc);
     *   void* ptr = debugAlloc.allocate(1024);
     *   // ... использование памяти ...
     *   debugAlloc.deallocate(ptr, 1024); // проверит guard bytes
     * 
     * Характеристики:
     * - Stateful (оборачивает другой аллокатор)
     * - НЕ thread-safe (требуется внешняя синхронизация)
     * - Overhead: 24 (header) + 8 (front guard) + 8 (back guard) = 40 байт на аллокацию
     * - Производительность: значительно медленнее из-за проверок (только для debug!)
     * 
     * Детектируемые ошибки:
     * 1. Buffer overflow - перезапись guard bytes
     * 2. Buffer underflow - перезапись front guard
     * 3. Use-after-free - чтение/запись после deallocate (poison pattern)
     * 4. Double-free - повторный deallocate уже освобождённого блока
     * 5. Invalid pointer - deallocate неправильного указателя
     * 6. Size mismatch - deallocate с неправильным размером
     * 
     * ВАЖНО:
     * - Использовать ТОЛЬКО в debug сборках!
     * - В release заменить на обычный аллокатор для производительности
     * - Ошибки детектируются через assert/abort (см. handleError)
     * 
     * TODO (будущие улучшения):
     * - Stack traces для каждой аллокации (интеграция с debug info)
     * - Списки живых аллокаций для leak detection
     * - Configurable error handling (exception vs abort vs log)
     * - Статистика детектированных ошибок
     */
    template<typename UnderlyingAllocator>
    class DebugAllocator
    {
    public:
        /**
         * Получить размер debug overhead (header + guard bytes).
         * 
         * Этот метод используется stateful аллокаторами (например, PoolAllocator)
         * для автоматической корректировки размера блока в debug режиме.
         * 
         * @return Размер overhead в байтах (header + 2 * guard = 24 + 8 + 8 = 40)
         */
        static constexpr size_t getDebugOverhead()
        {
            return sizeof(Header) + GUARD_SIZE * 2;
        }

        /**
         * Конструктор по умолчанию для stateless аллокаторов.
         * Создаёт underlying аллокатор через default конструктор.
         */
        DebugAllocator()
            : underlying()
        {
        }

        /**
         * Конструктор от underlying аллокатора.
         * 
         * @param underlying Базовый аллокатор для реальных аллокаций
         */
        explicit DebugAllocator(UnderlyingAllocator&& underlying)
            : underlying(std::move(underlying))
        {
        }

        /**
         * Perfect forwarding конструктор для stateful аллокаторов.
         * Передаёт аргументы в конструктор underlying аллокатора.
         * 
         * @param args Аргументы для конструктора underlying аллокатора
         */
        template<typename... Args>
        explicit DebugAllocator(Args&&... args)
            : underlying(std::forward<Args>(args)...)
        {
        }

        /**
         * Выделить блок памяти с debug обёрткой.
         * 
         * @param size Размер пользовательских данных в байтах
         * @return Указатель на пользовательскую область или nullptr при ошибке
         * 
         * Алгоритм:
         * 1. Выделяем дополнительную память для header + guards
         * 2. Инициализируем header (размер, magic, checksum)
         * 3. Устанавливаем guard bytes (front и back)
         * 4. Возвращаем указатель на пользовательскую область
         * 
         * Overhead: sizeof(Header) + GUARD_SIZE * 2 = 40 байт
         */
        void* allocate(size_t size);

        /**
         * Освободить блок памяти с проверкой корректности.
         * 
         * @param ptr Указатель на пользовательскую область
         * @param size Размер пользовательских данных (должен совпадать с allocate)
         * 
         * Проверки:
         * 1. Валидность указателя (magic число в header)
         * 2. Совпадение размера с allocate
         * 3. Целостность front guard bytes
         * 4. Целостность back guard bytes
         * 5. Checksum header'а
         * 6. Не был ли уже освобождён (double-free detection)
         * 
         * После проверок:
         * - Заполняет пользовательскую область poison pattern (0xDEADC0DE)
         * - Маркирует header как freed (для детекции double-free)
         * - Освобождает underlying память
         */
        void deallocate(void* ptr, size_t size);

    private:
        // Magic numbers для детекции
        static constexpr buint32 MAGIC_ALLOCATED = 0xABADBABE; // Живой блок
        static constexpr buint32 MAGIC_FREED = 0xDEADDEAD;     // Освобождённый блок
        static constexpr buint64 GUARD_PATTERN = 0xDEADBEEFDEADBEEFULL; // Guard bytes
        static constexpr buint32 POISON_PATTERN = 0xDEADC0DE; // Poison для freed памяти

        static constexpr size_t GUARD_SIZE = sizeof(buint64); // 8 байт

        /**
         * Header блока памяти.
         * Хранит метаинформацию для валидации и детекции ошибок.
         */
        struct Header
        {
            size_t size;        // Размер пользовательских данных
            buint32 magic;      // Magic число (MAGIC_ALLOCATED или MAGIC_FREED)
            buint32 padding;    // Выравнивание до 8 байт
            buint64 checksum;   // Checksum header для детекции corruption

            // Вычислить checksum header'а
            buint64 calculateChecksum() const
            {
                // Простой checksum: XOR всех полей кроме самого checksum
                return static_cast<buint64>(size) ^ 
                       static_cast<buint64>(magic) ^
                       static_cast<buint64>(padding);
            }
        };

        /**
         * Получить указатель на header из пользовательского указателя.
         */
        static Header* getHeader(void* userPtr)
        {
            if (!userPtr)
            {
                return nullptr;
            }
            
            // Header находится перед front guard
            unsigned char* ptr = static_cast<unsigned char*>(userPtr);
            ptr -= GUARD_SIZE; // Пропускаем front guard
            ptr -= sizeof(Header); // Пропускаем header
            
            return reinterpret_cast<Header*>(ptr);
        }

        /**
         * Получить указатель на front guard.
         */
        static buint64* getFrontGuard(void* userPtr)
        {
            if (!userPtr)
            {
                return nullptr;
            }
            
            unsigned char* ptr = static_cast<unsigned char*>(userPtr);
            ptr -= GUARD_SIZE;
            
            return reinterpret_cast<buint64*>(ptr);
        }

        /**
         * Получить указатель на back guard.
         */
        static buint64* getBackGuard(void* userPtr, size_t size)
        {
            if (!userPtr)
            {
                return nullptr;
            }
            
            unsigned char* ptr = static_cast<unsigned char*>(userPtr);
            ptr += size; // Переходим за пользовательские данные
            
            return reinterpret_cast<buint64*>(ptr);
        }

        /**
         * Проверить валидность блока и его guards.
         * 
         * @param userPtr Указатель на пользовательскую область
         * @param expectedSize Ожидаемый размер
         * @return true если блок валидный, false при ошибке
         */
        bool validateBlock(void* userPtr, size_t expectedSize);

        /**
         * Заполнить память poison pattern.
         */
        static void poisonMemory(void* ptr, size_t size)
        {
            buint32* data = static_cast<buint32*>(ptr);
            size_t count = size / sizeof(buint32);
            
            for (size_t i = 0; i < count; ++i)
            {
                data[i] = POISON_PATTERN;
            }
            
            // Остаток байтов
            size_t remainder = size % sizeof(buint32);
            if (remainder > 0)
            {
                unsigned char* remainderPtr = reinterpret_cast<unsigned char*>(data + count);
                for (size_t i = 0; i < remainder; ++i)
                {
                    remainderPtr[i] = static_cast<unsigned char>(POISON_PATTERN >> (i * 8));
                }
            }
        }

        /**
         * Обработать ошибку детекции.
         * В текущей реализации просто вызывает abort.
         * TODO: Configurable error handling.
         */
        static void handleError(const char* message);

        UnderlyingAllocator underlying; // Базовый аллокатор
    };

    /**
     * Специализация AllocatorTraits для DebugAllocator.
     * Stateful - хранит underlying аллокатор.
     */
    template<typename UnderlyingAllocator>
    struct AllocatorTraits<DebugAllocator<UnderlyingAllocator>>
    {
        static constexpr bool isStateless = false;
    };

} // namespace memory
} // namespace blib

// Включаем реализацию template методов
#include <blib/system/memory/impl/debugAllocator.inl>
