// debugAllocator.inl - реализация template методов DebugAllocator
// Этот файл включается в конец debugAllocator.h

#pragma once

#include <cstdlib>
#include <cstdio>

namespace blib
{
namespace memory
{
    template<typename UnderlyingAllocator>
    void* DebugAllocator<UnderlyingAllocator>::allocate(size_t size)
    {
        if (size == 0)
        {
            return nullptr;
        }

        // Вычисляем полный размер с overhead
        size_t totalSize = sizeof(Header) + GUARD_SIZE + size + GUARD_SIZE;

        // Выделяем память через underlying аллокатор
        void* rawMemory = underlying.allocate(totalSize);
        
        if (!rawMemory)
        {
            // Аллокация не удалась
            return nullptr;
        }

        // Инициализируем header
        Header* header = static_cast<Header*>(rawMemory);
        header->size = size;
        header->magic = MAGIC_ALLOCATED;
        header->padding = 0;
        header->checksum = header->calculateChecksum();

        // Устанавливаем front guard
        unsigned char* ptr = static_cast<unsigned char*>(rawMemory);
        ptr += sizeof(Header);
        buint64* frontGuard = reinterpret_cast<buint64*>(ptr);
        *frontGuard = GUARD_PATTERN;

        // Указатель на пользовательскую область
        ptr += GUARD_SIZE;
        void* userPtr = ptr;

        // Устанавливаем back guard
        ptr += size;
        buint64* backGuard = reinterpret_cast<buint64*>(ptr);
        *backGuard = GUARD_PATTERN;

        return userPtr;
    }

    template<typename UnderlyingAllocator>
    void DebugAllocator<UnderlyingAllocator>::deallocate(void* ptr, size_t size)
    {
        if (!ptr)
        {
            // nullptr можно безопасно игнорировать
            return;
        }

        // Валидируем блок перед освобождением
        if (!validateBlock(ptr, size))
        {
            // Ошибка детектирована в validateBlock (он вызовет handleError)
            return;
        }

        // Получаем header
        Header* header = getHeader(ptr);

        // Проверяем что блок не был уже освобождён (double-free detection)
        if (header->magic == MAGIC_FREED)
        {
            handleError("DebugAllocator: Double-free detected!");
            return;
        }

        // Заполняем пользовательскую область poison pattern
        poisonMemory(ptr, size);

        // Маркируем блок как freed
        header->magic = MAGIC_FREED;
        header->checksum = header->calculateChecksum();

        // Вычисляем полный размер и освобождаем через underlying
        size_t totalSize = sizeof(Header) + GUARD_SIZE + size + GUARD_SIZE;
        underlying.deallocate(header, totalSize);
    }

    template<typename UnderlyingAllocator>
    bool DebugAllocator<UnderlyingAllocator>::validateBlock(void* userPtr, size_t expectedSize)
    {
        if (!userPtr)
        {
            handleError("DebugAllocator: Null pointer passed to deallocate!");
            return false;
        }

        // Получаем header
        Header* header = getHeader(userPtr);

        // Проверяем magic number
        if (header->magic != MAGIC_ALLOCATED)
        {
            if (header->magic == MAGIC_FREED)
            {
                handleError("DebugAllocator: Use-after-free or double-free detected!");
            }
            else
            {
                handleError("DebugAllocator: Invalid pointer (corrupted or not allocated by DebugAllocator)!");
            }
            return false;
        }

        // Проверяем checksum header
        buint64 expectedChecksum = header->calculateChecksum();
        if (header->checksum != expectedChecksum)
        {
            handleError("DebugAllocator: Header corruption detected!");
            return false;
        }

        // Проверяем размер
        if (header->size != expectedSize)
        {
            handleError("DebugAllocator: Size mismatch! Allocated size differs from deallocate size!");
            return false;
        }

        // Проверяем front guard
        buint64* frontGuard = getFrontGuard(userPtr);
        if (*frontGuard != GUARD_PATTERN)
        {
            handleError("DebugAllocator: Buffer underflow detected! Front guard bytes corrupted!");
            return false;
        }

        // Проверяем back guard
        buint64* backGuard = getBackGuard(userPtr, header->size);
        if (*backGuard != GUARD_PATTERN)
        {
            handleError("DebugAllocator: Buffer overflow detected! Back guard bytes corrupted!");
            return false;
        }

        // Всё в порядке
        return true;
    }

    template<typename UnderlyingAllocator>
    void DebugAllocator<UnderlyingAllocator>::handleError(const char* message)
    {
        // Выводим сообщение об ошибке
        std::fprintf(stderr, "*** MEMORY ERROR DETECTED ***\n");
        std::fprintf(stderr, "%s\n", message);
        std::fprintf(stderr, "Aborting...\n");
        std::fflush(stderr);

        // Прерываем выполнение
        // Используем access violation вместо abort() чтобы SEH мог перехватить в тестах
        // TODO: В будущем можно сделать configurable (exception, log only, etc.)
        #ifdef _MSC_VER
            // Генерируем access violation для перехвата через __try/__except
            // Это позволяет тестам с BLIB_TEST_EXPECT_ABORT работать корректно
            volatile int* null_ptr = nullptr;
            *null_ptr = 0; // Access violation
        #else
            std::abort();
        #endif
    }

} // namespace memory
} // namespace blib
